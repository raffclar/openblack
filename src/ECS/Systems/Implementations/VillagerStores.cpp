/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "VillagerStores.h"

#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Trees.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::systems;

town_stores::TemporaryStore VillagerStores::GetTemporaryStore(entt::entity town, const map_coords::MapCoords& from,
                                                              ResourceType type)
{
	return town_stores::GetTemporaryResourceStorePotOrPos(town, from, type);
}

void VillagerStores::MakeDroppedLog(glm::vec3 position, uint32_t mesh, float multiplier, glm::vec3 velocity, glm::vec3 angular,
                                    std::optional<glm::vec3> momentum)
{
	const auto entity = CreateDroppedLog(position, mesh, multiplier);
	physics::PhysicsObjects::AddDroppedObject(entity, velocity, angular, momentum);
}
