/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <deque>
#include <vector>

#include <glm/gtc/constants.hpp>
#include <gtest/gtest.h>

#include "Temple/TemplePeopleRules.h"

using namespace openblack;
using namespace openblack::temple_people;

namespace
{
/// Dice that give back what they are told, in order, and count their draws
class ScriptedDice final: public Dice
{
public:
	std::deque<uint32_t> below;
	std::deque<float> shares;
	std::vector<float> asked;

	uint32_t Below(int32_t n) override
	{
		const auto value = below.empty() ? 0u : below.front();
		if (!below.empty())
		{
			below.pop_front();
		}
		return value % static_cast<uint32_t>(n);
	}
	/// A share of x: 0 to 1 by what it is told, 0 once it runs out
	float UpTo(float x) override
	{
		asked.push_back(x);
		const float share = shares.empty() ? 0.0f : shares.front();
		if (!shares.empty())
		{
			shares.pop_front();
		}
		return share * x;
	}
};

/// Two triangles making a square of side 10 on the floor, their vertices unshared: (0,0)-(10,0)-(10,10) and
/// (0,0)-(10,10)-(0,10)
WalkMesh Square()
{
	const std::vector<glm::vec3> vertices {{0, 0, 0}, {10, 0, 0}, {10, 0, 10}, {0, 0, 0}, {10, 0, 10}, {0, 0, 10}};
	const std::vector<std::array<uint16_t, 3>> triangles {{0, 1, 2}, {3, 4, 5}};
	return BuildWalkMesh(vertices, triangles);
}
} // namespace

TEST(TemplePeopleRules, WalkMeshMergesCornersAndFindsNeighbours)
{
	const auto mesh = Square();
	ASSERT_EQ(mesh.corners.size(), 4u);
	ASSERT_EQ(mesh.triangles.size(), 2u);
	// The first triangle's edge facing its second corner, (10,10)-(0,0), is the second triangle's
	EXPECT_EQ(mesh.triangles[0].beyond, (std::array<int32_t, 3> {-1, 1, -1}));
	// The second's edge facing its third corner, (0,0)-(10,10), is the first's
	EXPECT_EQ(mesh.triangles[1].beyond, (std::array<int32_t, 3> {-1, -1, 0}));
}

TEST(TemplePeopleRules, PointInGoesAlongTheFirstEdgeThenTheSecond)
{
	const auto mesh = Square();
	ScriptedDice dice;
	dice.shares = {0.5f, 0.5f};
	const auto point = PointIn(mesh, 0, dice);
	// Half along (0,0)-(10,0), then half of the half left along (0,0)-(10,10)
	EXPECT_FLOAT_EQ(point.x, 7.5f);
	EXPECT_FLOAT_EQ(point.z, 2.5f);
	ASSERT_EQ(dice.asked.size(), 2u);
	EXPECT_FLOAT_EQ(dice.asked[1], 0.5f);
}

TEST(TemplePeopleRules, PointOnEdgeRunsBetweenTheOtherTwoCorners)
{
	const auto mesh = Square();
	ScriptedDice dice;
	dice.shares = {0.25f};
	// The edge facing the first corner runs from (10,0) to (10,10)
	const auto point = PointOnEdge(mesh, 0, 0, dice);
	EXPECT_FLOAT_EQ(point.x, 10.0f);
	EXPECT_FLOAT_EQ(point.z, 2.5f);
}

TEST(TemplePeopleRules, HeadingsAndFacings)
{
	EXPECT_FLOAT_EQ(HeadingBetween({0, 0, 0}, {1, 0, 0}), 0.0f);
	EXPECT_FLOAT_EQ(HeadingBetween({0, 0, 0}, {0, 0, 1}), glm::half_pi<float>());
	// The model faces a quarter turn on from the heading
	EXPECT_FLOAT_EQ(FacingOf({1, 0, 0}), glm::half_pi<float>());
	EXPECT_FLOAT_EQ(FacingOf({0, 0, 1}), glm::pi<float>());
	EXPECT_FLOAT_EQ(FacingOf({0, 5, 0}), 0.0f);
}

