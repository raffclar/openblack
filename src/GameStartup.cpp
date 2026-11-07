/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The parts of the start of the game and of a land that the game itself does: the players' profiles, the new player's
// box, the skirmish box and the work behind the loading screen. The loading screen system draws the screens.

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <future>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <SDL.h>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "Common/MachineClock.h"
#include "Common/UserData.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureSkin.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/DanceSystemInterface.h"
#include "ECS/Systems/GestureSystemInterface.h"
#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "ECS/Systems/HighDetailSystemInterface.h"
#include "ECS/Systems/InspectorLoading.h"
#include "ECS/Systems/LoadingScreenSystemInterface.h"
#include "ECS/Systems/MiracleFxSystemInterface.h"
#include "ECS/Systems/ObjectDrawListSystemInterface.h"
#include "ECS/Systems/ParticleRuleSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "ECS/Systems/ProfileFolderSystemInterface.h"
#include "EngineConfig.h"
#include "FileSystem/FileSystemInterface.h"
#include "Game.h"
#include "Graphics/RendererInterface.h"
#include "Gui/GameInterface.h"
#include "Gui/LoadingScreenRules.h"
#include "Gui/MenuBackdrop.h"
#include "Gui/TextDatabase.h"
#include "Input/WindowPointer.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack;

namespace
{
/// The renderer does the work of a frame on its own thread while the game makes the next frame. Thus the work that a
/// frame asks for is complete when the frame after it ends.
constexpr uint32_t k_FramesToReleaseGpuWorkMemory = 2;

/// Milliseconds since the program started, as the game counts frames with
uint32_t Milliseconds()
{
	return SDL_GetTicks();
}
} // namespace

bool Game::HasPlayerProfiles() noexcept
{
	return Locator::playerProfileSystem::has_value() && Locator::playerProfileSystem::value().GetProfileCount() > 0;
}

player_profiles::Store Game::ProfileStore() const
{
	auto store = player_profiles::StoreFor(
	    {.profileFolder = _profileFolder, .userFolder = user_data::ProfileFolder(), .automated = _automatedRun});
	if (store.developer)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "A developer's run: playing as the profile \"{}\", its creature kept in {}",
		                   player_profiles::k_DeveloperName,
		                   store.creatureMinds.has_value() ? store.creatureMinds->generic_string() : "no folder");
	}
	else if (store.folder.has_value())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The players' profiles are kept in {}", store.folder->generic_string());
	}
	return store;
}

std::optional<std::u16string> Game::PlayerForProfileFiles() const
{
	if (_developerProfile || !Locator::playerProfileSystem::has_value())
	{
		return std::nullopt;
	}
	const auto profile = Locator::playerProfileSystem::value().GetCurrentProfile();
	if (!profile.has_value())
	{
		return std::nullopt;
	}
	return gui::ToUtf16(profile->name);
}

void Game::LoadHelpRecord()
{
	const auto player = PlayerForProfileFiles();
	if (!player.has_value() || !Locator::profileFolderSystem::has_value() || !Locator::helpProfileSystem::has_value())
	{
		return;
	}
	Locator::helpProfileSystem::value().LoadRecord(Locator::profileFolderSystem::value(), *player);
}

void Game::SaveHelpRecord()
{
	const auto player = PlayerForProfileFiles();
	if (!player.has_value() || !Locator::profileFolderSystem::has_value() || !Locator::helpProfileSystem::has_value())
	{
		return;
	}
	Locator::helpProfileSystem::value().SaveRecord(Locator::profileFolderSystem::value(), *player);
}

bool Game::AskForNewPlayer()
{
	auto& profiles = Locator::playerProfileSystem::value();
	if (profiles.GetProfileCount() > 0)
	{
		return true;
	}
	if (!Locator::gameInterface::has_value() || !Locator::windowing::has_value() ||
	    Locator::config::value().graphicsBackend == GraphicsBackend::Noop)
	{
		// With no box to ask in, the player gets the profile the game makes for one who gives no name: the account's
		profiles.CreateProfile(u"", 0);
		return !_quitRequested;
	}
	// Tools wait while the player is asked
	const ecs::systems::InspectorLoading asking("new player");
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "No player has a profile: asking the new player for their name");
	SDL_StartTextInput();
	Locator::gameInterface::value().ShowNewPlayerBox(
	    false, [&profiles](std::u16string_view name) { return profiles.CheckNewName(name); });
	gui::menu_backdrop::Fade fade;
	auto last = Milliseconds();
	while (!gui::menu_backdrop::FadeOver(fade) && !_quitRequested)
	{
		const auto now = Milliseconds();
		const auto frame = static_cast<int>(now - last);
		last = now;
		auto& box = Locator::gameInterface::value().GetNewPlayerBox();
		const auto resolution = static_cast<glm::u16vec2>(Locator::windowing::value().GetSize());
		SDL_Event event;
		while (SDL_PollEvent(&event) != 0)
		{
			// On the web the mouse events get the place of the pointer of the game
			input::PointWindowEvent(event);
			if (event.type == SDL_QUIT)
			{
				RequestQuit();
			}
			else
			{
				Locator::gameInterface::value().ProcessNewPlayerBoxEvent(event, resolution);
			}
		}
		// The profile is made as the box is answered
		if (const auto answer = box.TakeAnswer(); answer.has_value())
		{
			profiles.CreateProfile(answer->name, answer->symbol);
		}
		gui::menu_backdrop::StepFade(fade, !box.IsActive(), frame);
		const auto mouse = input::WindowPointer();
		Locator::loadingScreenSystem::value().PresentStartupFrame([mouse, frame, &fade](glm::u16vec2 size) {
			Locator::gameInterface::value().DrawNewPlayerScreen(size, mouse, machine_clock::Ticks(), static_cast<float>(frame),
			                                                    fade.alpha);
		});
	}
	SDL_StopTextInput();
	if (const auto profile = profiles.GetCurrentProfile(); profile.has_value())
	{
		Locator::gameInterface::value().SetPlayerProfile(gui::ToUtf16(profile->name), gui::ToUtf16(profile->creatureName),
		                                                 profile->symbol, profile->settings);
		RefreshPlayersPage();
	}
	return !_quitRequested;
}

