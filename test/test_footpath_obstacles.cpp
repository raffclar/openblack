/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <array>
#include <optional>
#include <tuple>
#include <vector>

#include <glm/common.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Common/GUtilsDistance.h"
#include "ECS/FootpathObstacles.h"
#include "ECS/FootpathPlanning.h"
#include "ECS/FootpathRules.h"

using namespace openblack;
using namespace openblack::ecs;
namespace obstacle_rules = openblack::ecs::footpath_obstacles;
namespace planning = openblack::ecs::footpath_planning;
namespace rules = openblack::ecs::footpath_rules;

namespace
{

/// A place this many metres east of a point well inside the map
glm::ivec2 East(float metres)
{
	constexpr int32_t k_Base = 1000 * 6553;
	return {k_Base + static_cast<int32_t>(metres * 6553.6f), k_Base};
}

/// What the rules asked of the world
struct Calls
{
	std::vector<std::pair<glm::ivec2, glm::ivec2>> plans;
	std::vector<std::pair<uint32_t, bool>> lookAgain;
	std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> sendOn;
	std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> cutOff;
};

obstacle_rules::Hooks FakeHooks(Calls& calls, std::optional<std::vector<glm::ivec2>> route)
{
	return {
	    .planRoute =
	        [&calls, route](glm::ivec2 from, glm::ivec2 to) {
		        calls.plans.emplace_back(from, to);
		        return route;
	        },
	    .lookAgain = [&calls](uint32_t key, bool towardsStart) { calls.lookAgain.emplace_back(key, towardsStart); },
	    .sendOn = [&calls](uint32_t gone, uint32_t back, glm::ivec2 /*backPlace*/, uint32_t on,
	                       glm::ivec2 /*onPlace*/) { calls.sendOn.emplace_back(gone, back, on); },
	    .cutOff = [&calls](uint32_t gone, uint32_t onwardFrom,
	                       uint32_t first) { calls.cutOff.emplace_back(gone, onwardFrom, first); },
	    .altitude = [](glm::ivec2 /*place*/) { return 0.0f; },
	};
}

/// Three anchors in a line 20 m apart, the middle one where the thing is put
std::vector<obstacle_rules::KeyedNode> ThreeAnchors()
{
	return {
	    {.node = {.place = East(0.0f)}, .key = 1},
	    {.node = {.place = East(20.0f)}, .key = 2},
	    {.node = {.place = East(40.0f)}, .key = 3},
	};
}

constexpr float k_Radius = 3.0f;

} // namespace

TEST(FootpathObstacles, ANodeUnderTheThingIsHiddenAndTheStretchPlannedRoundIt)
{
	auto nodes = ThreeAnchors();
	Calls calls;
	const std::vector<glm::ivec2> route {East(15.0f) + glm::ivec2(0, 30000), East(25.0f) + glm::ivec2(0, 30000)};
	obstacle_rules::GoRound(nodes, {.radius = k_Radius, .place = East(20.0f)}, FakeHooks(calls, route));

	// The stretch from the first anchor to the last is planned, the hidden node passed over
	ASSERT_EQ(calls.plans.size(), 1u);
	EXPECT_EQ(calls.plans[0].first, East(0.0f));
	EXPECT_EQ(calls.plans[0].second, East(40.0f));
	// The route, the marker on the thing's place, the hidden node kept, then the last anchor
	ASSERT_EQ(nodes.size(), 6u);
	EXPECT_EQ(nodes[0].key, 1u);
	EXPECT_EQ(nodes[1].node, (planning::PlanNode {.place = route[0], .planned = true}));
	EXPECT_EQ(nodes[2].node, (planning::PlanNode {.place = route[1], .planned = true}));
	EXPECT_EQ(nodes[3].node, (planning::PlanNode {.place = East(20.0f), .planned = true, .marker = true}));
	EXPECT_EQ(nodes[4].key, 2u);
	EXPECT_TRUE(nodes[4].node.hidden);
	EXPECT_EQ(nodes[5].key, 3u);
	// Walkers heading for the two anchors look again; those heading for the nodes between continue to an anchor
	EXPECT_EQ(calls.lookAgain, (std::vector<std::pair<uint32_t, bool>> {{3u, false}, {1u, true}}));
	ASSERT_EQ(calls.sendOn.size(), 2u);
	EXPECT_EQ(calls.sendOn[0], std::make_tuple(2u, 1u, 3u));
	EXPECT_EQ(std::get<1>(calls.sendOn[1]), 1u);
	EXPECT_EQ(std::get<2>(calls.sendOn[1]), 3u);
}

