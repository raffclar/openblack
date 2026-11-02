/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PuzzleTotemArchetype.h"

#include "Common/PortableRotations.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/PuzzleGame.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Utils.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

entt::entity PuzzleTotemArchetype::Create(const glm::vec3& position, AbodeInfo kind, float yAngleRadians, float scale,
                                          entt::entity puzzle)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	// It takes its place in the order of the things made
	TakeThingMadePlace();
	const auto& info = Locator::infoConstants::value().abode.at(static_cast<size_t>(kind));
	const auto& transform =
	    registry.Assign<Transform>(entity, position, glm::mat3(portable_math::EulerAngleY(-yAngleRadians)), glm::vec3(scale));
	registry.Assign<Mesh>(entity, resources::HashIdentifier(info.meshId), static_cast<int8_t>(0), static_cast<int8_t>(0));
	const auto [point, radius] = GetFixedObstacleBoundingCircle(info.meshId, transform);
	registry.Assign<Fixed>(entity, point, radius);
	registry.Assign<PuzzleTotem>(entity, PuzzleTotem {.puzzle = puzzle});
	// Nothing destroys it
	registry.Assign<Indestructible>(entity);
	return entity;
}
