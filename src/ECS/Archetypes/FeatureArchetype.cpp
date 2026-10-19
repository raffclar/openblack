/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FeatureArchetype.h"

#include "3D/L3DMesh.h"
#include "Common/X87Angle.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/PlacedTurn.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"
#include "Utils.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

entt::entity FeatureArchetype::Create(const glm::vec3& position, FeatureInfo type, float yAngleRadians, float scale)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();

	const auto& info = Locator::infoConstants::value().feature.at(static_cast<size_t>(type));

	const auto& transform =
	    registry.Assign<Transform>(entity, position, x87::GameTurnMatrix(0.0f, yAngleRadians, 0.0f), glm::vec3(scale));
	const auto [point, radius] = GetFixedObstacleBoundingCircle(info.meshId, transform);
	registry.Assign<Fixed>(entity, point, radius);
	registry.Assign<Feature>(entity, type);
	const auto resourceId = resources::HashIdentifier(info.meshId);
	registry.Assign<Mesh>(entity, resourceId, static_cast<int8_t>(0), static_cast<int8_t>(1));

	// The thing keeps the angles it got
	ecs::placed_turn::KeepAngles(Locator::entitiesRegistry::value(), entity, glm::vec3(0.0f, yAngleRadians, 0.0f));
	return entity;
}
