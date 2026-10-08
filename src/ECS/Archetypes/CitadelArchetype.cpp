/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CitadelArchetype.h"

#include <cmath>

#include <bit>

#include <entt/core/hashed_string.hpp>
#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "3D/ObjectMatrix.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Life.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandTap.h"
#include "ECS/Systems/TempleExteriorSystemInterface.h"
#include "ECS/Town/BuildingSites.h"
#include "ECS/Town/TownPlacement.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Worship/Citadel.h"
#include "Worship/SpecialPoints.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

namespace
{
/// The heart's mesh, B_FIRST_TEMPLE from Data\Citadel\OutsideMeshes (Game.cpp loads that folder as "temple/<file>"),
/// until the outside's first blend gives the heart its own
constexpr auto k_TempleMesh = entt::hashed_string("temple/b_first_temple_l3d");
/// The heart's creation skips the creation index of the heart, its CitadelEntrance and its temple leash: openblack
/// makes the entrance without one, and no leash
constexpr uint32_t k_HeartObjects = 3;

/// The land flattening under the temple: cx = truncate(x x 0.1), cz = truncate(z x 0.1); A = the altitude byte of the
/// land cell (cx, cz), 0 outside 0..511 or without a block; for x = cx - 8..cx + 8 (outer), z = cz - 8..cz + 8: t =
/// the blend factor of (x - cx, z - cz), a = the cell's altitude (0 outside), the cell's altitude =
/// truncate(a x t + (1 - t) x A) & 0xFF. It runs when the heart is created: at load for CREATE_CITADEL, at the plan's
/// conversion for a planned citadel
void FlattenLandUnderTemple(const glm::vec3& position)
{
	if (!Locator::terrainSystem::has_value() || Locator::terrainSystem::value().GetMaterialInfo().empty())
	{
		return; // no island loaded (tests)
	}
	auto& island = Locator::terrainSystem::value();
	// the 3D object's x / z (its map coordinates) x 0.1, truncated (a float product)
	const float x10 = map_coords::Quantise(position.x) * 0.1f;
	const float z10 = map_coords::Quantise(position.z) * 0.1f;
	const auto cx = static_cast<int>(x10);
	const auto cz = static_cast<int>(z10);
	// the original's bounds are 0..511; openblack's island its cells per side
	const int last = island.GetCellsPerSide() - 1;
	const auto altitudeAt = [&island, last](int x, int z) {
		if (x < 0 || z < 0 || x > last || z > last)
		{
			return 0;
		}
		return static_cast<int>(island.GetCellAltitude(island.GetCell(glm::u16vec2(x, z))) & 0xFF);
	};
	const int centreAltitude = altitudeAt(cx, cz);
	int changed = 0;
	for (int x = cx - 8; x <= cx + 8; ++x)
	{
		for (int z = cz - 8; z <= cz + 8; ++z)
		{
			const auto value = CitadelArchetype::FlattenedAltitude(x - cx, z - cz, altitudeAt(x, z), centreAltitude);
			// the original writes every cell, those out of the map too. (pending) whether it clamps them: openblack's
			// island has no cell there to write
			if (x < 0 || z < 0 || x > last || z > last)
			{
				continue;
			}
			if (static_cast<int>(value) != altitudeAt(x, z))
			{
				island.SetCellAltitude(glm::u16vec2(x, z), value);
				++changed;
			}
		}
	}
	// (openblack) the land blocks, their physics and the height map follow the new altitudes
	island.RebuildAltitudes();
	// (openblack) RebuildAltitudes remakes the whole height map in this call (a frame hitch inside BUILD_BUILDING; the
	// original rewrites its land in place)
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Temple at ({:.0f}, {:.0f}): land flattened to {} ({} cells changed)", position.x,
	                   position.z, centreAltitude, changed);
}

/// The citadel entrance: an object at the heart's point (heart angle, scale 1) that points back at its heart, with a
/// 3D object of the entrance mesh, Entrance.l3d. Its draw does nothing and nothing puts it in the map cells: the hand's
/// pick collides that mesh once the temple is fully built (HandSystem::PickObjectAlongRay), so it has no Mesh here (a
/// Mesh would draw it and print a footprint)
entt::entity CreateEntrance(entt::entity heart, const glm::vec3& position, float yAngle)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entrance = registry.Create();
	registry.Assign<Transform>(entrance, position, affine::AngleY(yAngle), glm::vec3(1.0f));
	registry.Assign<CitadelEntrance>(entrance, heart);
	return entrance;
}
} // namespace

