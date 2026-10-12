/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <algorithm>
#include <numbers>
#include <optional>
#include <vector>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureRoute.h"
#include "Creature/CreatureRouteFollow.h"
#include "Creature/CreatureRouteSearch.h"

using namespace openblack;
using namespace openblack::creature_route;

namespace
{
constexpr float k_Tolerance = 1e-3f;
constexpr float k_Pi = std::numbers::pi_v<float>;

std::optional<bool> Dry(int32_t /*x*/, int32_t /*z*/)
{
	return false;
}

/// Searches until it ends, or gives up counting
Search::Status Finish(Search& search, Obstacles& obstacles, int32_t mostSteps = 20000)
{
	for (int32_t step = 0; step < mostSteps && search.GetStatus() == Search::Status::Searching; ++step)
	{
		search.Step(obstacles);
	}
	return search.GetStatus();
}

/// A route's legs with their circles, followed from the start
Route Followed(std::vector<Leg> legs, const Obstacles& obstacles)
{
	SetRounds(legs, obstacles);
	return {.legs = std::move(legs), .segment = 0, .travelled = 0.0f};
}

float LengthOfRoute(const Route& route)
{
	float length = 0.0f;
	for (const auto& leg : route.legs)
	{
		length += LengthOf(leg);
	}
	return length;
}

/// Whether every point along a route keeps out of every circle
bool ClearOf(const Route& route, const Obstacles& obstacles)
{
	const auto points = route.Points(k_Pi / 90.0f);
	for (size_t i = 1; i < points.size(); ++i)
	{
		for (const auto& obstacle : obstacles.All())
		{
			if (obstacle.active &&
			    DistanceToSegment(obstacle.circle.centre, points[i - 1], points[i]) < obstacle.circle.radius - 0.05f)
			{
				return false;
			}
		}
	}
	return true;
}

Obstacles Empty(glm::vec2 walker)
{
	Obstacles obstacles;
	obstacles.Reset(walker, 2.0f);
	return obstacles;
}
} // namespace

TEST(CreatureRouteSearch, TangentsTouchTheirCircles)
{
	const Circle circle {.centre = {0.0f, 0.0f}, .radius = 5.0f};
	const glm::vec2 from {20.0f, 0.0f};
	for (const auto way : {Way::Clockwise, Way::Anticlockwise})
	{
		const auto side = SidePointFrom(circle, from, way);
		EXPECT_TRUE(side.tangent);
		EXPECT_NEAR(glm::length(side.point), 5.0f, k_Tolerance);
		// The line from the point meets the radius at a right angle
		EXPECT_NEAR(glm::dot(side.point - from, side.point), 0.0f, 1e-2f);
	}
	// Going anticlockwise round it towards the point, it leaves on the side away from the left
	EXPECT_LT(SidePointFrom(circle, from, Way::Anticlockwise).point.y, 0.0f);
	EXPECT_GT(SidePointFrom(circle, from, Way::Clockwise).point.y, 0.0f);
	// From inside, the nearest point of its edge
	const auto inside = SidePointFrom(circle, {2.0f, 0.0f}, Way::Clockwise);
	EXPECT_FALSE(inside.tangent);
	EXPECT_NEAR(inside.point.x, 5.0f, k_Tolerance);

	// The same way round two circles of a size: a straight line alongside both; the other way, a line crossing between
	const Circle other {.centre = {30.0f, 0.0f}, .radius = 5.0f};
	const auto outer = TangentBetween(circle, Way::Clockwise, other, Way::Clockwise);
	ASSERT_TRUE(outer.has_value());
	EXPECT_NEAR(outer->leaving.y, 5.0f, k_Tolerance);
	EXPECT_NEAR(outer->arriving.y, 5.0f, k_Tolerance);
	const auto crossing = TangentBetween(circle, Way::Clockwise, other, Way::Anticlockwise);
	ASSERT_TRUE(crossing.has_value());
	EXPECT_NEAR(glm::length(crossing->leaving), 5.0f, k_Tolerance);
	EXPECT_NEAR(glm::distance(crossing->arriving, other.centre), 5.0f, k_Tolerance);
	EXPECT_GT(crossing->leaving.y, 0.0f);
	EXPECT_LT(crossing->arriving.y, 0.0f);
	EXPECT_NEAR(glm::dot(crossing->arriving - crossing->leaving, crossing->leaving), 0.0f, 1e-2f);
	// Circles too close for a crossing tangent have none
	EXPECT_FALSE(TangentBetween(circle, Way::Clockwise, {.centre = {8.0f, 0.0f}, .radius = 5.0f}, Way::Anticlockwise));

	// A quarter of the way round, either way
	EXPECT_NEAR(ArcLength(circle, Way::Anticlockwise, {5, 0}, {0, 5}), 2.5f * k_Pi, k_Tolerance);
	EXPECT_NEAR(ArcLength(circle, Way::Clockwise, {0, 5}, {5, 0}), 2.5f * k_Pi, k_Tolerance);
}

