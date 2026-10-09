/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ArenaArchetype.h"

#include <memory>

#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Creature/CreatureArena.h"
#include "ECS/Components/CreatureArena.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "Locator.h"
#include "Particles/LightSheet.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

entt::entity ArenaArchetype::Create(const glm::ivec2& place, float radius, bool temporary)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto ground = [](glm::vec2 point) {
		return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(point) : 0.0f;
	};
	const glm::vec2 centre {map_coords::ToMetres(place.x), map_coords::ToMetres(place.y)};
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, glm::vec3(centre.x, ground(centre), centre.y), glm::mat3(1.0f), glm::vec3(1.0f));

	auto ring = std::make_shared<particles::LightSheet>();
	ring->Start(creature_arena::RingPoints(centre, radius, ground), creature_arena::k_RingRgb,
	            radius * creature_arena::k_RingHeightShare, creature_arena::k_RingShiftSeconds);
	ring->SetSpread(creature_arena::k_RingSpread);
	ring->SetStrength(creature_arena::k_RingStrength);
	ring->SetHidden(true);
	if (Locator::particleSystem::has_value())
	{
		Locator::particleSystem::value().AddLightSheet(ring);
	}
	registry.Assign<CreatureArena>(entity, CreatureArena {.place = place,
	                                                      .radius = radius,
	                                                      .temporary = temporary,
	                                                      .fightOn = false,
	                                                      .first = entt::null,
	                                                      .second = entt::null,
	                                                      .ring = std::move(ring)});
	return entity;
}