uint16_t CitadelArchetype::FlattenedAltitude(int dx, int dz, int altitude, int centreAltitude)
{
	// the blend factor: d = sqrt((10 dx)^2 + (10 dz)^2) stored as a float; d > 70 -> 1; d < 35 -> 0; else (d - 35) x
	// 0.0285714 (about 1/35, the exact float bits below). The game runs its floating point at single precision: every
	// step rounds to a float, so d == 70 gives exactly 1
	const float x = static_cast<float>(dx) * 10.0f;
	const float z = static_cast<float>(dz) * 10.0f;
	const float xx = x * x;
	const float zz = z * z;
	const float d = std::sqrt(xx + zz);
	float t = 0.0f;
	if (d > 70.0f)
	{
		t = 1.0f;
	}
	else if (!(d < 35.0f))
	{
		const float over = d - 35.0f;
		t = over * std::bit_cast<float>(0x3CEA0EA1u);
	}
	// a x t + (1 - t) x A, each step a float, truncated, & 0xFF
	const float at = static_cast<float>(altitude) * t;
	const float rest = 1.0f - t;
	const float centre = rest * static_cast<float>(centreAltitude);
	const float sum = at + centre;
	return static_cast<uint16_t>(static_cast<int32_t>(sum) & 0xFF);
}

const GCitadelHeartInfo& CitadelArchetype::HeartInfo(uint32_t heartInfo)
{
	if (heartInfo != 0)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "Citadel: heart info {} read as 0 (openblack keeps one record)", heartInfo);
	}
	return Locator::infoConstants::value().citadelHeart;
}

entt::entity CitadelArchetype::CreateHeart(const glm::vec3& position, PlayerNames owner, entt::entity citadel, float yAngle,
                                           float scale, float life, bool underConstruction)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& info = HeartInfo(0);
	// (openblack) the CitadelEntrance class's tap handlers, once
	RegisterTapHandlers();
	// 1. the heart
	ecs::object_index::Skip(k_HeartObjects);
	const auto heart = registry.Create();
	// the point of the map coordinates, the Y angle and scale 1.0 (the plan's scale only feeds the influence)
	registry.Assign<Transform>(heart, position, affine::AngleY(yAngle), glm::vec3(1.0f));
	registry.Assign<Temple>(heart, owner);
	// under construction -> under-construction flag, percent built 0; else built, percent built = life
	auto& part = registry.Assign<CitadelPartBuild>(heart);
	if (underConstruction)
	{
		part.buildFlags = CitadelPartBuild::k_UnderConstruction;
		part.percentBuilt = 0.0f;
	}
	else
	{
		part.buildFlags = CitadelPartBuild::k_Built;
		part.percentBuilt = life;
	}
	// the part's citadel, its life = the info's StartLife, the part at the head of the citadel's parts. A new citadel
	// when the player has none: openblack keeps it on this entity (CitadelWorship); the heart becomes the citadel's
	// heart when it has none, and its influence (InfluenceSources' CitadelInfluence)
	auto& component = registry.Assign<CitadelHeart>(heart);
	component.citadel = citadel != entt::null ? citadel : heart;
	component.scale = scale;
	ecs::life::SetLife(heart, info.startLife);
	if (citadel == entt::null)
	{
		worship::citadel::Initialise(heart, yAngle);
	}
	// 2. the 3D side (skipped when the 3D object exists: never here)
	//    2.1 the 3D object (citadel type); its position (above)
	//    2.2 the mesh with the percent built and the player, its fade 0, then the draw percent set with that percent;
	//        the land flattening
	registry.Assign<Mesh>(heart, k_TempleMesh, static_cast<int8_t>(0), static_cast<int8_t>(0));
	worship::citadel::SetHeartDrawPercent(heart, registry.Get<const CitadelPartBuild>(heart).percentBuilt);
	FlattenLandUnderTemple(position);
	//        the outside's start, neutral and small, and its first blend, on the flattened land. (openblack) tests
	//        without the service make the heart without it
	if (Locator::templeExteriorSystem::has_value())
	{
		Locator::templeExteriorSystem::value().Create(heart);
	}
	//    2.3 the fixed-for-map list (map_cells' own)
	//    2.4 the entrance
	registry.Get<CitadelHeart>(heart).entrance = CreateEntrance(heart, position, yAngle);
	//    2.5 into the map cells. (approximate) its collide shape is not ported: map_cells takes the mesh's
	ecs::map_cells::InsertMapObject(heart);
	//    2.6 (pending) the global heart list, the leashes and the rest of the heart's attachments
	// 3. the alignment bird flock (not ported)
	// 4. life >= 1 -> the citadel's worship sites opened: CREATE_CITADEL; never a plan's conversion
	if (!(life < 1.0f))
	{
		worship::citadel::OpenWorshipSites(registry.Get<const CitadelHeart>(heart).citadel, 0.0f);
	}
	return heart;
}

