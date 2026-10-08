/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The route planner: ObstacleGrid::AddObject, GetFirstObject, StartObstacleSidePoint, and the
// RoutePlan search (SetDest, StepSearch). Hand-made circles, no world (no fill callback).

#include <cmath>

#include <memory>

#include <gtest/gtest.h>

#include "RoutePlanner/ObstacleGrid.h"
#include "RoutePlanner/RoutePlan.h"

using namespace openblack::route_planner;

namespace
{
class RoutePlanTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		ObstacleGrid::InstallCallbacks(nullptr, nullptr);
		_holder = std::make_unique<ObstacleGrid>();
	}

	/// The footpaths' use (GenerateRoutesBetween): SetStart(start, 0.5, holder, -1, -1, 0), SetDest(dest, 0,
	/// 0, 0, -1, 0, (range + 1) x 5, 1), StepSearch(0) while searching, at most 128 times
	RoutePlan::State Plan(RoutePlan& plan, Point2D start, Point2D dest, float maxLength = -1.0f)
	{
		plan.SetStart(start, 0.5f, *_holder, -1, -1, 0);
		const float limit = maxLength >= 0.0f ? maxLength : (start.DistanceTo(dest) + 1.0f) * 5.0f;
		plan.SetDest(dest, 0.0f, 0.0f, 0.0f, -1, 0, limit, true);
		for (int i = 0; i < 128 && plan.GetState() == RoutePlan::State::Searching; ++i)
		{
			plan.StepSearch(0);
		}
		return plan.GetState();
	}

	std::unique_ptr<ObstacleGrid> _holder;
};

TEST_F(RoutePlanTest, AddObjectKeepsTheEndsOutAndMergesCircles)
{
	_holder->AddObject(1, {200.0f, 200.0f}, 10.0f, 0);
	// inside the existing one: not added
	_holder->AddObject(2, {201.0f, 200.0f}, 5.0f, 0);
	EXPECT_EQ(_holder->ObjectCount(), 1);
	// swallows the existing one: added, the old one inactive
	_holder->AddObject(3, {200.0f, 200.0f}, 20.0f, 0);
	ASSERT_EQ(_holder->ObjectCount(), 2);
	EXPECT_EQ(_holder->Object(0).active, 0);
	EXPECT_EQ(_holder->Object(1).active, 1);
	// two anonymous at the same centre: the second not added
	_holder->AddObject(-1, {400.0f, 400.0f}, 7.1f, 0);
	_holder->AddObject(-1, {400.0f, 400.0f}, 7.1f, 0);
	EXPECT_EQ(_holder->ObjectCount(), 3);
	// a dest strictly inside: not added
	const Point2D dest {600.0f, 600.0f};
	_holder->SetEnds(&dest, nullptr);
	_holder->AddObject(4, {602.0f, 600.0f}, 5.0f, 0);
	EXPECT_EQ(_holder->ObjectCount(), 3);
	// off the grid (square 64): not added
	_holder->SetEnds(nullptr, nullptr);
	_holder->AddObject(5, {5115.0f, 100.0f}, 10.0f, 0);
	EXPECT_EQ(_holder->ObjectCount(), 3);
}

TEST_F(RoutePlanTest, GetFirstObjectHitAndSide)
{
	_holder->AddObject(7, {200.0f, 105.0f}, 10.0f, 0);
	Point2D to {300.0f, 100.0f};
	Point2D hit;
	int32_t side = 0;
	EXPECT_EQ(_holder->GetFirstObject({100.0f, 100.0f}, to, -1, hit, side, 0.0f), 0);
	// t = 100, b = -200, disc = 40000 - 4 (10025 - 100) = 300: s = (sqrt(300) - 200) x -0.5
	EXPECT_NEAR(hit.x, 100.0f + (200.0f - std::sqrt(300.0f)) * 0.5f, 1e-3f);
	EXPECT_FLOAT_EQ(hit.z, 100.0f);
	EXPECT_EQ(side, 2); // the centre on the left of the way (q > 0)
	// ignored: nothing, hit = from
	EXPECT_EQ(_holder->GetFirstObject({100.0f, 100.0f}, to, 0, hit, side, 0.0f), -1);
	EXPECT_FLOAT_EQ(hit.x, 100.0f);
	// square 0 counts as off the grid
	Point2D near0 {70.0f, 100.0f};
	EXPECT_EQ(_holder->GetFirstObject({10.0f, 100.0f}, near0, -1, hit, side, 0.0f), -1);
}

