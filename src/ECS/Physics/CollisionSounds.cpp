/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CollisionSounds.h"

#include <cstdlib>

#include <algorithm>
#include <array>
#include <vector>

#include <LNDFile.h>
#include <entt/core/hashed_string.hpp>
#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "Audio/Audio.h"
#include "Buildings.h"
#include "Debug/DebugEnv.h"
#include "Dust.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/FishShoals.h"
#include "ECS/Physics/PhysicsObjectsState.h"
#include "ECS/Registry.h"
#include "ECS/SeaCells.h"
#include "ECS/Systems/PhysicsObjectsSystemInterface.h"
#include "ECS/WaterRings.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "PhysicsObjects.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::physics;

namespace
{
constexpr int k_Ground = 27;   // SOUND_COLLISION_TYPE_GROUND
constexpr int k_Water = 29;    // SOUND_COLLISION_TYPE_WATER
constexpr int k_Fragment = 31; // SOUND_COLLISION_TYPE_FRAGMENT
constexpr int k_Grain = 11;
constexpr int k_Bush = 2;
constexpr int k_HollowWood = 18;
constexpr int k_BrickBuilding = 10;

// the A (hitter) and B (hit) codes of each SOUND_COLLISION_TYPE
constexpr std::array<int, 33> k_TabA = {21, 24, 20, 20, 20, 20, 20, 19, 19, 19, 19, 20, 25, 22, 22, 22, 22,
                                        22, 30, 25, 35, 31, 24, 23, 24, 25, 25, 21, 21, 21, 33, 34, 42};
constexpr std::array<int, 33> k_TabB = {11, 14, 10, 10, 10, 10, 10, 9,  9,  9,  9,  10, 15, 12, 12, 12, 12,
                                        12, 20, 15, 23, 19, 14, 13, 14, 15, 15, 16, 18, 17, 21, 22, 24};

using Pair = openblack::ecs::physics::SoundPair;

/// The physics objects' state (Locator::physicsObjectsSystem)
openblack::ecs::physics::State& PhysicsState()
{
	return openblack::Locator::physicsObjectsSystem::value().GetState();
}

bool Listed(entt::entity a, entt::entity b)
{
	return std::ranges::any_of(PhysicsState().soundPairs,
	                           [&](const Pair& p) { return (p.a == a && p.b == b) || (p.a == b && p.b == a); });
}
} // namespace

int CollisionSounds::TypeOf(entt::entity entity)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (registry.AllOf<Fragment>(entity))
	{
		return k_Bush;
	}
	if (registry.AllOf<DeadTree>(entity))
	{
		if (const auto* mesh = registry.TryGet<const Mesh>(entity);
		    mesh != nullptr && mesh->id == resources::HashIdentifier(static_cast<MeshId>(406)))
		{
			return k_HollowWood;
		}
	}
	if (registry.AnyOf<Abode, StoragePit>(entity))
	{
		return k_BrickBuilding;
	}
	if (const auto* info = PhysicsObjects::ObjectInfo(entity))
	{
		return static_cast<int>(info->collideSound);
	}
	return 0;
}

void CollisionSounds::PlayAnimEffect(const std::array<int32_t, 5>& key, entt::entity owner, glm::vec3 at, bool track)
{
	// the distance from the camera to the point
	const auto camera = audio::ListenerPoint();
	const float distance = camera ? glm::distance(*camera, at) : 0.0f;
	audio::PlayAnimationEffect(owner != entt::null ? audio::Owner::Thing(owner) : audio::Owner::None(), distance, key,
	                           audio::AnimAction::Play, audio::Bank(audio::SfxBank::Editor), track, 0.0f, 0.0f);
}

