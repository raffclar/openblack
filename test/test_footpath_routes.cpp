/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Footpaths made by the route planner: ConvertCreaturePlanToFootpath between two nodes, on a plan
// round one hand-made circle (no world).

#define LOCATOR_IMPLEMENTATIONS

#include <memory>

#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/Components/Footpath.h"
#include "ECS/Footpaths.h"
#include "ECS/Registry.h"
#include "ECS/RoutePlanWorld.h"
#include "Locator.h"
#include "RoutePlanner/ObstacleGrid.h"
#include "RoutePlanner/RoutePlan.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace footpaths = openblack::ecs::footpaths;
namespace route_plan_world = openblack::ecs::route_plan_world;

namespace
{
class FootpathRoutesTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		route_planner::ObstacleGrid::InstallCallbacks(nullptr, nullptr);
	}
	void TearDown() override
	{
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
	}
	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }
};

TEST_F(FootpathRoutesTest, ConvertsTheRouteRoundACircleBetweenTwoNodes)
{
	auto holder = std::make_unique<route_planner::ObstacleGrid>();
	holder->AddObject(7, {200.0f, 100.0f}, 10.0f, 0);
	route_planner::RoutePlan plan;
	const route_planner::Point2D start {100.0f, 100.0f};
	const route_planner::Point2D dest {300.0f, 100.0f};
	plan.SetStart(start, 0.5f, *holder, -1, -1, 0);
	plan.SetDest(dest, 0.0f, 0.0f, 0.0f, -1, 0, (start.DistanceTo(dest) + 1.0f) * 5.0f, true);
	for (int i = 0; i < 128 && plan.GetState() == route_planner::RoutePlan::State::Searching; ++i)
	{
		plan.StepSearch(0);
	}
	ASSERT_EQ(plan.GetState(), route_planner::RoutePlan::State::Found);

	// a footpath with its two ends, as GenerateRoutesBetween makes it (the head first)
	const auto e = Reg().Create();
	auto& f = Reg().Assign<Footpath>(e);
	const auto headCoords = route_plan_world::ToMapCoords(start);
	const auto tailCoords = route_plan_world::ToMapCoords(dest);
	f.nodes.push_back({glm::vec3(0.0f), headCoords, 1, f.nextNodeId++});
	f.nodes.push_back({glm::vec3(0.0f), tailCoords, 1, f.nextNodeId++});
	footpaths::ConvertCreaturePlanToFootpath(*holder, plan, e, 0, 1, nullptr);

	const auto& nodes = Reg().Get<Footpath>(e).nodes;
	ASSERT_GT(nodes.size(), 4u); // the ends, the plan's start and dest, and the arc's steps
	EXPECT_EQ(nodes.front().id, 0);
	EXPECT_EQ(nodes.back().id, 1);
	// every new node stays out of the circle, has bits 0 and 1 and a new id
	for (size_t i = 1; i + 1 < nodes.size(); ++i)
	{
		const auto p = route_plan_world::ToPoint2D(nodes.at(i).coords);
		EXPECT_GE(p.DistanceTo({200.0f, 100.0f}), 10.0f - 0.01f);
		EXPECT_EQ(nodes.at(i).flags, 3);
		EXPECT_GE(nodes.at(i).id, 2);
	}
}

TEST_F(FootpathRoutesTest, KeepsTheHiddenNodesBetweenAfterTheNewOnes)
{
	auto holder = std::make_unique<route_planner::ObstacleGrid>();
	route_planner::RoutePlan plan;
	const route_planner::Point2D start {100.0f, 100.0f};
	const route_planner::Point2D dest {200.0f, 100.0f};
	plan.SetStart(start, 0.5f, *holder, -1, -1, 0);
	plan.SetDest(dest, 0.0f, 0.0f, 0.0f, -1, 0, 1000.0f, true);
	ASSERT_EQ(plan.GetState(), route_planner::RoutePlan::State::Found);

	const auto e = Reg().Create();
	auto& f = Reg().Assign<Footpath>(e);
	f.nodes.push_back({glm::vec3(0.0f), route_plan_world::ToMapCoords(start), 1, f.nextNodeId++});
	f.nodes.push_back({glm::vec3(0.0f), route_plan_world::ToMapCoords({150.0f, 120.0f}), 1, f.nextNodeId++});
	f.nodes.push_back({glm::vec3(0.0f), route_plan_world::ToMapCoords({160.0f, 120.0f}), 4, f.nextNodeId++});
	f.nodes.push_back({glm::vec3(0.0f), route_plan_world::ToMapCoords(dest), 1, f.nextNodeId++});
	footpaths::ConvertCreaturePlanToFootpath(*holder, plan, e, 0, 3, nullptr);

	// EraseNodesBetween: node 1 deleted, node 2 (hidden) kept, chained to b after the new ones
	const auto& nodes = Reg().Get<Footpath>(e).nodes;
	ASSERT_EQ(nodes.size(), 5u); // a, the plan's start, its dest, the hidden one, b
	EXPECT_EQ(nodes.at(0).id, 0);
	EXPECT_EQ(nodes.at(3).id, 2);
	EXPECT_EQ(nodes.at(4).id, 3);
}
} // namespace