TEST(FootpathObstacles, WithNoWayFoundNodesAreSteppedRoundTheCircle)
{
	// A path that passes the thing 1 m to one side, with no node near it
	std::vector<obstacle_rules::KeyedNode> nodes {
	    {.node = {.place = East(0.0f) + glm::ivec2(0, 6553)}, .key = 1},
	    {.node = {.place = East(40.0f) + glm::ivec2(0, 6553)}, .key = 2},
	};
	Calls calls;
	obstacle_rules::GoRound(nodes, {.radius = k_Radius, .place = East(20.0f)}, FakeHooks(calls, std::nullopt));
	ASSERT_EQ(calls.plans.size(), 1u);
	ASSERT_GT(nodes.size(), 3u);
	EXPECT_EQ(nodes.front().key, 1u);
	EXPECT_EQ(nodes.back().key, 2u);
	// Every node between is planned and outside the line's side of the circle, near its edge
	for (size_t i = 1; i + 1 < nodes.size(); ++i)
	{
		EXPECT_TRUE(nodes[i].node.planned);
		EXPECT_FALSE(nodes[i].node.hidden || nodes[i].node.marker);
		const glm::vec2 off {static_cast<float>(nodes[i].node.place.x - East(20.0f).x) / 6553.6f,
		                     static_cast<float>(nodes[i].node.place.y - East(20.0f).y) / 6553.6f};
		EXPECT_NEAR(std::sqrt(off.x * off.x + off.y * off.y), k_Radius, 0.01f);
	}
	// The walkers on both ends of the stretch look again
	EXPECT_EQ(calls.lookAgain.size(), 2u);
}

TEST(FootpathObstacles, ALineThatMissesTheThingIsLeftAlone)
{
	std::vector<obstacle_rules::KeyedNode> nodes {
	    {.node = {.place = East(0.0f) + glm::ivec2(0, 6553 * 10)}, .key = 1},
	    {.node = {.place = East(40.0f) + glm::ivec2(0, 6553 * 10)}, .key = 2},
	};
	const auto before = nodes;
	Calls calls;
	obstacle_rules::GoRound(nodes, {.radius = k_Radius, .place = East(20.0f)}, FakeHooks(calls, std::nullopt));
	EXPECT_EQ(nodes, before);
	EXPECT_TRUE(calls.plans.empty());
}

TEST(FootpathObstacles, TakenAwayTheHiddenNodeShowsAgainAndTheMarkerGoes)
{
	auto nodes = ThreeAnchors();
	{
		Calls calls;
		const std::vector<glm::ivec2> route {East(15.0f) + glm::ivec2(0, 30000)};
		obstacle_rules::GoRound(nodes, {.radius = k_Radius, .place = East(20.0f)}, FakeHooks(calls, route));
	}
	Calls calls;
	obstacle_rules::StopGoingRound(nodes, {.radius = k_Radius, .place = East(20.0f)}, FakeHooks(calls, std::nullopt));
	// The stretches each side of the node shown again are planned again, and find no way here
	ASSERT_EQ(calls.plans.size(), 2u);
	EXPECT_EQ(calls.plans[0], std::make_pair(East(0.0f), East(20.0f)));
	EXPECT_EQ(calls.plans[1], std::make_pair(East(20.0f), East(40.0f)));
	ASSERT_EQ(nodes.size(), 3u);
	EXPECT_EQ(nodes[0].key, 1u);
	EXPECT_EQ(nodes[1].key, 2u);
	EXPECT_FALSE(nodes[1].node.hidden);
	EXPECT_EQ(nodes[2].key, 3u);
}

TEST(FootpathObstacles, TakenAwayABendRoundTheThingIsCutOut)
{
	// A path bent round the thing by nodes stepped round its circle, with no hidden node and no marker
	std::vector<obstacle_rules::KeyedNode> nodes {
	    {.node = {.place = East(0.0f) + glm::ivec2(0, 6553)}, .key = 1},
	    {.node = {.place = East(40.0f) + glm::ivec2(0, 6553)}, .key = 2},
	};
	{
		Calls calls;
		obstacle_rules::GoRound(nodes, {.radius = k_Radius, .place = East(20.0f)}, FakeHooks(calls, std::nullopt));
	}
	ASSERT_GT(nodes.size(), 3u);
	const auto bent = nodes.size();
	Calls calls;
	obstacle_rules::StopGoingRound(nodes, {.radius = k_Radius, .place = East(20.0f)}, FakeHooks(calls, std::nullopt));
	EXPECT_EQ(calls.plans.size(), 1u);
	EXPECT_LT(nodes.size(), bent);
	EXPECT_EQ(nodes.front().key, 1u);
	EXPECT_EQ(nodes.back().key, 2u);
}

