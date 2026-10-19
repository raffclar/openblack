/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DeadTreeArchetype.h"

#include <glm/mat3x3.hpp>

#include "Common/X87Angle.h"
#include "ECS/Components/DeadTree.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Unlit.h"
#include "ECS/PlacedTurn.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Utils.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

entt::entity DeadTreeArchetype::Create(const glm::vec3& position, TreeInfo type, float yAngleRadians, float scale)
{
	// An uprooted tree keeps the mesh of the tree it was
	const auto& info = Locator::infoConstants::value().tree.at(static_cast<size_t>(type));
	const auto entity = Create(position, type, x87::GameTurnMatrix(0.0f, yAngleRadians, 0.0f), scale, info.normal);
	Locator::entitiesRegistry::value().Get<DeadTree>(entity).model.reset();
	// The thing keeps the angles it got
	ecs::placed_turn::KeepAngles(Locator::entitiesRegistry::value(), entity, glm::vec3(0.0f, yAngleRadians, 0.0f));
	return entity;
}

entt::entity DeadTreeArchetype::Create(const glm::vec3& position, TreeInfo type, float xAngleRadians, float yAngleRadians,
                                       float zAngleRadians, float scale)
{
	const auto& info = Locator::infoConstants::value().tree.at(static_cast<size_t>(type));
	const auto entity =
	    Create(position, type, x87::GameTurnMatrix(xAngleRadians, yAngleRadians, zAngleRadians), scale, info.normal);
	Locator::entitiesRegistry::value().Get<DeadTree>(entity).model.reset();
	// The thing keeps the angles it got
	ecs::placed_turn::KeepAngles(Locator::entitiesRegistry::value(), entity,
	                             glm::vec3(xAngleRadians, yAngleRadians, zAngleRadians));
	return entity;
}

entt::entity DeadTreeArchetype::Create(const glm::vec3& position, TreeInfo type, const glm::mat3& rotation, float scale,
                                       MeshId model)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();

	const auto& transform = registry.Assign<Transform>(entity, position, rotation, glm::vec3(scale));
	const auto [point, radius] = GetFixedObstacleBoundingCircle(model, transform);
	registry.Assign<Fixed>(entity, point, radius);
	registry.Assign<DeadTree>(entity, DeadTree {.type = type, .model = model});
	registry.Assign<Unlit>(entity);
	registry.Assign<Mesh>(entity, resources::HashIdentifier(model), static_cast<int8_t>(0), static_cast<int8_t>(-1));

	return entity;
}

void DeadTreeArchetype::FitObstacle(entt::entity deadTree)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& dead = registry.Get<const DeadTree>(deadTree);
	const auto& info = Locator::infoConstants::value().tree.at(static_cast<size_t>(dead.type));
	const auto [point, radius] =
	    GetFixedObstacleBoundingCircle(dead.model.value_or(info.normal), registry.Get<const Transform>(deadTree));
	registry.AssignOrReplace<Fixed>(deadTree, point, radius);
}