TEST(CreatureRouteSearch, CirclesAreKeptOnlyWhereTheyAddSomething)
{
	auto obstacles = Empty({1000.0f, 1000.0f});
	EXPECT_TRUE(obstacles.Add(7u, {1100.0f, 1000.0f}, 10.0f));
	// Inside one already kept: left out
	EXPECT_FALSE(obstacles.Add(8u, {1102.0f, 1000.0f}, 5.0f));
	// Covering one already kept: that one is put out of use
	EXPECT_TRUE(obstacles.Add(9u, {1101.0f, 1000.0f}, 20.0f));
	ASSERT_EQ(obstacles.All().size(), 2u);
	EXPECT_FALSE(obstacles.At(0).active);
	EXPECT_TRUE(obstacles.BlockHas(9u, {110, 100}));
	EXPECT_FALSE(obstacles.BlockHas(8u, {110, 100}));
	// The land's circles are kept once at each centre
	EXPECT_TRUE(obstacles.Add(std::nullopt, {1500.0f, 1500.0f}, 7.1f));
	EXPECT_FALSE(obstacles.Add(std::nullopt, {1500.0f, 1500.0f}, 7.1f));
	// Off the land's blocks: left out
	EXPECT_FALSE(obstacles.Add(10u, {5200.0f, 1000.0f}, 10.0f));

	// The land's blocked cells round a cell are circles at their middles
	const auto land = WalkableLand::Build([](int32_t x, int32_t z) { return (x == 151 && z == 151) ? 40.0f : 5.0f; }, Dry);
	ASSERT_EQ(land.At(150, 150), Ground::Blocked);
	auto landObstacles = Empty({1000.0f, 1000.0f});
	AddLandCircles(land, {150, 150}, landObstacles);
	AddLandCircles(land, {149, 150}, landObstacles);
	// The raised corner makes the four cells about it too steep; the second cell's neighbours add none twice
	ASSERT_EQ(landObstacles.All().size(), 4u);
	EXPECT_TRUE(std::ranges::any_of(landObstacles.All(),
	                                [](const Obstacle& o) { return o.circle.centre == glm::vec2(1505.0f, 1505.0f); }));
	EXPECT_TRUE(std::ranges::all_of(landObstacles.All(), [](const Obstacle& o) {
		return !o.thing.has_value() && std::abs(o.circle.radius - k_DestinationClearance) < k_Tolerance;
	}));
}

TEST(CreatureRouteSearch, ALineMeetsTheNearestCircleOnItsWay)
{
	auto obstacles = Empty({1000.0f, 1000.0f});
	obstacles.Add(1u, {1060.0f, 1003.0f}, 5.0f);
	obstacles.Add(2u, {1030.0f, 997.0f}, 5.0f);
	glm::vec2 to {1100.0f, 1000.0f};
	const auto hit = obstacles.FirstInWay({1000.0f, 1000.0f}, to, std::nullopt, 0.0f);
	ASSERT_TRUE(hit.has_value());
	EXPECT_EQ(obstacles.At(hit->obstacle).thing, 2u);
	EXPECT_NEAR(glm::distance(hit->point, obstacles.At(hit->obstacle).circle.centre), 5.0f, k_Tolerance);
	// Its middle is to the right of the line: it is gone round clockwise
	EXPECT_EQ(hit->way, Way::Clockwise);
	// Ignoring it, the other is met, gone round the other way
	const auto next = obstacles.FirstInWay({1000.0f, 1000.0f}, to, hit->obstacle, 0.0f);
	ASSERT_TRUE(next.has_value());
	EXPECT_EQ(obstacles.At(next->obstacle).thing, 1u);
	EXPECT_EQ(next->way, Way::Anticlockwise);
	// Stopping short of the end, the end comes nearer and what lies beyond is not met
	glm::vec2 shortEnd {1100.0f, 1000.0f};
	EXPECT_FALSE(obstacles.FirstInWay({1000.0f, 1000.0f}, shortEnd, hit->obstacle, 50.0f).has_value());
	EXPECT_NEAR(shortEnd.x, 1050.0f - 0.005f, k_Tolerance);
}