TEST(FootpathObstacles, StraightEnoughIsTheSignedTangentAtTheCorner)
{
	// A straight line through the corner has a cross product of 0
	EXPECT_TRUE(obstacle_rules::StraightEnough({0.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}));
	// A right angle one way is far from straight, the other way counts as straight (the cross product is signed)
	EXPECT_FALSE(obstacle_rules::StraightEnough({1.0f, 0.0f, 1.0f}, {2.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}) &&
	             obstacle_rules::StraightEnough({1.0f, 0.0f, -1.0f}, {2.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}));
}

TEST(FootpathObstacles, ReachesAreTheGames)
{
	EXPECT_FLOAT_EQ(obstacle_rules::ReturnReach(10.0f), 10.0f * 1.03f + 0.4f);
	// 3 m is 19660 whole units; its square widened by a hundredth
	EXPECT_FLOAT_EQ(obstacle_rules::HideReachSq(3.0f), 19660.0f * 19660.0f * 1.01f);
}

TEST(FootpathRulesPassedBy, WalkersPassHiddenNodesAndMarkersBy)
{
	const std::array<glm::ivec2, 5> places {East(0.0f), East(10.0f), East(20.0f), East(30.0f), East(40.0f)};
	const std::array<uint8_t, 5> passedBy {0, 1, 1, 0, 0};
	EXPECT_EQ(rules::NextNode(places, 0, false, passedBy), 3u);
	EXPECT_EQ(rules::NextNode(places, 3, true, passedBy), 0u);
	EXPECT_EQ(rules::NextNode(places, 0, true, passedBy), std::nullopt);
	// The nearest node to a walker on a passed-by node is one that walkers walk to
	EXPECT_EQ(rules::NearestNode(places, East(19.0f), false, passedBy), 3u);
	// Without the flags every node counts
	EXPECT_EQ(rules::NextNode(places, 0, false), 1u);
}

TEST(FootpathObstaclesClear, AThingFarFromEveryNodeIsClearOfThePath)
{
	const auto nodes = ThreeAnchors();
	const obstacle_rules::Obstacle far {.radius = k_Radius, .place = East(100.0f)};
	EXPECT_TRUE(obstacle_rules::GoesClearOf(nodes.front().node.place, nodes.back().node.place, far));
	const obstacle_rules::Obstacle near {.radius = k_Radius, .place = East(20.0f)};
	EXPECT_FALSE(obstacle_rules::GoesClearOf(nodes.front().node.place, nodes.back().node.place, near));
}

TEST(FootpathObstaclesClear, AThingJustOffTheEndOfThePathTouchesIt)
{
	// The square about the thing reaches the last node exactly: the rules look at that node
	const auto nodes = ThreeAnchors();
	const auto half = gutils::ConvertMetersToWholeDistance(k_Radius);
	const auto last = nodes.back().node.place;
	const obstacle_rules::Obstacle touching {.radius = k_Radius, .place = {last.x + half, last.y}};
	EXPECT_FALSE(obstacle_rules::GoesClearOf(nodes.front().node.place, last, touching));
	const obstacle_rules::Obstacle clear {.radius = k_Radius, .place = {last.x + half + 1, last.y}};
	EXPECT_TRUE(obstacle_rules::GoesClearOf(nodes.front().node.place, last, clear));
}

TEST(FootpathObstaclesClear, ASquareThatWrapsIsNeverClear)
{
	const obstacle_rules::Obstacle atEdge {.radius = k_Radius, .place = {0, 0}};
	EXPECT_FALSE(obstacle_rules::GoesClearOf(East(100.0f), East(200.0f), atEdge));
}

