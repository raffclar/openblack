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

#include "3D/DayNightClock.h"
#include "3D/LandIslandInterface.h"
#include "3D/OceanInterface.h"
#include "3D/TempleDoors.h"
#include "3D/TempleInteriorInterface.h"
#include "Camera/Camera.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/BoatFilm.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/HighDetail.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Sky.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/VillagerPose.h"
#include "ECS/DrawnRegistry.h"
#include "ECS/GamePlayers.h"
#include "ECS/Registry.h"
#include "ECS/SwimmerRules.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/RenderingSystemInterface.h"
#include "ECS/Systems/SkyClockSystemInterface.h"
#include "ECS/Systems/SkySystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
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

using InstanceHolder = DrawnFrame::InstanceHolder;

/// The first draw that holds an instance takes it. Thus the pass uses the palette only when that first draw does. The
/// list keeps its memory from frame to frame.
void FindFirstHolders(const RenderContext& models, std::vector<InstanceHolder>& holders)
{
	holders.assign(models.instanceUniforms.size(), InstanceHolder::None);
	if (!models.bonePaletteTexture)
	{
		return;
	}
	for (const auto* descs : {&models.instancedDrawDescs, &models.fadingDrawDescs})
	{
		for (const auto& [meshId, placers] : *descs)
		{
			const auto end = std::min<size_t>(static_cast<size_t>(placers.offset) + placers.count, holders.size());
			const auto taken = placers.bonePalette ? InstanceHolder::WithPalette : InstanceHolder::WithoutPalette;
			for (size_t instance = placers.offset; instance < end; ++instance)
			{
				if (holders[instance] == InstanceHolder::None)
				{
					holders[instance] = taken;
				}
			}
		}
	}
}

