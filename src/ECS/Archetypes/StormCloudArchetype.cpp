/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "StormCloudArchetype.h"

#include <glm/mat3x3.hpp>

#include "3D/Mists.h"
#include "ECS/Components/Mist.h"
#include "ECS/Components/StormCloud.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

entt::entity StormCloudArchetype::Create(entt::entity storm, uint8_t index, float cloudHeight,
                                         const storm_clouds::Random& random)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	// The mist's animation takes its number first, then the cloud its shape and place
	const auto counter = mists::StartCounter(random(0.0f, 16.0f));
	const auto cloud = storm_clouds::MakeCloud(random, cloudHeight);
	// It is placed and coloured as it first steps, unseen until then
	registry.Assign<Transform>(entity, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Mist>(entity, Mist {
	                                  .size = 1.0f,
	                                  .colour = 0,
	                                  .shrinksEdgeOn = true,
	                                  .edgeShrink = cloud.edgeShrink,
	                                  .counter = counter,
	                                  .counterRemainder = 0.0f,
	                              });
	registry.Assign<StormCloud>(entity, StormCloud {
	                                        .storm = storm,
	                                        .index = index,
	                                        .drift = cloud.drift,
	                                        .sizeFactor = cloud.sizeFactor,
	                                        .shade = 0.0f,
	                                        .flashGrey = 0,
	                                    });
	return entity;
}
