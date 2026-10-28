/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BigForestArchetype.h"

#include <glm/gtx/euler_angles.hpp>

#include "Common/PortableRotations.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Unlit.h"
#include "ECS/PlacedTurn.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ForestSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Nature/BigForestRules.h"
#include "Resources/ResourceManager.h"
#include "Utils.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

entt::entity BigForestArchetype::Create(const glm::vec3& position, BigForestInfo type, [[maybe_unused]] uint32_t unknown,
                                        float yAngleRadians, float scale)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	// It takes its place in the order of the things made
	TakeThingMadePlace();

	const auto& info = Locator::infoConstants::value().bigForest.at(static_cast<size_t>(type));

	const auto& transform =
	    registry.Assign<Transform>(entity, position, portable_math::EulerAngleY(-yAngleRadians), glm::vec3(scale));
	const auto [point, radius] = GetFixedObstacleBoundingCircle(info.meshId, transform);
	registry.Assign<Fixed>(entity, point, radius);
	registry.Assign<Forest>(entity);
	registry.Assign<BigForest>(
	    entity, BigForest {.type = static_cast<int>(type), .wood = big_forest::StartWood(scale, info.woodValue)});
	registry.Assign<MorphWithTerrain>(entity);
	registry.Assign<Unlit>(entity);
	const auto resourceId = resources::HashIdentifier(info.meshId);
	registry.Assign<Mesh>(entity, resourceId, static_cast<int8_t>(0), static_cast<int8_t>(1));
	// A big forest is the forest of its own trees, made about its place
	if (Locator::forestSystem::has_value())
	{
		Locator::forestSystem::value().MakeLandForest(std::nullopt, position, entity, false);
	}

	// The thing keeps the angles it got
	ecs::placed_turn::KeepAngles(Locator::entitiesRegistry::value(), entity, glm::vec3(0.0f, yAngleRadians, 0.0f));
	return entity;
}