TEST_F(RoutePlanTest, SidePointIsTheTangentPoint)
{
	_holder->AddObject(7, {200.0f, 100.0f}, 10.0f, 0);
	Point2D out;
	EXPECT_EQ(_holder->StartObstacleSidePoint(0, {100.0f, 100.0f}, out, 2), 1);
	const float t = 100.0f / 9900.0f;
	const float k = 1.0f / (t + 1.0f);
	const float m = std::sqrt(t) * k;
	EXPECT_NEAR(out.x, 100.0f * k + 100.0f, 1e-3f);
	EXPECT_NEAR(out.z, 100.0f * m + 100.0f, 1e-3f);
	// on the circle
	EXPECT_NEAR(out.DistanceTo({200.0f, 100.0f}), 10.0f, 1e-3f);
	// the other side mirrors it
	Point2D other;
	EXPECT_EQ(_holder->StartObstacleSidePoint(0, {100.0f, 100.0f}, other, 1), 1);
	EXPECT_NEAR(other.z, 200.0f - out.z, 1e-3f);
	// from inside: pushed out to the circle, 0
	EXPECT_EQ(_holder->StartObstacleSidePoint(0, {203.0f, 100.0f}, out, 2), 0);
	EXPECT_NEAR(out.x, 210.0f, 1e-4f);
}

TEST_F(RoutePlanTest, StraightWhenNothingIsInTheWay)
{
	RoutePlan plan;
	EXPECT_EQ(Plan(plan, {100.0f, 100.0f}, {300.0f, 100.0f}), RoutePlan::State::Found);
	const auto& route = plan.GetRoute(plan.GetBestRoute());
	EXPECT_EQ(route.first, route.last);
	EXPECT_FLOAT_EQ(plan.GetNode(route.first).cost, 200.0f);
}

TEST_F(RoutePlanTest, GoesRoundAnObstacleAndStaysOutOfIt)
{
	_holder->AddObject(7, {200.0f, 100.0f}, 10.0f, 0);
	RoutePlan plan;
	ASSERT_EQ(Plan(plan, {100.0f, 100.0f}, {300.0f, 100.0f}), RoutePlan::State::Found);
	const auto& route = plan.GetRoute(plan.GetBestRoute());
	const float length = plan.GetNode(route.last).cost;
	EXPECT_GT(length, 200.0f);
	EXPECT_LT(length, 200.0f + 3.1416f * 10.0f);
	for (auto at = route.first; at != k_None; at = plan.GetNode(at).child)
	{
		EXPECT_GE(plan.GetNode(at).to.DistanceTo({200.0f, 100.0f}), 10.0f - 1e-2f);
	}
	// deterministic: the same holder state gives the same route
	ObstacleGrid::InstallCallbacks(nullptr, nullptr);
	auto again = std::make_unique<ObstacleGrid>();
	again->AddObject(7, {200.0f, 100.0f}, 10.0f, 0);
	std::swap(_holder, again);
	RoutePlan second;
	ASSERT_EQ(Plan(second, {100.0f, 100.0f}, {300.0f, 100.0f}), RoutePlan::State::Found);
	EXPECT_FLOAT_EQ(second.GetNode(second.GetRoute(second.GetBestRoute()).last).cost, length);
}

TEST_F(RoutePlanTest, DestInsideAnObstacle)
{
	_holder->AddObject(7, {300.0f, 100.0f}, 10.0f, 0);
	RoutePlan plan;
	EXPECT_EQ(Plan(plan, {100.0f, 100.0f}, {302.0f, 100.0f}), RoutePlan::State::DestInside);
}

TEST_F(RoutePlanTest, GoesRoundTwoOverlappingCircles)
{
	_holder->AddObject(7, {200.0f, 100.0f}, 15.0f, 0);
	_holder->AddObject(8, {220.0f, 103.0f}, 15.0f, 0);
	RoutePlan plan;
	ASSERT_EQ(Plan(plan, {100.0f, 100.0f}, {350.0f, 100.0f}), RoutePlan::State::Found);
	const auto& route = plan.GetRoute(plan.GetBestRoute());
	for (auto at = route.first; at != k_None; at = plan.GetNode(at).child)
	{
		const auto& to = plan.GetNode(at).to;
		EXPECT_GE(to.DistanceTo({200.0f, 100.0f}), 15.0f - 1e-2f);
		EXPECT_GE(to.DistanceTo({220.0f, 103.0f}), 15.0f - 1e-2f);
	}
	EXPECT_LT(plan.GetNode(route.last).cost, 250.0f + 3.1416f * 25.0f);
}
} // namespace
