/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <algorithm>
#include <ranges>
#include <span>
#include <vector>

#include "3D/TempleInteriorInterface.h"
#include "Camera/Camera.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/BoatFilm.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/HighDetail.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Sky.h"
#include "ECS/Components/VillagerPose.h"
#include "ECS/DrawnRegistry.h"
#include "ECS/Registry.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/RenderingSystemInterface.h"
#include "ECS/Systems/SkyClockSystemInterface.h"
#include "ECS/Systems/SkySystemInterface.h"
#include "ECS/Systems/VortexSystemInterface.h"
#include "EngineConfig.h"
#include "GameDetail.h"
#include "Graphics/DrawnFrame.h"
#include "Locator.h"
#include "Profiler.h"

namespace openblack::game_detail
{
namespace
{
using ecs::systems::RenderContext;
using graphics::DrawnFrame;

/// Adds the matrices to the bones of the frame and gives their run
DrawnFrame::BoneRun KeepBones(DrawnFrame& frame, std::span<const glm::mat4> bones)
{
	const DrawnFrame::BoneRun run {.first = static_cast<uint32_t>(frame.bones.size()),
	                               .count = static_cast<uint32_t>(bones.size())};
	frame.bones.insert(frame.bones.end(), bones.begin(), bones.end());
	return run;
}

/// The first draw that holds an instance takes it. Thus the pass uses the palette only when that first draw does.
std::vector<bool> FirstHolderPalette(const RenderContext& models)
{
	std::vector<bool> held(models.instanceUniforms.size(), false);
	std::vector<bool> palette(models.instanceUniforms.size(), false);
	if (!models.bonePaletteTexture)
	{
		return palette;
	}
	for (const auto* descs : {&models.instancedDrawDescs, &models.fadingDrawDescs})
	{
		for (const auto& [meshId, placers] : *descs)
		{
			const auto end = std::min<size_t>(static_cast<size_t>(placers.offset) + placers.count, palette.size());
			for (size_t instance = placers.offset; instance < end; ++instance)
			{
				if (!held[instance])
				{
					held[instance] = true;
					palette[instance] = placers.bonePalette;
				}
			}
		}
	}
	return palette;
}

DrawnFrame::EntityLook LookOf(DrawnFrame& frame, const ecs::Registry& registry, entt::entity entity, uint32_t instance,
                              const std::vector<bool>& palette)
{
	DrawnFrame::EntityLook look;
	if (const auto* pose = registry.TryGet<const ecs::components::VillagerPose>(entity))
	{
		DrawnFrame::VillagerLook villager {
		    .clip = pose->clip,
		    .poseBones = static_cast<uint32_t>(pose->bones.size()),
		    .bones = std::nullopt,
		    .inPhysics = registry.AllOf<ecs::components::InPhysics>(entity),
		    .inHand = registry.AllOf<ecs::components::InHand>(entity),
		    .highDetail = registry.AllOf<ecs::components::HighDetail>(entity),
		};
		// A villager that the bone palette draws needs no bones of its own
		if (instance >= palette.size() || !palette[instance])
		{
			villager.bones = KeepBones(frame, pose->bones);
		}
		look.villager = villager;
	}
	if (const auto* pose = registry.TryGet<const ecs::components::AnimalPose>(entity))
	{
		look.animal = DrawnFrame::AnimalLook {
		    .bones = KeepBones(frame, pose->bones),
		    .light = pose->light,
		    .colour = pose->colour,
		    .alpha = pose->alpha,
		    .cutBelow = pose->cutBelow,
		    .aboveCutShown = pose->aboveCutShown,
		    .underSea = pose->underSea,
		};
	}
	if (const auto* pose = registry.TryGet<const ecs::components::AnimatedStaticPose>(entity))
	{
		look.still = DrawnFrame::StillLook {
		    .bones = KeepBones(frame, pose->bones),
		    .boatFilmPart = registry.AllOf<ecs::components::BoatFilmPart>(entity),
		};
	}
	return look;
}

DrawnFrame::Sky SkyOf()
{
	DrawnFrame::Sky sky;
	if (Locator::skyClockSystem::has_value())
	{
		sky.skyType = Locator::skyClockSystem::value().GetCurrentSkyType();
	}
	if (Locator::alignmentSystem::has_value())
	{
		sky.skyLand = Locator::alignmentSystem::value().GetSkyLand();
	}
	if (!Locator::skySystem::has_value())
	{
		return sky;
	}
	const auto& skySystem = Locator::skySystem::value();
	const auto& registry = Locator::entitiesRegistry::value();
	if (const auto* dome = registry.TryGet<const ecs::components::SkyDome>(skySystem.GetDome()))
	{
		sky.dome = *dome;
	}
	sky.sunHidden = skySystem.IsSunHidden();
	if (const auto [sun, body] =
	        registry.TryGet<const ecs::components::Sun, const ecs::components::CelestialBody>(skySystem.GetSun());
	    sun != nullptr && body != nullptr)
	{
		sky.sun = DrawnFrame::Sky::Sun {.sun = *sun, .body = *body};
	}
	if (const auto [moon, body, glow] = registry.TryGet<const ecs::components::Moon, const ecs::components::CelestialBody,
	                                                    const ecs::components::CelestialGlow>(skySystem.GetMoon());
	    moon != nullptr && body != nullptr)
	{
		sky.moon = DrawnFrame::Sky::Moon {
		    .moon = *moon,
		    .body = *body,
		    .glow = glow != nullptr ? std::optional(*glow) : std::nullopt,
		};
	}
	return sky;
}
} // namespace

void FillDrawnFrame(graphics::DrawnFrame& frame, const EngineConfig& config, uint32_t time)
{
	auto section = Locator::profiler::value().BeginScoped(Profiler::Stage::DrawnFrameFill);
	frame.camera = Locator::camera::has_value() ? Locator::camera::value().DrawnCopy() : nullptr;
	frame.time = time;
	frame.timeOfDay = config.timeOfDay;
	frame.templeActive = Locator::temple::has_value() && Locator::temple::value().Active();
	frame.sky = SkyOf();

	frame.land.reflected = config.detail.landReflection;
	frame.land.coarse = config.detail.coarseLand;
	frame.land.groundMarks.clear();
	if (Locator::vortexSystem::has_value())
	{
		const auto marks = Locator::vortexSystem::value().GetGroundMarks();
		frame.land.groundMarks.assign(marks.begin(), marks.end());
	}

	frame.entityLooks.clear();
	frame.drawnEntityLooks.clear();
	frame.bones.clear();
	frame.models = nullptr;
	frame.modelWrites = 0;
	if (!Locator::rendereringSystem::has_value())
	{
		return;
	}
	auto& renderingSystem = Locator::rendereringSystem::value();
	const auto& models = renderingSystem.GetContext();
	frame.models = &models;
	frame.modelWrites = renderingSystem.GetContextWrites();
	const auto palette = FirstHolderPalette(models);
	const auto& registry = Locator::entitiesRegistry::value();
	frame.entityLooks.reserve(models.entityDraws.size());
	for (const auto& [entity, instance] : models.entityDraws)
	{
		frame.entityLooks.push_back(LookOf(frame, registry, entity, instance, palette));
	}
	if (Locator::drawnRegistry::has_value())
	{
		const auto& drawn = Locator::drawnRegistry::value();
		frame.drawnEntityLooks.reserve(models.drawnEntityDraws.size());
		for (const auto& [entity, instance] : models.drawnEntityDraws)
		{
			frame.drawnEntityLooks.push_back(LookOf(frame, drawn, entity, instance, palette));
		}
	}
}

} // namespace openblack::game_detail
