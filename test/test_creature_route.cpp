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

using namespace openblack;
using namespace openblack::creature_route;

namespace
{
constexpr float k_Tolerance = 1e-3f;
constexpr float k_Pi = std::numbers::pi_v<float>;

/// Flat land 5 units up, without water
float Flat(int32_t /*x*/, int32_t /*z*/)
{
	return 5.0f;
}
std::optional<bool> Dry(int32_t /*x*/, int32_t /*z*/)
{
	return false;
}

/// Flat land with a steep ridge across x = 2000 to 2020, from z = 1790 to 2210
WalkableLand LandWithCliff()
{
	return WalkableLand::Build([](int32_t x, int32_t z) { return (x == 201 && z >= 180 && z <= 220) ? 40.0f : 5.0f; }, Dry);
}

WalkableLand FlatLand()
{
	return WalkableLand::Build(Flat, Dry);
}

} // namespace

TEST(CreatureRoute, SortsTheLand)
{
	const auto land = WalkableLand::Build(
	    [](int32_t x, int32_t /*z*/) {
		    // Deep sea west of x = 100, a steep wall along x = 300, an island cut off beyond x = 400
		    if (x < 100 || (x >= 300 && x <= 301) || x >= 400)
		    {
			    return x < 100 || x >= 420 ? 0.0f : (x == 301 ? 60.0f : 5.0f);
		    }
		    return 5.0f;
	    },
	    [](int32_t x, int32_t z) -> std::optional<bool> {
		    if (z == 0)
		    {
			    return std::nullopt;
		    }
		    return x >= 150 && x < 160;
	    });
	EXPECT_EQ(land.At(50, 50), Ground::Blocked);
	EXPECT_EQ(land.At(200, 50), Ground::Open);
	EXPECT_EQ(land.At(155, 50), Ground::Water);
	// No cell at all counts as water
	EXPECT_EQ(land.At(200, 0), Ground::Water);
	EXPECT_EQ(land.At(300, 50), Ground::Blocked);
	EXPECT_EQ(land.At(350, 50), Ground::CutOff);
	EXPECT_EQ(land.At(-1, 50), Ground::Blocked);
	EXPECT_EQ(land.AtPoint({2005.0f, 505.0f}), Ground::Open);
}

TEST(CreatureRoute, CreaturesStandClearOfBlockedCells)
{
	const auto land = LandWithCliff();
	EXPECT_TRUE(land.IsValid({1000.0f, 1000.0f}, k_Clearance));
	// The ridge's cells are 200 and 201, either side of its high corners; standing in them is not allowed
	EXPECT_FALSE(land.IsValid({2005.0f, 2000.0f}, k_Clearance));
	// Within 7.05 of a blocked cell's centre is not allowed either
	EXPECT_FALSE(land.IsValid({1999.0f, 2005.0f}, k_Clearance));
	EXPECT_TRUE(land.IsValid({1990.0f, 2005.0f}, k_Clearance));
	const auto nearest = land.NearestValid({2005.0f, 2005.0f}, k_Clearance, 100.0f);
	ASSERT_TRUE(nearest.has_value());
	EXPECT_TRUE(land.IsValid(*nearest, k_Clearance));
	EXPECT_LT(glm::distance(*nearest, glm::vec2(2005.0f, 2005.0f)), 20.0f);
}

TEST(CreatureRoute, FollowsTheRoute)
{
	Route route {.legs = {{.from = {0, 0}, .to = {0, 10}}, {.from = {0, 10}, .to = {10, 10}}}, .segment = 0, .travelled = 0.0f};
	EXPECT_NEAR(route.RemainingTotal(), 20.0f, k_Tolerance);
	EXPECT_NEAR(std::abs(route.TurnAtEnd()), k_Pi / 2.0f, k_Tolerance);
	auto advanced = Advance(route, 4.0f);
	EXPECT_FALSE(advanced.segmentChanged);
	EXPECT_NEAR(advanced.position.y, 4.0f, k_Tolerance);
	// Heading along +z
	EXPECT_NEAR(std::abs(advanced.heading), k_Pi, k_Tolerance);
	advanced = Advance(route, 8.0f);
	EXPECT_TRUE(advanced.segmentChanged);
	EXPECT_NEAR(advanced.position.x, 2.0f, k_Tolerance);
	EXPECT_TRUE(route.OnLastSegment());
	advanced = Advance(route, 100.0f);
	EXPECT_TRUE(advanced.finished);
	EXPECT_EQ(advanced.position, glm::vec2(10, 10));
}