TEST(FootpathObstaclesClear, APathClearOfTheThingIsLeftAsItIsWithNoCalls)
{
	// Many paths of random nodes and things at random places: each time the test says clear, the rules change nothing
	uint32_t seed = 12345;
	const auto next = [&seed](int32_t range) {
		seed = seed * 1664525u + 1013904223u;
		return static_cast<int32_t>((seed >> 8u) % static_cast<uint32_t>(range));
	};
	int clearCount = 0;
	for (int round = 0; round < 3000; ++round)
	{
		std::vector<obstacle_rules::KeyedNode> nodes;
		const auto count = 2 + next(6);
		for (int32_t i = 0; i < count; ++i)
		{
			nodes.push_back({.node = {.place = East(static_cast<float>(next(200))) + glm::ivec2 {0, next(400000)},
			                          .planned = next(3) == 0,
			                          .hidden = next(5) == 0,
			                          .marker = next(7) == 0},
			                 .key = static_cast<uint32_t>(i + 1)});
		}
		const obstacle_rules::Obstacle thing {.radius = 1.0f + static_cast<float>(next(20)),
		                                      .place = East(static_cast<float>(next(260)) - 30.0f) +
		                                               glm::ivec2 {0, next(500000) - 50000}};
		glm::ivec2 low = nodes.front().node.place;
		glm::ivec2 high = low;
		for (const auto& node : nodes)
		{
			low = glm::min(low, node.node.place);
			high = glm::max(high, node.node.place);
		}
		if (!obstacle_rules::GoesClearOf(low, high, thing))
		{
			continue;
		}
		++clearCount;
		Calls calls;
		auto changed = nodes;
		obstacle_rules::GoRound(changed, thing, FakeHooks(calls, std::vector<glm::ivec2> {East(1.0f)}));
		EXPECT_EQ(changed, nodes);
		EXPECT_TRUE(calls.plans.empty());
		EXPECT_TRUE(calls.lookAgain.empty());
		EXPECT_TRUE(calls.sendOn.empty());
		EXPECT_TRUE(calls.cutOff.empty());
	}
	EXPECT_GT(clearCount, 100);
}

TEST(FootpathObstaclesClear, ANodeOnTheEdgeOfTheReturnSquareIsNotInIt)
{
	const obstacle_rules::Obstacle thing {.radius = k_Radius, .place = East(100.0f)};
	const obstacle_rules::ReturnSquare square(thing);
	const auto half = gutils::ConvertMetersToWholeDistance(obstacle_rules::ReturnReach(k_Radius));
	EXPECT_TRUE(square.Holds(thing.place));
	EXPECT_TRUE(square.Holds({thing.place.x + half - 1, thing.place.y}));
	EXPECT_FALSE(square.Holds({thing.place.x + half, thing.place.y}));
	EXPECT_FALSE(square.Holds({thing.place.x, thing.place.y - half}));
}

TEST(FootpathObstaclesClear, APathWithNoNodeInTheReturnSquareIsLeftAsItIsWithNoCalls)
{
	// Many paths of random nodes about things removed at random places: each time no node is in the square, the way
	// back changes nothing
	uint32_t seed = 54321;
	const auto next = [&seed](int32_t range) {
		seed = seed * 1664525u + 1013904223u;
		return static_cast<int32_t>((seed >> 8u) % static_cast<uint32_t>(range));
	};
	int clearCount = 0;
	int heldCount = 0;
	for (int round = 0; round < 3000; ++round)
	{
		const obstacle_rules::Obstacle thing {.radius = 1.0f + static_cast<float>(next(20)),
		                                      .place = East(100.0f + static_cast<float>(next(60)))};
		std::vector<obstacle_rules::KeyedNode> nodes;
		const auto count = 2 + next(6);
		for (int32_t i = 0; i < count; ++i)
		{
			nodes.push_back(
			    {.node = {.place = East(60.0f + static_cast<float>(next(140))) + glm::ivec2 {0, next(80000) - 40000},
			              .planned = next(3) != 0,
			              .hidden = next(4) == 0,
			              .marker = next(4) == 0},
			     .key = static_cast<uint32_t>(i + 1)});
		}
		const obstacle_rules::ReturnSquare square(thing);
		if (std::ranges::any_of(nodes, [&square](const auto& node) { return square.Holds(node.node.place); }))
		{
			++heldCount;
			continue;
		}
		++clearCount;
		Calls calls;
		auto changed = nodes;
		obstacle_rules::StopGoingRound(changed, thing, FakeHooks(calls, std::vector<glm::ivec2> {East(1.0f)}));
		EXPECT_EQ(changed, nodes);
		EXPECT_TRUE(calls.plans.empty());
		EXPECT_TRUE(calls.lookAgain.empty());
		EXPECT_TRUE(calls.sendOn.empty());
		EXPECT_TRUE(calls.cutOff.empty());
	}
	EXPECT_GT(clearCount, 100);
	EXPECT_GT(heldCount, 100);
}
