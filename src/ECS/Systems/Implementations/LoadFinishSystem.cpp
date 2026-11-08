/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "LoadFinishSystem.h"

#include <algorithm>
#include <chrono>
#include <future>
#include <thread>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/TempleInteriorInterface.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureSkin.h"
#include "Debug/DebugGuiInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/DanceSystemInterface.h"
#include "ECS/Systems/GestureSystemInterface.h"
#include "ECS/Systems/HighDetailSystemInterface.h"
#include "ECS/Systems/LoadingScreenSystemInterface.h"
#include "ECS/Systems/MiracleFxSystemInterface.h"
#include "ECS/Systems/ObjectDrawListSystemInterface.h"
#include "ECS/Systems/ParticleRuleSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/RenderingSystemInterface.h"
#include "EngineConfig.h"
#include "Graphics/WarmUpViews.h"
#include "Gui/Canvas.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"
#include "Simulation/WorldStart.h"

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{
/// The renderer does the work of a frame on its own thread while the game makes the next frame. Thus the work that a
/// frame asks for is complete when the frame after it ends.
constexpr uint32_t k_FramesToReleaseGpuWorkMemory = 2;

/// A redraw point inside the loading: the loading screen system draws and shows its screen if it is due, or at once
/// when drawNow is true. Gives true when a frame showed.
bool RenderLoadingFrame(bool drawNow = false, const std::function<void()>& drawUnder = nullptr)
{
	return Locator::loadingScreenSystem::value().RenderLoadingFrame(drawNow, drawUnder);
}
} // namespace

LoadFinishSystem::LoadFinishSystem(LoadFinishHooks hooks)
    : _hooks(std::move(hooks))
{
}

