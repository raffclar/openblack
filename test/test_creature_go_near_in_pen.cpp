/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <vector>

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureCastMoves.h"
#include "Creature/CreatureObstacles.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/ObjectMeasures.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
constexpr float k_Tolerance = 1e-4f;
/// A grown creature's radius on its walks, and its radius as the pen shows it at a newborn's size
constexpr float k_OwnRadius = 3.0f;
constexpr float k_PenSize = 0.22f;
constexpr float k_ShownRadius = k_OwnRadius * k_PenSize;
/// The share of the creature's radius that a thing on the land with no height keeps clear of
constexpr float k_FlatThingShare = 0.7f;

/// A grown creature in its temple's pen, shown at a newborn's size
entt::entity ACreatureInItsPen(Registry& registry)
{
	const auto creature = registry.Create();
	registry.Assign<Transform>(creature, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(k_PenSize));
	auto& body = registry.Assign<Creature>(creature);
	body.size = 1.0f;
	body.penSize = k_PenSize;
	auto& walk = registry.Assign<CreatureLocomotion>(creature);
	walk.radius = k_OwnRadius;
	walk.shownRadius = k_ShownRadius;
	return creature;
}

/// A toy with no model, which has no radius and no height, nine metres away
entt::entity AToy(Registry& registry)
{
	const auto toy = registry.Create();
	registry.Assign<Transform>(toy, glm::vec3(9.0f, 0.0f, 0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	return toy;
}

/// The same toy as the routes of the creature see it
creature_obstacles::Thing AsObstacle()
{
	return {.avoidance = creature_obstacles::Avoidance::Object,
	        .shape = creature_obstacles::Shape::AtPlace,
	        .position = {9.0f, 0.0f},
	        .yAngle = 0.0f,
	        .scale = 1.0f,
	        .boxCentre = glm::vec3(0.0f),
	        .boxHalfSize = glm::vec3(0.0f),
	        .height = 0.0f,
	        .width = 0.0f,
	        .burning = false,
	        .hasFire = false,
	        .justBegun = false,
	        .walking = false,
	        .playerAlignment = 0.0f,
	        .worshipSites = {},
	        .owner = PlayerNames::NEUTRAL};
}
} // namespace

TEST(CreatureGoNearInPen, OnItsWalksTheCreatureKeepsClearOfThingsByItsOwnSize)
{
	Registry registry;
	const auto creature = ACreatureInItsPen(registry);
	EXPECT_FLOAT_EQ(object_measures::NavRadius(registry, creature), k_OwnRadius);
	// While it clears its way, its walks keep clear of things by its shown size
	registry.Get<CreatureLocomotion>(creature).radius = k_ShownRadius;
	EXPECT_FLOAT_EQ(object_measures::NavRadius(registry, creature), k_ShownRadius);
}

TEST(CreatureGoNearInPen, WhatItKeepsClearOfTakesAShareOfItsRadiusOnItsWalks)
{
	Registry registry;
	const auto creature = ACreatureInItsPen(registry);
	const auto toy = AToy(registry);
	EXPECT_NEAR(object_measures::RoutePlanRadius(registry, toy, creature), -k_FlatThingShare * k_OwnRadius, k_Tolerance);
}

TEST(CreatureGoNearInPen, TheWalkUpToAThingEndsOnTheEdgeOfTheCircleThatItsRouteGoesRound)
{
	Registry registry;
	const auto creature = ACreatureInItsPen(registry);
	const auto toy = AToy(registry);
	const creature_obstacles::Walker walker {.position = {0.0f, 0.0f},
	                                         .height = creature_cast_moves::k_HeightOfSizeOne * k_PenSize,
	                                         .radius = k_OwnRadius,
	                                         .scriptControlled = false,
	                                         .owner = PlayerNames::PLAYER_ONE};
	const auto circles = creature_obstacles::CirclesOf(AsObstacle(), walker);
	ASSERT_EQ(circles.size(), 1u);
	// The near edge of the walk's ring is the edge of the toy's circle, so a route to the ring exists
	const float near =
	    object_measures::RoutePlanRadius(registry, toy, creature) + object_measures::NavRadius(registry, creature);
	EXPECT_NEAR(near, circles.front().radius, k_Tolerance);
	// A ring measured by the shown size lies wholly in the circle, where no route can end
	EXPECT_LT((-k_FlatThingShare * k_ShownRadius) + k_ShownRadius, circles.front().radius);
}