void CollisionSounds::AttemptToAddSoundEvent(const PhysicsObject& po)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto obj = po.entity;
	const auto hitObj = po.hitBy != nullptr ? po.hitBy->entity : entt::entity {entt::null};
	// a rock hitting a building: the building plays its own sound
	if (hitObj != entt::null && registry.AnyOf<Abode, StoragePit>(hitObj) && Buildings::PhysicallyDestroysAbodes(obj))
	{
		return;
	}
	if (Listed(obj, hitObj) || PhysicsState().soundPairs.size() >= 128)
	{
		return;
	}
	const int typeA = TypeOf(obj);
	int typeB = k_Ground;
	const auto at = po.body.Centre();
	if (hitObj != entt::null)
	{
		typeB = TypeOf(hitObj);
	}
	else if (Locator::terrainSystem::has_value())
	{
		// not IsDryLand (altitude < 4, the water bit is not read) -> ring; and no cell (off the map, no block) or an
		// altitude under 3 at the cell rounded to the nearest -> WATER; altitude 3: ring + dust
		const auto& terrain = Locator::terrainSystem::value();
		const bool land = ecs::sea_cells::IsDryLand(terrain, ecs::sea_cells::CellOf(at));
		const auto* c = land ? nullptr : ecs::sea_cells::CellAt(terrain, ecs::sea_cells::RoundedCellOf(at));
		const bool deep = !land && (c == nullptr || terrain.GetCellAltitude(*c) < 3);
		const float ground = terrain.GetHeightAt(glm::vec2(at.x, at.z));
		const float size = std::min(2.0f * po.body.Radius(), 5.0f);
		const auto dustAt = glm::vec3(at.x, ground, at.z);
		if (deep)
		{
			typeB = k_Water;
			ecs::SplashWater(at);
		}
		if (!land)
		{
			// the body radius (max |vertex - CoM| x scale); rate 1 / r and growth 2 r with no guard
			const float radius = po.body.Radius();
			const ecs::WaterRing ring {
			    .position = glm::vec3(at.x, 0.1f, at.z), .growth = 2.0f * radius, .rate = 1.0f / radius, .cell = 0x3F};
			ecs::AddWaterRing(ring);
		}
		// 6 liquid particles (1 s, no gravity) of the foam / dust colour. The original tints that colour first by the
		// snow cover at the point (a 128 x 128 grid, 40 units a cell, k in 0..255): each channel c += floor((base - c) *
		// k / 256) towards the light's base colour. Without snow k = 0 and the colour stays; there is no snow cover
		// yet, so none is applied.
		for (int i = 0; i < 6; ++i)
		{
			Dust::Emit(dustAt, Dust::RandomVelocity(), deep ? 0x28C8F0F4u : 0x50806040u, size);
		}
	}
	if (typeA == k_Fragment || typeB == k_Fragment)
	{
		return;
	}
	PhysicsState().soundPairs.push_back({obj, hitObj, 2});
	// level from the unscaled info weight: 3 soft (g < 1.25), 2, 1 hard (g > 3); GRAIN never at 1
	const auto* info = PhysicsObjects::ObjectInfo(obj);
	const float weight = info != nullptr ? info->weight : 0.0f;
	const float g = weight > 0.0f ? po.impact / (weight * 9.81f) : 1000.0f;
	int level = g < 1.25f ? 3 : g > 3.0f ? 1 : 2;
	if (typeA == k_Grain && level == 1)
	{
		level = 2;
	}
	const int a = k_TabA.at(static_cast<size_t>(std::clamp(typeA, 0, 32)));
	const int b = k_TabB.at(static_cast<size_t>(std::clamp(typeB, 0, 32)));
	// the key {level, 0, A, B, 75 COLLIDE}
	const std::array<int32_t, 5> key = {level, 0, a, b, 75};
	if (debug_env::PhysicsTrace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Collision sound: types {}/{} level {} -> editor.sad key {{{}, 0, {}, {}, 75}}",
		                   typeA, typeB, level, level, a, b);
	}
	// the channel follows the object unless its A code is 0x16
	PlayAnimEffect(key, obj, at, a != 0x16);
}

void CollisionSounds::EndTurn()
{
	for (auto& p : PhysicsState().soundPairs)
	{
		--p.turns;
	}
	std::erase_if(PhysicsState().soundPairs, [](const Pair& p) { return p.turns <= 0; });
}