entt::entity CitadelArchetype::Create(const glm::vec3& position, PlayerNames playerOwner, const glm::mat4& rotation,
                                      [[maybe_unused]] const glm::vec3& size)
{
	// a new citadel and its heart (angle, scale 1.0, life 1.0, built)
	return CreateHeart(position, playerOwner, entt::null, worship::YAngleOf(glm::mat3(rotation)), 1.0f, 1.0f, false);
}

void CitadelArchetype::CreatePlan(entt::entity town, const glm::vec3& position, uint32_t heartInfo, float yAngle, float scale)
{
	// a plan (position, angle, scale, heart info, the town) added to the town's planned list
	if (town == entt::null)
	{
		return;
	}
	PlannedAbode plan {AbodeInfo::None, position, yAngle, scale, false};
	plan.citadelHeart = true;
	plan.heartInfo = heartInfo;
	ecs::plans::AddPlanned(town, plan);
}

entt::entity CitadelArchetype::CreatePlanned(entt::entity town, size_t plan, float life)
{
	const auto* t = Locator::entitiesRegistry::value().TryGet<const Town>(town);
	if (t == nullptr || plan >= t->plannedAbodes.size())
	{
		return entt::null;
	}
	// the heart info's mesh must be suitable for a fixed object at the plan's position, angle and scale
	const auto& p = t->plannedAbodes.at(plan);
	const auto& info = HeartInfo(p.heartInfo);
	const auto at = map_coords::FromMetres(glm::vec2(p.position.x, p.position.z));
	const auto mesh = resources::HashIdentifier(info.meshType);
	if (!ecs::town_placement::IsSuitableForFixed(at, mesh, p.yAngleRadians, p.scale))
	{
		return entt::null;
	}
	return CreatePlannedNoFixedCheck(town, plan, life);
}

entt::entity CitadelArchetype::CreatePlannedNoFixedCheck(entt::entity town, size_t plan, float life)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* t = registry.TryGet<const Town>(town);
	if (t == nullptr || plan >= t->plannedAbodes.size() || !t->plannedAbodes.at(plan).citadelHeart)
	{
		return entt::null;
	}
	const PlannedAbode p = t->plannedAbodes.at(plan);
	// 1. the town's player (the town used without a null check); none -> null. openblack's towns always have an owner
	//    (the neutral player too)
	const auto player = t->owner;
	// 2. the player's citadel; none -> a new one, made with the heart below
	const auto citadel = worship::citadel::Of(player);
	// 3. the heart under construction (plan position, angle, scale, life); none -> null
	const auto heart = CreateHeart(p.position, player, citadel, p.yAngleRadians, p.scale, life, true);
	if (heart == entt::null)
	{
		return entt::null;
	}
	// 4. the heart's town = the plan's town
	registry.Get<CitadelHeart>(heart).town = town;
	// 5. a rebuild plan's site is a repair site
	if (p.wasBuilt)
	{
		registry.Get<CitadelPartBuild>(heart).buildFlags |= CitadelPartBuild::k_NotRepaired;
	}
	// 6. the plan's footpaths (not ported); the plan has no town of its own: no new-building check
	// 7. the plan deleted (removed from the town's list)
	ecs::plans::RemovePlanned(town, plan);
	// 8. the heart
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Citadel: the plan of town {} is a temple under construction ({})", t->id,
	                   static_cast<uint32_t>(heart));
	return heart;
}

void CitadelArchetype::RegisterTapHandlers()
{
	// hand_tap::Register keeps the first registration of a class
	// the entrance's valid-to-tap and tap
	ecs::hand_tap::Register<CitadelEntrance>(
	    [](entt::entity entrance, const ecs::pot_resource::Dropper&) { return worship::citadel::EntranceValidToTap(entrance); },
	    [](entt::entity entrance, const ecs::pot_resource::Dropper& is, glm::vec3) {
		    return worship::citadel::EntranceTap(entrance, is.isMyInterface);
	    });
}