DrawnFrame::EntityLook LookOf(DrawnFrame& frame, const ecs::Registry& registry, entt::entity entity, uint32_t instance,
                              std::span<const InstanceHolder> holders)
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
		if (instance >= holders.size() || holders[instance] != InstanceHolder::WithPalette)
		{
			villager.bones = KeepBones(frame, pose->bones);
		}
		// A swimmer is cut at the water that it swims in: the sea, or a lake above the sea of a generated land
		const auto* transform = registry.TryGet<const ecs::components::Transform>(entity);
		if (transform != nullptr && Locator::terrainSystem::has_value() &&
		    ecs::swimmer_rules::Swims(villager.highDetail, villager.clip))
		{
			villager.waterLevel =
			    Locator::terrainSystem::value().GetWaterLevelAt({transform->position.x, transform->position.z});
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
		const auto& skyClock = Locator::skyClockSystem::value();
		sky.skyType = skyClock.GetCurrentSkyType();
		sky.windowTime = skyClock.GetClock().GetVisualTime();
		sky.windowNight = skyClock.GetClock().IsVisualNight();
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
/// Copies what the passes read of the temple into the copy that the frame keeps. The vectors keep their memory from
/// frame to frame.
void FillTemple(graphics::DrawnTemple& drawn, bool active)
{
	drawn.ClearRoomParts();
	drawn.text.clear();
	drawn.map.clear();
	drawn.mapMarkers.clear();
	drawn.mapSpellSeeds.clear();
	drawn.caveTrophies.clear();
	drawn.caveSeeds.clear();
	if (!active)
	{
		drawn.roomsDrawn = {};
		drawn.wayOutSeconds.reset();
		drawn.mapInfluence.reset();
		drawn.textTexture = nullptr;
		return;
	}
	const auto& temple = Locator::temple::value();
	drawn.position = temple.GetPosition();
	drawn.light = temple.GetLight();
	drawn.fog = temple.GetFog();
	for (size_t room = 0; room < graphics::DrawnTemple::k_RoomCount; ++room)
	{
		drawn.roomsDrawn.at(room) = temple.IsRoomDrawn(static_cast<TempleRoom>(room));
	}
	drawn.wayOutSeconds = temple.GetWayOutSeconds();
	drawn.visits = temple.GetVisits();
	drawn.poolTime = temple.GetPoolTime();
	drawn.mapMarkerTurn = temple.GetMapMarkerTurn();
	drawn.text.assign(temple.GetText().begin(), temple.GetText().end());
	drawn.textTexture = temple.GetTextTexture();
	drawn.map.assign(temple.GetMap().begin(), temple.GetMap().end());
	drawn.mapMarkers.assign(temple.GetMapMarkers().begin(), temple.GetMapMarkers().end());
	drawn.mapSpellSeeds.assign(temple.GetMapSpellSeeds().begin(), temple.GetMapSpellSeeds().end());
	drawn.mapInfluence = temple.GetMapInfluence();
	drawn.caveTrophies.assign(temple.GetCaveTrophies().begin(), temple.GetCaveTrophies().end());
	drawn.caveSeeds.assign(temple.GetCaveSeeds().begin(), temple.GetCaveSeeds().end());
	drawn.doorJoints = temple.GetDoors().GetJoints();
	drawn.CopyRoomParts(temple.GetRooms());
}

DrawnFrame::Hands HandsOf()
{
	DrawnFrame::Hands hands;
	if (Locator::handSystem::has_value())
	{
		const auto& handSystem = Locator::handSystem::value();
		hands.ownPlayer = handSystem.GetOwnPlayer();
		const auto own = handSystem.GetOwnHands();
		std::ranges::copy(own, hands.own.begin());
	}
	if (Locator::alignmentSystem::has_value())
	{
		hands.ownAlignment = Locator::alignmentSystem::value().GetPlayerAlignment(hands.ownPlayer);
	}
	return hands;
}

DrawnFrame::Sea SeaOf()
{
	if (!Locator::oceanSystem::has_value())
	{
		return {};
	}
	const auto& ocean = Locator::oceanSystem::value();
	return {.reflection = &ocean.GetReflectionFramebuffer(), .mesh = &ocean.GetMesh(), .texture = ocean.GetTexture()};
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
	if (Locator::time::has_value())
	{
		const auto& clock = Locator::time::value();
		frame.clock = {.turnFraction = clock.GetTurnFraction(),
		               .frameGameTime = clock.GetFrameGameTime(),
		               .frameRealTime = clock.GetFrameRealTime(),
		               .elapsed = clock.GetElapsedTime()};
	}
	else
	{
		frame.clock = {};
	}
	FillTemple(frame.temple, frame.templeActive);
	frame.hands = HandsOf();
	frame.sea = SeaOf();

	frame.land.island = Locator::terrainSystem::has_value() ? &Locator::terrainSystem::value() : nullptr;
	frame.land.extent = frame.land.island != nullptr ? std::optional(frame.land.island->GetExtent()) : std::nullopt;
	frame.land.highestHeight = frame.land.island != nullptr ? frame.land.island->HighestHeight() : 0.0f;
	frame.land.number = ecs::game_players::LandNumber();
	if (frame.land.island != nullptr)
	{
		frame.land.heights.Follow(*frame.land.island);
	}
	else
	{
		frame.land.heights.Clear();
	}
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
	FindFirstHolders(models, frame.instanceHolders);
	const auto& registry = Locator::entitiesRegistry::value();
	frame.entityLooks.reserve(models.entityDraws.size());
	for (const auto& [entity, instance] : models.entityDraws)
	{
		frame.entityLooks.push_back(LookOf(frame, registry, entity, instance, frame.instanceHolders));
	}
	if (Locator::drawnRegistry::has_value())
	{
		const auto& drawn = Locator::drawnRegistry::value();
		frame.drawnEntityLooks.reserve(models.drawnEntityDraws.size());
		for (const auto& [entity, instance] : models.drawnEntityDraws)
		{
			frame.drawnEntityLooks.push_back(LookOf(frame, drawn, entity, instance, frame.instanceHolders));
		}
	}
}

} // namespace openblack::game_detail
