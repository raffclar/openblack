/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A creature among the physics objects: the lower speed at which what a creature lets go of is thrown, the heavy body
// it stands as for what is thrown at it, its weight, and what it lets go of going into the physics

#define LOCATOR_IMPLEMENTATIONS

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/Mobile.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreaturePhysics.h"
#include "ECS/MapCells.h"
#include "ECS/Physics/FromHand.h"
#include "ECS/Physics/PhysicsBody.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Systems/Implementations/CreatureObjectActionSystem.h"
#include "ECS/Systems/Implementations/MapCellsSystem.h"
#include "creature/CreatureSystemWorld.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
using openblack::ecs::creature_physics::Release;
using openblack::ecs::physics::PhysicsBody;
using openblack::ecs::physics::PhysicsObjects;
using openblack::ecs::physics::from_hand::IsThrown;
using openblack::ecs::systems::CreatureObjectActionSystem;

namespace
{
TEST(CreaturePhysicsRules, ACreatureThrowsAtALowerSpeedThanTheHand)
{
	// the speed across the ground squared: 1, 2.25 and 4
	EXPECT_FALSE(IsThrown(glm::vec3(1.0f, 0.0f, 0.0f), true));
	EXPECT_FALSE(IsThrown(glm::vec3(1.0f, 0.0f, 0.0f), false));
	EXPECT_TRUE(IsThrown(glm::vec3(1.5f, 0.0f, 0.0f), true));
	EXPECT_FALSE(IsThrown(glm::vec3(1.5f, 0.0f, 0.0f), false));
	EXPECT_TRUE(IsThrown(glm::vec3(0.0f, 0.0f, 2.0f), true));
	EXPECT_FALSE(IsThrown(glm::vec3(0.0f, 0.0f, 2.0f), false));
	EXPECT_TRUE(IsThrown(glm::vec3(2.0f, 0.0f, 0.5f), false));
	// up and down is not across the ground
	EXPECT_FALSE(IsThrown(glm::vec3(0.0f, 9.0f, 0.0f), true));
}

TEST(CreaturePhysicsRules, ACreaturesBodyIsAHeavyBallAsTallAsItThatFacesOut)
{
	const auto shape = ecs::creature_physics::ShapeOf(15.0f);
	EXPECT_FLOAT_EQ(shape.radius, 7.5f);
	EXPECT_EQ(shape.centre, glm::vec3(0.0f, 7.5f, 0.0f));
	EXPECT_FLOAT_EQ(shape.mass, 1000.0f);
	EXPECT_FALSE(shape.dynamic);
	ASSERT_EQ(shape.points.size(), 6u);
	ASSERT_EQ(shape.faces.size(), 8u);
	for (const auto& point : shape.points)
	{
		EXPECT_FLOAT_EQ(glm::length(point), 7.5f);
	}
	for (const auto& face : shape.faces)
	{
		const auto& a = shape.points.at(face[0]);
		const auto& b = shape.points.at(face[1]);
		const auto& c = shape.points.at(face[2]);
		EXPECT_GT(glm::dot(glm::cross(b - a, c - a), a + b + c), 0.0f);
	}
}

TEST(CreaturePhysicsRules, OnlyACreatureCanThrowAtATarget)
{
	const auto creature = static_cast<entt::entity>(5);
	EXPECT_EQ(ecs::creature_physics::ReleaseOf(true, creature), Release::AtTarget);
	EXPECT_EQ(ecs::creature_physics::ReleaseOf(false, creature), Release::LetGo);
	EXPECT_EQ(ecs::creature_physics::ReleaseOf(true, entt::null), Release::LetGo);
}

class CreaturePhysicsTest: public ::testing::Test
{
protected:
	void SetUp() override { test::EmplaceMapAndVillagerDefaults(); }
	void TearDown() override { test::ResetMapAndVillagerDefaults(); }

	// the world lists a thing put down reaches (the map cells, the fires, the animals' shared state...), reset after
	// the registry as the game's shutdown does
	const test::ScopedWorldSystems _worldSystems;
	test::creature_world::World _world;
};

TEST_F(CreaturePhysicsTest, ACreatureStandsAsItsBodyWhereItIs)
{
	const glm::vec3 at(100.0f, 2.0f, 50.0f);
	const auto creature = test::creature_world::World::MakeCreature(at);
	PhysicsBody body;
	ASSERT_TRUE(ecs::creature_physics::SetUpBody(creature, body));
	EXPECT_FLOAT_EQ(body.Mass(), 1000.0f);
	EXPECT_FLOAT_EQ(body.Radius(), 7.5f);
	EXPECT_NEAR(glm::distance(body.Centre(), at + glm::vec3(0.0f, 7.5f, 0.0f)), 0.0f, 1e-4f);
	EXPECT_EQ(body.Vertices().size(), 6u);

	auto& registry = test::creature_world::World::Registry();
	const auto rock = registry.Create();
	registry.Assign<Transform>(rock, at, glm::mat3(1.0f), glm::vec3(1.0f));
	PhysicsBody none;
	EXPECT_FALSE(ecs::creature_physics::SetUpBody(rock, none));
}

TEST_F(CreaturePhysicsTest, OnceRegisteredThePhysicsBuildsAndWeighsACreatureAsOne)
{
	const auto creature = test::creature_world::World::MakeCreature();
	ecs::creature_physics::RegisterPhysicsHandlers();
	EXPECT_FLOAT_EQ(PhysicsObjects::Weight(creature), 1000.0f);
	PhysicsBody body;
	ASSERT_TRUE(PhysicsObjects::SetUpBody(creature, body, false));
	EXPECT_FLOAT_EQ(body.Mass(), 1000.0f);
	// it never flies itself
	EXPECT_FALSE(PhysicsObjects::CanBecomeAPhysicsObject(creature));
}

TEST_F(CreaturePhysicsTest, WhatACreatureThrowsAtATargetIsPutDownWhereItIsWhenNoBodyCanBeMadeForIt)
{
	const auto creature = test::creature_world::World::MakeCreature();
	auto& registry = test::creature_world::World::Registry();
	const auto rock = registry.Create();
	registry.Assign<Transform>(rock, glm::vec3(110.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<MobileObject>(rock, MobileObjectInfo::Ball);
	_world.teleported.clear();

	// the rock has no mesh, so the physics can make it no body
	CreatureObjectActionSystem::ReleaseHeld(rock, glm::vec3(120.0f, 0.0f, 90.0f), glm::vec3(30.0f, 5.0f, 0.0f), creature, true);

	ASSERT_EQ(_world.teleported.size(), 1u);
	EXPECT_EQ(_world.teleported.front(), rock);
	EXPECT_EQ(PhysicsObjects::Find(rock), nullptr);
	EXPECT_EQ(registry.Get<Transform>(rock).position, glm::vec3(120.0f, 0.0f, 90.0f));
	EXPECT_TRUE(ecs::map_cells::IsObjectInMap(rock));
}
} // namespace
