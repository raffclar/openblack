/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The route planner's world for a creature: a footpath's holder (no plan) gets the circles it always got, a creature's
// follower is told apart by its plan, a map shield is avoided unless it is the creature's own player's, and the
// follower's context and remaining length

#define LOCATOR_IMPLEMENTATIONS

#include <memory>

#include <gtest/gtest.h>

#include "3D/LandAvoid.h"
#include "3D/LandAvoidState.h"
#include "ECS/Components/MapShield.h"
#include "ECS/RoutePlanWorld.h"
#include "ECS/Systems/Implementations/MapCellsSystem.h"
#include "ECS/Systems/LandAvoidSystemInterface.h"
#include "RoutePlanner/ObstacleGrid.h"
#include "RoutePlanner/RouteFollower.h"
#include "creature/CreatureSystemWorld.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using openblack::route_planner::ObstacleGrid;
using openblack::route_planner::RouteFollower;

namespace
{
class RoutePlanWorldCreatureTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::mapCellsSystem::emplace<systems::MapCellsSystem>();
		// a 32 x 32 mask of land, one avoided cell and one cell of water a creature can walk in, both next to (5, 5)
		auto& state = Locator::landAvoidSystem::value().GetState();
		state.size = 32;
		state.avoid.assign(32 * 32, land_avoid::k_Land);
		state.avoid[static_cast<size_t>(5 * 32 + 6)] = land_avoid::k_Avoid;
		state.avoid[static_cast<size_t>(4 * 32 + 5)] = land_avoid::k_Water;
	}

	test::creature_world::World _world;
	const test::RestoreService<Locator::mapCellsSystem> _restoreCells;
};
} // namespace

TEST_F(RoutePlanWorldCreatureTest, AFootpathHolderGetsTheCirclesItAlwaysGot)
{
	// no plan: the avoided cell and the water cell each give a circle, as the footpaths have always had them
	auto footpath = std::make_unique<ObstacleGrid>();
	ASSERT_EQ(footpath->GetPlan(), nullptr);
	route_plan_world::CheckSquareFunction(5, 5, *footpath);
	ASSERT_EQ(footpath->ObjectCount(), 2);
	EXPECT_EQ(footpath->Object(0).id, -1);
	// x outer, z inner: the water cell (5, 4) first, then the avoided cell (6, 5); each at its cell's centre
	EXPECT_FLOAT_EQ(footpath->Object(0).c.x, 55.0f);
	EXPECT_FLOAT_EQ(footpath->Object(0).c.z, 45.0f);
	EXPECT_FLOAT_EQ(footpath->Object(1).c.x, 65.0f);
	EXPECT_FLOAT_EQ(footpath->Object(1).c.z, 55.0f);

	// a creature's follower is its own plan: the water it can walk in is no obstacle
	const auto creature = test::creature_world::World::MakeCreature();
	auto follower = std::make_unique<RouteFollower>();
	follower->Init(static_cast<int32_t>(entt::to_integral(creature)), {}, {}, {}, 1);
	route_plan_world::CheckSquareFunction(5, 5, *follower);
	ASSERT_EQ(follower->ObjectCount(), 1);
	EXPECT_FLOAT_EQ(follower->Object(0).c.x, 65.0f);
}

TEST_F(RoutePlanWorldCreatureTest, AnotherPlayersShieldIsAvoided)
{
	auto& registry = test::creature_world::World::Registry();
	const auto shield = registry.Create();
	// its spell is gone: the shield takes the interface's player, the first
	registry.Assign<MapShield>(shield);
	const auto mine = test::creature_world::World::MakeCreature(glm::vec3(100.0f), PlayerNames::PLAYER_ONE);
	const auto theirs = test::creature_world::World::MakeCreature(glm::vec3(100.0f), PlayerNames::PLAYER_TWO);

	// with no creature, as the footpaths ask, a shield is never in the way
	EXPECT_FALSE(route_plan_world::CreatureMustAvoid(shield));
	EXPECT_FALSE(route_plan_world::CreatureMustAvoid(shield, mine));
	EXPECT_TRUE(route_plan_world::CreatureMustAvoid(shield, theirs));
	// the other things are as with no creature
	EXPECT_FALSE(route_plan_world::CreatureMustAvoid(theirs, mine));
	EXPECT_EQ(route_plan_world::CreatureMustAvoid(theirs, mine), route_plan_world::CreatureMustAvoid(theirs));
	// something gone is never in the way
	const auto gone = registry.Create();
	registry.Destroy(gone);
	EXPECT_FALSE(route_plan_world::CreatureMustAvoid(gone, mine));
}

TEST(RouteFollowerCreature, ContextAndRemainingLength)
{
	ObstacleGrid::InstallCallbacks(nullptr, nullptr);
	auto follower = std::make_unique<RouteFollower>();
	follower->Init(42, {}, {}, {}, 64);
	follower->SetIdle();
	EXPECT_EQ(follower->GetContext(), 42);
	EXPECT_FLOAT_EQ(follower->RemainingLength(), 0.0f);

	const route_planner::Point2D start {100.0f, 100.0f};
	follower->SetPosition(start);
	follower->SetPlanStart(start);
	follower->SetDest({300.0f, 100.0f}, 0.0f, 1.0f, 0.5f, 1005.0f);
	for (int i = 0; i < 16 && follower->GetState() == RouteFollower::State::Planning; ++i)
	{
		follower->Update(nullptr, 0);
	}
	ASSERT_EQ(follower->GetState(), RouteFollower::State::Following);
	const auto whole = follower->RemainingLength();
	EXPECT_NEAR(whole, 200.0f, 1.0f);
	// 0.1 x 50 = 5 along it
	follower->SetSpeed(50.0f);
	follower->MoveAlongRoute();
	EXPECT_NEAR(follower->RemainingLength(), whole - 5.0f, 1e-3f);
}
