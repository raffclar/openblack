/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PuzzleGameArchetype.h"

#include <glm/vec3.hpp>

#include "Common/X87Angle.h"
#include "ECS/Components/PuzzleGame.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Locator.h"

using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

entt::entity PuzzleGameArchetype::Create(const glm::vec3& position, float yAngleRadians, float scale,
                                         script::PuzzleGameType type)
{
	auto& registry = openblack::Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, position, openblack::x87::GameTurnMatrix(0.0f, yAngleRadians, 0.0f), glm::vec3(scale));
	registry.Assign<PuzzleGame>(entity, type);
	return entity;
}
