/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DeadTreeArchetype.h"

#include <glm/gtx/euler_angles.hpp>

#include "ECS/Components/DeadTree.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Unlit.h"
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
	const auto entity = Create(position, type, glm::mat3(glm::eulerAngleY(-yAngleRadians)), scale, info.normal);
	Locator::entitiesRegistry::value().Get<DeadTree>(entity).model.reset();
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