TEST(CreatureRouteSearch, GoingRoundACircleStopsWhereItRunsIntoAnother)
{
	auto obstacles = Empty({900.0f, 900.0f});
	obstacles.Add(1u, {1000.0f, 1000.0f}, 10.0f);
	obstacles.Add(2u, {1015.0f, 1000.0f}, 8.0f);
	// Anticlockwise from the bottom to the top, it passes the other circle's side
	glm::vec2 leaving {1000.0f, 1010.0f};
	const auto into = obstacles.RunsInto(0, Way::Anticlockwise, leaving, {1000.0f, 990.0f});
	ASSERT_TRUE(into.has_value());
	EXPECT_EQ(*into, 1u);
	EXPECT_NEAR(glm::distance(leaving, glm::vec2(1000.0f, 1000.0f)), 10.0f, 1e-2f);
	EXPECT_NEAR(glm::distance(leaving, glm::vec2(1015.0f, 1000.0f)), 8.0f, 1e-2f);
	EXPECT_LT(leaving.y, 1000.0f);
	// Clockwise the same way round, it runs into it on the other side
	glm::vec2 back {1000.0f, 990.0f};
	ASSERT_TRUE(obstacles.RunsInto(0, Way::Clockwise, back, {1000.0f, 1010.0f}).has_value());
	EXPECT_GT(back.y, 1000.0f);
	// Round the far side, it never comes near it
	glm::vec2 farSide {1000.0f, 990.0f};
	EXPECT_FALSE(obstacles.RunsInto(0, Way::Anticlockwise, farSide, {1000.0f, 1010.0f}).has_value());
	EXPECT_EQ(farSide, glm::vec2(1000.0f, 990.0f));
}

TEST(CreatureRouteSearch, NothingInTheWayIsAStraightLine)
{
	auto obstacles = Empty({1000.0f, 1000.0f});
	Search search(obstacles, {.point = {1000.0f, 1000.0f}},
	              {.point = {1100.0f, 1000.0f}, .arriveWithin = 1.0f, .mostCost = 5200.0f});
	ASSERT_EQ(search.GetStatus(), Search::Status::Found);
	ASSERT_EQ(search.GetBest().size(), 1u);
	EXPECT_EQ(search.GetBest().front().to, glm::vec2(1100.0f, 1000.0f));
	// Arriving short of it, the line stops short
	Search shortOf(obstacles, {.point = {1000.0f, 1000.0f}},
	               {.point = {1100.0f, 1000.0f}, .stopShort = 10.0f, .arriveWithin = 11.0f, .mostCost = 5200.0f});
	ASSERT_EQ(shortOf.GetStatus(), Search::Status::Found);
	EXPECT_NEAR(shortOf.GetBest().front().to.x, 1090.0f - 0.005f, k_Tolerance);
}

TEST(CreatureRouteSearch, ACircleInTheWayIsWalkedRoundAlongItsTangents)
{
	auto obstacles = Empty({1000.0f, 1000.0f});
	obstacles.Add(1u, {1050.0f, 1000.0f}, 10.0f);
	Search search(obstacles, {.point = {1000.0f, 1000.0f}},
	              {.point = {1100.0f, 1000.0f}, .arriveWithin = 1.0f, .mostCost = 5200.0f});
	ASSERT_EQ(Finish(search, obstacles), Search::Status::Found);
	const auto route = Followed(search.GetBest(), obstacles);
	EXPECT_TRUE(ClearOf(route, obstacles));
	EXPECT_EQ(route.End(), glm::vec2(1100.0f, 1000.0f));
	// Searching back along it, the head-on approach is replaced by the tangent: the shortest way round
	const auto shortest =
	    (2.0f * std::sqrt((50.0f * 50.0f) - (10.0f * 10.0f))) + (10.0f * (k_Pi - (2.0f * std::acos(10.0f / 50.0f))));
	EXPECT_NEAR(LengthOfRoute(route), shortest, 0.05f);
	EXPECT_TRUE(std::ranges::any_of(route.legs, [](const Leg& leg) { return leg.arc; }));
}

