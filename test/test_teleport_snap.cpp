/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// ecs::SnapTurnStart: a thing put somewhere outside its own turn takes its new position as its position at the start
// of the turn, so the draw between the turns does not slide it from where it was

#include <glm/glm.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/DrawPosition.h"
#include "ECS/Components/Shark.h"
#include "ECS/Components/Transform.h"
#include "ECS/MobileDrawing.h"
#include "ECS/Registry.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

TEST(TeleportSnap, DrawPositionTakesTheNewPosition)
{
	Registry registry;
	const auto entity = registry.Create();
	auto& transform = registry.Assign<Transform>(entity, glm::vec3(1.0f, 2.0f, 3.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<DrawPosition>(entity).turnStart = glm::vec3(40.0f, 2.0f, 50.0f);
	transform.position = glm::vec3(7.0f, 8.0f, 9.0f);

	SnapTurnStart(registry, entity);
	EXPECT_EQ(registry.Get<DrawPosition>(entity).turnStart, glm::vec3(7.0f, 8.0f, 9.0f));
}

TEST(TeleportSnap, SharkTakesTheNewPosition)
{
	Registry registry;
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, glm::vec3(5.0f, 0.0f, 6.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Shark>(entity).turnStart = glm::vec3(0.0f);

	SnapTurnStart(registry, entity);
	EXPECT_EQ(registry.Get<Shark>(entity).turnStart, glm::vec3(5.0f, 0.0f, 6.0f));
}

TEST(TeleportSnap, ThingsWithoutTurnStartAreLeftAlone)
{
	Registry registry;
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, glm::vec3(1.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	SnapTurnStart(registry, entity); // no DrawPosition, no Shark: nothing to do
	EXPECT_FALSE((registry.AnyOf<DrawPosition, Shark>(entity)));

	const auto bare = registry.Create(); // no Transform at all
	SnapTurnStart(registry, bare);
	EXPECT_FALSE((registry.AnyOf<DrawPosition, Shark>(bare)));
}