void LoadFinishSystem::FinishBehindScreen()
{
	// A load that stopped before it opened a landscape has no land, so the first frames have nothing to prepare
	if (!Locator::terrainSystem::has_value() || !Locator::terrainSystem::value().IsLoaded())
	{
		return;
	}
	auto& resources = Locator::resources::value();
	// If the game uses a resource before the loading threads load it, the resource loads in the middle of a frame and
	// makes that frame slow. Thus all the resources load before the first frame of the game, as in the original game.
	const auto start = std::chrono::steady_clock::now();
	// The pipelines of the land start in the background while the other resources load
	StartLandPipelines();
	const auto count = resources.LoadingCount();
	while (resources.LoadingCount() > 0 && !_hooks.quitRequested())
	{
		resources.UpdateLoading(simulation::k_LoadingScreenLoadBudget, graphics::UploadPacer::Allowance {});
		RenderLoadingFrame();
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	const auto loaded = std::chrono::steady_clock::now();
	// The first frame draws into targets of the window's size. Thus the warm-up draws into the same targets.
	_hooks.sizeSceneToWindow();
	PrepareFirstFrames();
	WarmUpFirstFrame();
	// The renderer makes the last resources in one more loading frame, so that the first frame of the game does not
	// make them
	RenderLoadingFrame(true);
	// The renderer can keep the memory that it used to send the resources to the GPU for hundreds of command lists.
	// Then one of the first frames of the game releases all of it, which can take a second. Thus the next loading
	// frame waits for the GPU and releases that memory. The frame after it waits for the renderer to complete that.
	if (Locator::rendererInterface::has_value())
	{
		Locator::rendererInterface::value().ReleaseGpuWorkMemory();
	}
	for (uint32_t frame = 0; frame < k_FramesToReleaseGpuWorkMemory; ++frame)
	{
		RenderLoadingFrame(true);
	}
	const auto prepared = std::chrono::steady_clock::now();
	SPDLOG_LOGGER_INFO(spdlog::get("game"),
	                   "Load: {} prefetched resources finished behind the loading screen in {:.1f} ms, then the first "
	                   "frames prepared in {:.1f} ms; {} pipelines made",
	                   count, std::chrono::duration<double, std::milli>(loaded - start).count(),
	                   std::chrono::duration<double, std::milli>(prepared - loaded).count(),
	                   Locator::rendererInterface::has_value() ? Locator::rendererInterface::value().GetPipelinesMade() : 0);
}

void LoadFinishSystem::PrepareFirstFrames()
{
	const auto& config = Locator::config::value();
	// The maps of where creatures can walk and where they may go read only the land. Other threads make them while
	// this thread does the rest. A creature's first move needs the first map and the first fight needs the second.
	std::vector<std::future<void>> landMaps;
	if (Locator::creatureLocomotionSystem::has_value())
	{
		landMaps.push_back(std::async(std::launch::async, [] {
			[[maybe_unused]] const auto& land = Locator::creatureLocomotionSystem::value().GetWalkableLand();
		}));
	}
	if (Locator::creatureFightSystem::has_value())
	{
		landMaps.push_back(std::async(std::launch::async, [] { Locator::creatureFightSystem::value().PrepareLand(); }));
	}
	// The first scene draw bakes the shadows of the whole island into the land, which takes tens of milliseconds
	if (config.drawEntities && Locator::rendererInterface::has_value())
	{
		Locator::rendererInterface::value().BakeLandShadows(config.drawVegetation);
	}
	// The first frame of a creature makes a texture for each of its painted skins. Make enough for the species with the
	// most skins.
	if (Locator::rendererInterface::has_value())
	{
		auto& resources = Locator::resources::value();
		const auto& files = resources.GetL3DFiles();
		size_t skins = 0;
		resources.GetCreatureRigs().Each([&files, &skins](entt::id_type, const auto& rig) {
			if (const auto* file = files.Find(creature_skin::MeshFileId(rig->baseMeshName)); file != nullptr)
			{
				skins = std::max(skins, file->GetSkins().size());
			}
		});
		Locator::rendererInterface::value().PrepareCreatureSkins(skins);
	}
	// The first frame reads the blocks of the land for the list of things to draw
	if (Locator::objectDrawListSystem::has_value())
	{
		Locator::objectDrawListSystem::value().PrepareLand();
	}
	// The first start of each effect reads its file, its models and its light maps
	if (Locator::particleRuleSystem::has_value())
	{
		Locator::particleRuleSystem::value().LoadAllTypes();
	}
	// The first draw of an effect finds the names of the texture sheets
	if (Locator::particleSystem::has_value())
	{
		Locator::particleSystem::value().LoadTextures();
	}
	// The first thing that a script puts in high detail reads its detailed model and its eyes
	if (Locator::highDetailSystem::has_value())
	{
		Locator::highDetailSystem::value().LoadModels();
	}
	// The first frame loads the models of the globes and the gesture templates
	if (Locator::miracleFxSystem::has_value())
	{
		Locator::miracleFxSystem::value().LoadMeshes();
	}
	if (Locator::gestureSystem::has_value())
	{
		Locator::gestureSystem::value().LoadTemplates();
	}
	// The first villager that joins a dance reads the shapes of the dances from the data folder
	if (Locator::danceSystem::has_value())
	{
		[[maybe_unused]] const auto& shapes = Locator::danceSystem::value().Shapes();
	}
	for (auto& map : landMaps)
	{
		map.get();
	}
}

void LoadFinishSystem::EndWithoutScreen()
{
	// The renderer made the land's pipelines and resources before the load, so no frame warms them up. The work of the
	// first frame that depends on the loaded world is done now.
	PrepareFirstFrames();
}

void LoadFinishSystem::WarmUpFirstFrame()
{
	const auto& config = Locator::config::value();
	if (!config.warmUpFirstFrame || !Locator::rendererInterface::has_value() || !Locator::rendereringSystem::has_value() ||
	    !Locator::camera::has_value())
	{
		return;
	}
	auto& renderer = Locator::rendererInterface::value();
	auto& rendering = Locator::rendereringSystem::value();
	const auto start = std::chrono::steady_clock::now();
	// Some renderers divide the warm-up between some frames, each with a part of the draws
	bool shown = false;
	const auto warmUp = [&renderer, &shown](const graphics::RendererInterface::DrawSceneDesc& desc) {
		bool partsLeft = true;
		for (uint32_t part = 0; partsLeft; ++part)
		{
			partsLeft = false;
			const bool partShown = RenderLoadingFrame(true, [&renderer, &desc, &partsLeft, part]() {
				partsLeft = renderer.WarmUpScene(desc, part);
				// The interface draws over the scene too, and the cinema bars are its first draw
				gui::Canvas::WarmUp(graphics::RenderPass::CinemaBars);
				// The villagers' labels and the tip bubble test the depth of the scene, sorted among its see-through
				// things outside the temple
				gui::Canvas::WarmUp(graphics::RenderPass::WorldLabels, gui::Canvas::DepthTest::NearerIsGreater);
				gui::Canvas::WarmUp(graphics::RenderPass::Translucent, gui::Canvas::DepthTest::NearerIsGreater, true);
				// The interface that the game draws over the scene, which the loading screen does not blend in each way
				gui::Canvas::WarmUp(graphics::RenderPass::Interface);
				// The debug menu comes up with the game's menu, over all of the game
				if (Locator::debugGui::has_value())
				{
					Locator::debugGui::value().WarmUp();
				}
			});
			shown = shown || partShown;
		}
	};
	const auto warmUpAll = [&config, &rendering, &warmUp, this]() {
		// The draw lists of the land's things, as the first frame makes them
		if (config.drawEntities)
		{
			rendering.PrepareDraw(config.drawBoundingBoxes, config.drawFootpaths, config.drawStreams,
			                      ecs::systems::DrawUse::WarmUp);
		}
		warmUp(_hooks.sceneDrawDesc(0, _hooks.handDrawn()));
		// The temple's rooms show only while the player is in the temple. A visit draws them with the temple's own
		// pipelines and targets, thus the warm-up draws a visit too. The temple draws no island and no sea.
		if (Locator::temple::has_value())
		{
			Locator::temple::value().WarmUpVisit([&config, &rendering, &warmUp, this]() {
				if (config.drawEntities)
				{
					rendering.PrepareDraw(config.drawBoundingBoxes, config.drawFootpaths, config.drawStreams,
					                      ecs::systems::DrawUse::WarmUp);
				}
				auto desc = _hooks.sceneDrawDesc(0, _hooks.handDrawn());
				desc.drawIsland = false;
				desc.drawWater = false;
				warmUp(desc);
			});
		}
	};
	// Some renderers can make the pipelines of the warm-up in the background, many at the same time. A first pass of
	// the warm-up starts them, and the loading screen shows while the renderer makes them. The second pass then gets
	// the ready pipelines and draws all of the warm-up.
	if (renderer.SetPipelinePrefetch(true))
	{
		warmUpAll();
		renderer.SetPipelinePrefetch(false);
		const auto waitStart = std::chrono::steady_clock::now();
		uint32_t framesAfter = 0;
		const auto waited = [&waitStart]() {
			return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - waitStart);
		};
		while (graphics::warm_up::PrefetchWaits(framesAfter, renderer.GetPipelinePrefetchPending(), waited()) &&
		       !_hooks.quitRequested() && RenderLoadingFrame(true))
		{
			++framesAfter;
			std::this_thread::sleep_for(graphics::warm_up::k_PrefetchFrameInterval);
		}
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "Load: the pipelines of the warm-up made in the background in {} ms, {} not ready", waited().count(),
		                   renderer.GetPipelinePrefetchPending());
	}
	warmUpAll();
	// The first frame makes the draw lists again from what the game's first update changed
	if (config.drawEntities)
	{
		rendering.SetLayoutDirty();
	}
	if (shown)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Load: the first frame drawn behind the loading screen in {:.1f} ms",
		                   std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
	}
}

void LoadFinishSystem::StartLandPipelines()
{
	const auto& config = Locator::config::value();
	if (!config.warmUpFirstFrame || !Locator::rendererInterface::has_value() || !Locator::camera::has_value())
	{
		return;
	}
	auto& renderer = Locator::rendererInterface::value();
	if (!renderer.SetPipelinePrefetch(true))
	{
		return;
	}
	// The pipelines include the formats of the targets, which follow the window's size
	_hooks.sizeSceneToWindow();
	// Only the land, the sky and the sea: the things on the land need resources that are not loaded yet
	auto desc = _hooks.sceneDrawDesc(0, false);
	desc.drawEntities = false;
	desc.drawSprites = false;
	desc.drawVegetation = false;
	desc.drawBoundingBoxes = false;
	RenderLoadingFrame(true, [&renderer, &desc]() { [[maybe_unused]] const bool partsLeft = renderer.WarmUpScene(desc, 0); });
	renderer.SetPipelinePrefetch(false);
}
