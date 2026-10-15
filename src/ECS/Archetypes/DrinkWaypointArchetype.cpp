/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DrinkWaypointArchetype.h"

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

#include "ECS/Components/DrinkWaypoint.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Locator.h"

using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

entt::entity DrinkWaypointArchetype::Create(const glm::vec3& position)
{
	auto& registry = openblack::Locator::entitiesRegistry::value();
	uint32_t made = 0;
	registry.Each<const DrinkWaypoint>([&made](const DrinkWaypoint&) { ++made; });
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, position, glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<DrinkWaypoint>(entity, made);
	return entity;
}