TEST(CreatureRouteSearch, TheShorterWayRoundIsChosen)
{
	auto obstacles = Empty({1000.0f, 1000.0f});
	// A wall across the way, reaching much further to the north (+z) than the south
	for (int i = -2; i <= 8; ++i)
	{
		obstacles.Add(static_cast<uint32_t>(i + 10), {1050.0f, 1000.0f + (static_cast<float>(i) * 8.0f)}, 6.0f);
	}
	Search search(obstacles, {.point = {1000.0f, 1000.0f}},
	              {.point = {1100.0f, 1000.0f}, .arriveWithin = 1.0f, .mostCost = 5200.0f});
	ASSERT_EQ(Finish(search, obstacles), Search::Status::Found);
	const auto route = Followed(search.GetBest(), obstacles);
	EXPECT_TRUE(ClearOf(route, obstacles));
	const auto points = route.Points(k_Pi / 36.0f);
	EXPECT_LT(std::ranges::min(points, {}, [](glm::vec2 p) { return p.y; }).y, 1000.0f - 16.0f);
	EXPECT_TRUE(std::ranges::all_of(points, [](glm::vec2 p) { return p.y < 1010.0f; }));
}

TEST(CreatureRouteSearch, ACircleTheCreatureStandsInReachesJustToIt)
{
	auto obstacles = Empty({1000.0f, 1000.0f});
	ASSERT_TRUE(obstacles.Add(1u, {1004.0f, 1000.0f}, 10.0f));
	EXPECT_NEAR(obstacles.At(0).circle.radius, 4.0f, k_Tolerance);
	// One about the point right under it is no use at all
	EXPECT_FALSE(obstacles.Add(2u, {1000.05f, 1000.0f}, 10.0f));
	// It walks out of the one it stood in
	Search search(obstacles, {.point = {1000.0f, 1000.0f}},
	              {.point = {900.0f, 1000.0f}, .arriveWithin = 1.0f, .mostCost = 5200.0f});
	ASSERT_EQ(Finish(search, obstacles), Search::Status::Found);
	EXPECT_EQ(search.GetBest().back().to, glm::vec2(900.0f, 1000.0f));
	// And through it, round its edge
	Search through(obstacles, {.point = {1000.0f, 1000.0f}},
	               {.point = {1100.0f, 1000.0f}, .arriveWithin = 1.0f, .mostCost = 5200.0f});
	ASSERT_EQ(Finish(through, obstacles), Search::Status::Found);
	EXPECT_TRUE(ClearOf(Followed(through.GetBest(), obstacles), obstacles));
}

TEST(CreatureRouteSearch, AGoalItCantReachIsGivenUp)
{
	// Inside a circle met already, with an arrival ring no wider than the circle: given up at once
	auto obstacles = Empty({1000.0f, 1000.0f});
	obstacles.Add(1u, {1101.0f, 1000.0f}, 8.0f);
	Search inside(obstacles, {.point = {1000.0f, 1000.0f}},
	              {.point = {1100.0f, 1000.0f}, .arriveWithin = 1.0f, .mostCost = 5200.0f});
	EXPECT_EQ(inside.GetStatus(), Search::Status::Enclosed);
	// A ring reaching out of it is reached outside it
	Search ring(obstacles, {.point = {1000.0f, 1000.0f}},
	            {.point = {1100.0f, 1000.0f}, .stopShort = 0.0f, .arriveWithin = 9.0f, .mostCost = 5200.0f});
	ASSERT_EQ(Finish(ring, obstacles), Search::Status::Found);
	EXPECT_GE(glm::distance(ring.GetBest().back().to, glm::vec2(1101.0f, 1000.0f)), 8.0f - k_Tolerance);

	// Walled in by a ring of circles: every way round comes back on itself, and the search fails
	auto walled = Empty({1000.0f, 1000.0f});
	for (int i = 0; i < 12; ++i)
	{
		const auto angle = static_cast<float>(i) * 2.0f * k_Pi / 12.0f;
		walled.Add(static_cast<uint32_t>(i),
		           glm::vec2(1100.0f, 1000.0f) + (20.0f * glm::vec2(std::cos(angle), std::sin(angle))), 6.0f);
	}
	Search walledIn(walled, {.point = {1000.0f, 1000.0f}},
	                {.point = {1100.0f, 1000.0f}, .arriveWithin = 1.0f, .mostCost = 5200.0f});
	EXPECT_EQ(Finish(walledIn, walled), Search::Status::Failed);
}