std::optional<std::filesystem::path> Game::AskForSkirmishLand()
{
	if (!Locator::gameInterface::has_value() || !Locator::windowing::has_value() ||
	    Locator::config::value().graphicsBackend == GraphicsBackend::Noop)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "The -SKIRMISH switch: there is no window for the skirmish box");
		return std::nullopt;
	}
	// Tools wait while the player chooses
	const ecs::systems::InspectorLoading asking("skirmish box");
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The -SKIRMISH switch: the skirmish box asks for the land");
	_skirmishLands = ReadSkirmishLands();
	std::vector<std::u16string> names;
	names.reserve(_skirmishLands.size());
	for (const auto& land : _skirmishLands)
	{
		names.push_back(land.titleText.has_value()
		                    ? std::u16string(Locator::gameInterface::value().GetTexts().Get(*land.titleText))
		                    : land.stem);
	}
	Locator::gameInterface::value().ShowSkirmishBox(std::move(names));
	gui::menu_backdrop::Fade fade;
	auto last = Milliseconds();
	std::optional<std::filesystem::path> picked;
	// A run with a number of frames (-n) stops after that number of frames, also in the box
	const auto frameLimit = Locator::config::value().numFramesToSimulate;
	uint32_t boxFrames = 0;
	while (!gui::menu_backdrop::FadeOver(fade) && !_quitRequested)
	{
		if (frameLimit > 0 && ++boxFrames > frameLimit)
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "The -SKIRMISH switch: the run stops in the skirmish box after {} frames",
			                   frameLimit);
			RequestQuit();
			break;
		}
		const auto now = Milliseconds();
		const auto frame = static_cast<int>(now - last);
		last = now;
		auto& box = Locator::gameInterface::value().GetSkirmishBox();
		const auto resolution = static_cast<glm::u16vec2>(Locator::windowing::value().GetSize());
		SDL_Event event;
		while (SDL_PollEvent(&event) != 0)
		{
			// On the web the mouse events get the place of the pointer of the game
			input::PointWindowEvent(event);
			if (event.type == SDL_QUIT)
			{
				RequestQuit();
			}
			else
			{
				Locator::gameInterface::value().ProcessSkirmishBoxEvent(event, resolution);
			}
		}
		if (const auto answer = box.TakeAnswer();
		    answer.choice == gui::SkirmishBox::Choice::Start && answer.land.has_value() && *answer.land < _skirmishLands.size())
		{
			picked = Locator::filesystem::value().GetPath<filesystem::Path::Playgrounds>() /
			         _skirmishLands.at(*answer.land).fileName;
		}
		gui::menu_backdrop::StepFade(fade, !box.IsActive(), frame);
		const auto mouse = input::WindowPointer();
		Locator::loadingScreenSystem::value().PresentStartupFrame([mouse, frame, &fade](glm::u16vec2 size) {
			Locator::gameInterface::value().DrawNewPlayerScreen(size, mouse, machine_clock::Ticks(), static_cast<float>(frame),
			                                                    fade.alpha);
		});
	}
	return _quitRequested ? std::nullopt : picked;
}

void Game::PlayStartupScreens()
{
	Locator::loadingScreenSystem::value().PlayStartupScreens(
	    {.build = _buildVersion.Short(), .skipLogos = _skipLogos, .preIntro = _preIntro});
}

void Game::BeginLoadingScreen(loading::LoadingClock::Mode look)
{
	Locator::loadingScreenSystem::value().BeginLoading(look);
}

bool Game::RenderLoadingFrame(bool drawNow, const std::function<void()>& drawUnder)
{
	return Locator::loadingScreenSystem::value().RenderLoadingFrame(drawNow, drawUnder);
}

void Game::FinishLoadingBehindScreen()
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
	while (resources.LoadingCount() > 0 && !_quitRequested)
	{
		resources.UpdateLoading(k_LoadingScreenLoadBudget, graphics::UploadPacer::Allowance {});
		RenderLoadingFrame();
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	const auto loaded = std::chrono::steady_clock::now();
	// The first frame draws into targets of the window's size. Thus the warm-up draws into the same targets.
	SizeSceneToWindow();
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

void Game::PrepareFirstFrames()
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

void Game::EndLoadWithoutScreen()
{
	// The renderer made the land's pipelines and resources before the load, so no frame warms them up. The work of the
	// first frame that depends on the loaded world is done now.
	PrepareFirstFrames();
}

void Game::EndLoadingScreen()
{
	auto& screens = Locator::loadingScreenSystem::value();
	if (!screens.IsLoading())
	{
		return;
	}
	FinishLoadingBehindScreen();
	screens.EndLoading();
}