TEST(CreatureRoute, FollowsAnArcRoundItsCircle)
{
	// A quarter of the way round a circle of 10 about the middle, anticlockwise from +x to +y
	const Leg arc {.from = {10, 0},
	               .to = {0, 10},
	               .arc = true,
	               .round = Circle {.centre = {0, 0}, .radius = 10.0f},
	               .roundWay = Way::Anticlockwise};
	EXPECT_NEAR(LengthOf(arc), 5.0f * k_Pi, k_Tolerance);
	const auto halfway = PointAlong(arc, 2.5f * k_Pi);
	EXPECT_NEAR(glm::length(halfway), 10.0f, k_Tolerance);
	EXPECT_NEAR(halfway.x, halfway.y, k_Tolerance);
	// Clockwise, the same points are three quarters of the way round
	auto clockwise = arc;
	clockwise.roundWay = Way::Clockwise;
	EXPECT_NEAR(LengthOf(clockwise), 15.0f * k_Pi, k_Tolerance);
	// Leaving the arc along a tangent doesn't turn
	Route route {.legs = {arc, {.from = {0, 10}, .to = {-20, 10}}}, .segment = 0, .travelled = 0.0f};
	EXPECT_NEAR(route.TurnAtEnd(), 0.0f, k_Tolerance);
	const auto points = route.Points(k_Pi / 18.0f);
	ASSERT_GE(points.size(), 10u);
	for (size_t i = 0; i + 1 < points.size(); ++i)
	{
		EXPECT_GE(glm::length(points[i]), 10.0f - k_Tolerance);
	}
}

TEST(CreatureRoute, ACatchStepIsCheckedAgainstTheBlockOfItsHeight)
{
	EXPECT_EQ(StepBlock({100.0f, 30.0f, 3000.0f}), glm::ivec2(1, 0));
	EXPECT_EQ(StepBlock({100.0f, 85.0f, 3000.0f}), glm::ivec2(1, 1));
	EXPECT_EQ(StepBlock({100.0f, -30.0f, 3000.0f}), glm::ivec2(1, 0));
}

TEST(CreatureRoute, AStepBlocksCellsReadARowOn)
{
	// Deep sea along the first three rows of cells
	const auto land = WalkableLand::Build([](int32_t /*x*/, int32_t z) { return z <= 3 ? 0.0f : 5.0f; }, Dry);
	ASSERT_EQ(land.At(3, 2), Ground::Blocked);
	ASSERT_EQ(land.At(3, 3), Ground::Open);
	const glm::vec2 far {3000.0f, 3000.0f};
	const auto circles = StepBlockCircles(land, {0, 0}, far);
	// Each circle's cell is judged by the one a row further on, so the last blocked row has none of its own
	EXPECT_TRUE(InsideAny({35.0f, 20.0f}, circles));
	EXPECT_FALSE(InsideAny({35.0f, 25.0f}, circles));
	EXPECT_FALSE(InsideAny({35.0f, 40.0f}, circles));
	// The circles give way to where the creature stands
	const auto nearby = StepBlockCircles(land, {0, 0}, {35.0f, 18.0f});
	EXPECT_FALSE(InsideAny({35.0f, 20.0f}, nearby));
	EXPECT_TRUE(InsideAny({35.0f, 15.5f}, nearby));
}

TEST(CreatureRoute, AStepIgnoresCellsReachingOffTheLand)
{
	// Deep sea in the last column of cells only
	const auto land = WalkableLand::Build([](int32_t x, int32_t /*z*/) { return x >= k_CellsPerSide - 1 ? 0.0f : 5.0f; }, Dry);
	ASSERT_EQ(land.At(k_CellsPerSide - 1, 20), Ground::Blocked);
	EXPECT_TRUE(StepBlockCircles(land, {63, 1}, {0.0f, 0.0f}).empty());
}