TEST(CreatureRouteSearch, CirclesAreGatheredBlockByBlockAsTheSearchReachesThem)
{
	std::vector<glm::ivec2> asked;
	Obstacles obstacles;
	obstacles.Reset({1000.0f, 1000.0f}, 2.0f, [&asked](glm::ivec2 cell, Obstacles& into) {
		asked.push_back(cell);
		// A hall stands in the cell of its middle
		if (cell == glm::ivec2(105, 100) && !into.BlockHas(5u, cell))
		{
			into.Add(5u, {1050.0f, 1000.0f}, 12.0f);
		}
	});
	Search search(obstacles, {.point = {1000.0f, 1000.0f}},
	              {.point = {1100.0f, 1000.0f}, .arriveWithin = 1.0f, .mostCost = 5200.0f});
	ASSERT_EQ(Finish(search, obstacles), Search::Status::Found);
	EXPECT_EQ(obstacles.All().size(), 1u);
	EXPECT_TRUE(ClearOf(Followed(search.GetBest(), obstacles), obstacles));
	// Each block's 64 cells once, only for the blocks near the way
	EXPECT_EQ(asked.size() % 64, 0u);
	EXPECT_LE(asked.size(), 64u * 4u);
}

TEST(CreatureRouteSearch, AWalkerFollowsTheRouteFoundAndStandsAtItsEnd)
{
	Follower follower;
	follower.Reset({1000.0f, 1000.0f}, 2.0f);
	follower.GetObstacles().Add(1u, {1050.0f, 1000.0f}, 10.0f);
	follower.SetDestination({.point = {1100.0f, 1000.0f}, .arriveWithin = 1.0f, .mostCost = 5200.0f});
	ASSERT_EQ(follower.GetState(), Follower::State::Planning);
	auto outcome = Follower::Outcome::None;
	for (int turn = 0; turn < 100 && outcome == Follower::Outcome::None; ++turn)
	{
		outcome = follower.Update(16, false, 0.0f);
	}
	ASSERT_EQ(outcome, Follower::Outcome::Found);
	EXPECT_EQ(follower.GetState(), Follower::State::Walking);
	EXPECT_TRUE(ClearOf(follower.GetRoute(), follower.GetObstacles()));
	Advanced advanced {};
	for (int turn = 0; turn < 1000 && !advanced.finished; ++turn)
	{
		advanced = follower.Advance(2.0f);
		EXPECT_GE(glm::distance(advanced.position, glm::vec2(1050.0f, 1000.0f)), 10.0f - 0.05f);
	}
	EXPECT_TRUE(advanced.finished);
	EXPECT_EQ(follower.GetState(), Follower::State::Standing);
	EXPECT_EQ(follower.GetPosition(), glm::vec2(1100.0f, 1000.0f));
	// What it walked round is forgotten at the end of the walk
	EXPECT_TRUE(follower.GetObstacles().All().empty());
}

