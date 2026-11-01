/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScaffoldArchetype.h"

#include <glm/vec3.hpp>

#include "Common/X87Angle.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/Transform.h"
#include "ECS/PlacedTurn.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Utils.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

entt::entity ScaffoldArchetype::Create(const glm::vec3& position, entt::entity town, entt::entity workshop, float yAngleRadians,
                                       float scale)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	// It takes its place in the order of the things made
	TakeThingMadePlace();
	const auto& info = Locator::infoConstants::value().scaffold;

	registry.Assign<Transform>(entity, position, x87::GameTurnMatrix(0.0f, yAngleRadians, 0.0f), glm::vec3(scale));
	registry.Assign<Mobile>(entity);
	registry.Assign<Mesh>(entity, resources::HashIdentifier(info.meshId), static_cast<int8_t>(0), static_cast<int8_t>(1));
	registry.Assign<Scaffold>(entity, Scaffold {.town = town, .workshop = workshop});

	// The thing keeps the angles it got
	ecs::placed_turn::KeepAngles(registry, entity, glm::vec3(0.0f, yAngleRadians, 0.0f));
	return entity;
}