TEST(TemplePeopleRules, ArrivingStandsInARandomTriangleForATenthOfASecond)
{
	const auto mesh = Square();
	ScriptedDice dice;
	dice.below = {1};
	dice.shares = {0.0f, 0.0f, 0.5f, 0.0f};
	const auto walker = Arrive(mesh, dice);
	EXPECT_EQ(walker.triangle, 1);
	EXPECT_EQ(walker.nextTriangle, 1);
	EXPECT_EQ(walker.gait, Gait::Standing);
	EXPECT_FLOAT_EQ(walker.pause, 0.1f);
	EXPECT_FLOAT_EQ(walker.speed, 1.0f);
	EXPECT_FALSE(walker.inside);
	EXPECT_EQ(walker.position, glm::vec3(0.0f));
	EXPECT_FLOAT_EQ(walker.target.x, 5.0f);
	EXPECT_FLOAT_EQ(walker.target.z, 5.0f);
	EXPECT_FLOAT_EQ(walker.heading, glm::quarter_pi<float>());
}

TEST(TemplePeopleRules, StandingSetsOffOnceThePauseIsOver)
{
	const auto mesh = Square();
	Walker walker;
	walker.clipPlace = 300;
	ScriptedDice dice;
	dice.shares = {0.5f};
	Step(walker, mesh, 0.05f, dice);
	EXPECT_EQ(walker.gait, Gait::Standing);
	Step(walker, mesh, 0.06f, dice);
	EXPECT_EQ(walker.gait, Gait::Walking);
	// The change starts the clock and the clip over, and draws the next pause: 2 and up to 2 more
	EXPECT_FLOAT_EQ(walker.timer, 0.0f);
	EXPECT_EQ(walker.clipPlace, 0u);
	EXPECT_FLOAT_EQ(walker.pause, 3.0f);
}

TEST(TemplePeopleRules, WalkingMovesBySpeedUntilItReaches)
{
	const auto mesh = Square();
	Walker walker;
	walker.gait = Gait::Walking;
	walker.position = {1, 0, 1};
	walker.target = {5, 0, 1};
	walker.speed = 2.0f;
	ScriptedDice dice;
	Step(walker, mesh, 1.0f, dice);
	EXPECT_FLOAT_EQ(walker.position.x, 3.0f);
	EXPECT_EQ(walker.target, glm::vec3(5, 0, 1));
	EXPECT_TRUE(dice.asked.empty());
}

TEST(TemplePeopleRules, ReachingFirstHeadsForAnEdgeThenIntoTheTriangleBeyond)
{
	const auto mesh = Square();
	Walker walker;
	walker.gait = Gait::Walking;
	walker.triangle = 0;
	walker.nextTriangle = 0;
	walker.position = {5, 0, 1};
	walker.target = {6, 0, 1};
	walker.heading = 0.0f;
	walker.speed = 10.0f;
	ScriptedDice dice;
	// Edges 0 and 2 have nothing beyond: a point in the triangle each. Edge 1: four points on the edge to the second
	// triangle; then four in the triangle itself. Every share 0.5 lands on (5,5)-ish points; the choice draw of 0
	// takes the first way
	for (int i = 0; i < 64; ++i)
	{
		dice.shares.push_back(0.5f);
	}
	dice.shares.push_back(0.9f);
	Step(walker, mesh, 1.0f, dice);
	EXPECT_EQ(walker.position, glm::vec3(6, 0, 1));
	EXPECT_TRUE(walker.inside);
	// The draws: two per dead end, three per sample of a way across an edge, two per sample inside
	// 2 + 2 + 4 * 3 + 4 * 2 = 24 of the samples, then the choice and the stop
	EXPECT_GE(dice.asked.size(), 26u);
	// The choice is drawn up to the sum of the weights: two dead ends and two ways weighted at least 1
	const float total = dice.asked.at(24);
	EXPECT_GE(total, 2.0f);
}