TEST(CreatureRouteSearch, WalkingANewDestinationIsSearchedFromPointsAhead)
{
	Follower follower;
	follower.Reset({1000.0f, 1000.0f}, 2.0f);
	follower.GetObstacles().Add(1u, {1050.0f, 1000.0f}, 10.0f);
	follower.SetDestination({.point = {1100.0f, 1000.0f}, .arriveWithin = 1.0f, .mostCost = 5200.0f});
	while (follower.Update(16, false, 0.0f) == Follower::Outcome::None)
	{
	}
	const auto legs = follower.GetRoute().legs.size();
	ASSERT_GE(legs, 3u);
	follower.Advance(5.0f);
	const auto position = follower.GetPosition();

	// Asked to go elsewhere, it walks on while searching from the end of each leg ahead
	follower.SetDestination({.point = {1100.0f, 1100.0f}, .arriveWithin = 1.0f, .mostCost = 5200.0f});
	EXPECT_EQ(follower.GetState(), Follower::State::Replanning);
	EXPECT_GE(follower.GetSearchCount(), 2u);
	EXPECT_LE(follower.GetSearchCount(), 5u);
	auto outcome = Follower::Outcome::None;
	for (int turn = 0; turn < 100 && outcome == Follower::Outcome::None; ++turn)
	{
		outcome = follower.Update(16, false, 0.0f);
	}
	ASSERT_EQ(outcome, Follower::Outcome::Found);
	EXPECT_EQ(follower.GetState(), Follower::State::Walking);
	EXPECT_EQ(follower.GetRoute().End(), glm::vec2(1100.0f, 1100.0f));
	// The new route is joined on where it is
	EXPECT_NEAR(glm::distance(follower.GetRoute().Position(), position), 0.0f, 1e-2f);
	EXPECT_TRUE(ClearOf(follower.GetRoute(), follower.GetObstacles()));
}

TEST(CreatureRouteSearch, ASearchFromAPointWalkedPastIsDropped)
{
	Follower follower;
	follower.Reset({1000.0f, 1000.0f}, 2.0f);
	follower.GetObstacles().Add(1u, {1050.0f, 1000.0f}, 10.0f);
	follower.SetDestination({.point = {1100.0f, 1000.0f}, .arriveWithin = 1.0f, .mostCost = 5200.0f});
	while (follower.Update(16, false, 0.0f) == Follower::Outcome::None)
	{
	}
	follower.SetDestination({.point = {1000.0f, 1200.0f}, .arriveWithin = 1.0f, .mostCost = 5200.0f});
	const auto searches = follower.GetSearchCount();
	ASSERT_GE(searches, 2u);
	// Walking past the end of the leg it is on, the search from there is too late
	follower.Advance(follower.GetRoute().Remaining() + 0.5f);
	EXPECT_EQ(follower.GetSearchCount(), searches - 1);
}

TEST(CreatureRouteSearch, ANearNewDestinationWaitsForTheSearchUnderWay)
{
	Follower follower;
	follower.Reset({1000.0f, 1000.0f}, 2.0f);
	follower.GetObstacles().Add(1u, {1050.0f, 1000.0f}, 10.0f);
	follower.SetDestination({.point = {1100.0f, 1000.0f}, .arriveWithin = 1.0f, .mostCost = 5200.0f});
	// Near the old destination: the search goes on, and the new one is searched for once it ends
	follower.SetDestination({.point = {1100.0f, 1010.0f}, .arriveWithin = 1.0f, .mostCost = 5200.0f});
	EXPECT_EQ(follower.GetState(), Follower::State::Planning);
	while (follower.Update(16, false, 0.0f) == Follower::Outcome::None)
	{
	}
	EXPECT_EQ(follower.GetDestination().point, glm::vec2(1100.0f, 1010.0f));
	EXPECT_EQ(follower.GetState(), Follower::State::Replanning);
	// Far from it: searched for afresh at once
	Follower restarted;
	restarted.Reset({1000.0f, 1000.0f}, 2.0f);
	restarted.SetDestination({.point = {1100.0f, 1000.0f}, .arriveWithin = 1.0f, .mostCost = 5200.0f});
	restarted.SetDestination({.point = {900.0f, 1000.0f}, .arriveWithin = 1.0f, .mostCost = 5200.0f});
	EXPECT_EQ(restarted.GetState(), Follower::State::Planning);
	ASSERT_EQ(restarted.Update(16, false, 0.0f), Follower::Outcome::Found);
	EXPECT_EQ(restarted.GetRoute().End(), glm::vec2(900.0f, 1000.0f));
}
