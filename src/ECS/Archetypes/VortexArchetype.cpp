/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VortexArchetype.h"

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

#include "ECS/Components/Transform.h"
#include "ECS/Components/Vortex.h"
#include "ECS/Registry.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

entt::entity VortexArchetype::Create(const glm::vec3& centre, VortexType type, VortexStateType state, uint32_t turn)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, centre, glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Vortex>(entity, Vortex {.type = type, .state = state, .stateStartTurn = turn, .centre = centre});
	return entity;
}