TEST(TemplePeopleRules, TheWayOnInsideIsForcedAfterAnEdge)
{
	const auto mesh = Square();
	Walker walker;
	walker.gait = Gait::Walking;
	walker.inside = true;
	walker.triangle = 0;
	walker.nextTriangle = 1;
	walker.position = {2, 0, 2};
	walker.target = {2, 0, 2};
	ScriptedDice dice;
	Step(walker, mesh, 0.1f, dice);
	// Now in the triangle beyond, it goes on inside it
	EXPECT_EQ(walker.triangle, 1);
	EXPECT_EQ(walker.nextTriangle, 1);
	EXPECT_FALSE(walker.inside);
}

TEST(TemplePeopleRules, AFifthOfPointsReachedStopTheWalker)
{
	const auto mesh = Square();
	Walker walker;
	walker.gait = Gait::Walking;
	walker.inside = true;
	walker.position = {2, 0, 2};
	walker.target = {2, 0, 2};
	ScriptedDice dice;
	// The 28 samples' shares, the choice, then the stop draw of 0.1 of 1, under 0.2
	for (int i = 0; i < 24; ++i)
	{
		dice.shares.push_back(0.0f);
	}
	dice.shares.push_back(0.0f);
	dice.shares.push_back(0.1f);
	dice.shares.push_back(0.0f);
	Step(walker, mesh, 0.1f, dice);
	EXPECT_EQ(walker.gait, Gait::Standing);
	EXPECT_FLOAT_EQ(walker.pause, 2.0f);
}

TEST(TemplePeopleRules, VisitorsComeSiteBySiteSkippingTheUnavailableAndTwiceListed)
{
	const auto a = static_cast<entt::entity>(1);
	const auto b = static_cast<entt::entity>(2);
	const auto c = static_cast<entt::entity>(3);
	const std::vector<std::vector<entt::entity>> sites {{a, b}, {a, c}};
	const auto visitors = ChooseVisitors(sites, [b](entt::entity e) { return e != b; });
	EXPECT_EQ(visitors, (std::vector<entt::entity> {a, c}));
}

TEST(TemplePeopleRules, AtMostThirtyTwoComeIn)
{
	std::vector<entt::entity> many;
	for (uint32_t i = 0; i < 40; ++i)
	{
		many.push_back(static_cast<entt::entity>(i));
	}
	const std::vector<std::vector<entt::entity>> sites {many};
	EXPECT_EQ(ChooseVisitors(sites, [](entt::entity) { return true; }).size(), k_MostVisitors);
}

TEST(TemplePeopleRules, DrawnOnlyOnScreenAndNear)
{
	EXPECT_TRUE(Drawn({0.0f, 0.0f, 0.0f, 100.0f}, 1.0f));
	EXPECT_FALSE(Drawn({0.0f, 0.0f, 0.0f, 250.0f}, 1.0f));
	EXPECT_TRUE(Drawn({0.0f, 0.0f, 0.0f, 249.0f}, 1.0f));
	EXPECT_FALSE(Drawn({0.0f, 0.0f, 0.0f, 0.5f}, 1.0f));
	// Feet off the side of the screen
	EXPECT_FALSE(Drawn({120.0f, 0.0f, 0.0f, 100.0f}, 1.0f));
	EXPECT_FALSE(Drawn({0.0f, -101.0f, 0.0f, 100.0f}, 1.0f));
}

TEST(TemplePeopleRules, ClipsAndSpeed)
{
	EXPECT_EQ(ClipOf(Gait::Walking, true), AnimId::PWalkMan);
	EXPECT_EQ(ClipOf(Gait::Walking, false), AnimId::PWalkWoman);
	EXPECT_EQ(ClipOf(Gait::Standing, true), AnimId::PStand);
	// The walk's stride each second of play, by the villager's scale
	EXPECT_FLOAT_EQ(SpeedOf(1.1603317f, 1000, 1.0f), 1.1603317f);
	EXPECT_FLOAT_EQ(SpeedOf(1.1603317f, 1000, 2.0f), 2.3206634f);
	EXPECT_FLOAT_EQ(SpeedOf(0.0f, 1000, 1.0f), 0.0f);
}
