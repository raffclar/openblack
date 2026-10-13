/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>

#include <gtest/gtest.h>

#include "Creature/LeashSnap.h"

using namespace openblack;
using namespace openblack::leash_snap;

namespace
{
constexpr float k_Tolerance = 1e-4f;
}

TEST(LeashSnap, TheRopesTouchWidthShrinksWithDepthButNeverBelowTwoPixels)
{
	// 1280 wide, half width 0.15: 1280 x 0.15 x 2 / 10 = 38.4, cut to 38
	EXPECT_EQ(TouchPixels(1280.0f, 0.15f, 10.0f), 38);
	EXPECT_EQ(TouchPixels(1280.0f, 0.15f, 100.0f), 3);
	EXPECT_EQ(TouchPixels(1280.0f, 0.15f, 1000.0f), 2);
}

TEST(LeashSnap, ASegmentIsTouchedAtItsEndsAndAlongIt)
{
	const glm::ivec2 a {100, 100};
	const glm::ivec2 b {200, 100};
	// Inside the disc round each end
	EXPECT_EQ(TouchAlong({103, 101}, a, 5, b, 5), 0.0f);
	EXPECT_EQ(TouchAlong({198, 99}, a, 5, b, 5), 1.0f);
	// Halfway along, just off the line: the share is the cosine of the cursor's direction from a
	const auto along = TouchAlong({150, 103}, a, 5, b, 5);
	ASSERT_TRUE(along.has_value());
	EXPECT_NEAR(*along, 50.0f / std::sqrt(50.0f * 50.0f + 9.0f), k_Tolerance);
	// Too far off it
	EXPECT_FALSE(TouchAlong({150, 110}, a, 5, b, 5).has_value());
	// Outside the box round it
	EXPECT_FALSE(TouchAlong({300, 100}, a, 5, b, 5).has_value());
	// The reach grows from one end to the other
	EXPECT_TRUE(TouchAlong({190, 108}, a, 2, b, 12).has_value());
	// The game's measure between the ends: anything inside the circle on the segment is touched, and near the ends only
	// within the reach there
	EXPECT_TRUE(TouchAlong({125, 119}, a, 2, b, 20).has_value());
	EXPECT_FALSE(TouchAlong({102, 115}, a, 2, b, 20).has_value());
}

TEST(LeashSnap, TheFirstSegmentTouchedGivesTheDepth)
{
	const std::array<ScreenPoint, 3> points {{
	    {.pixel = {100.0f, 100.0f}, .depth = 20.0f, .inFront = true},
	    {.pixel = {200.0f, 100.0f}, .depth = 40.0f, .inFront = true},
	    {.pixel = {300.0f, 100.0f}, .depth = 60.0f, .inFront = true},
	}};
	const auto depth = TouchDepth(points, {200, 100}, 1280.0f, 0.15f);
	ASSERT_TRUE(depth.has_value());
	// Inside the disc round the end of the first segment
	EXPECT_NEAR(*depth, 40.0f, k_Tolerance);
	EXPECT_FALSE(TouchDepth(points, {200, 160}, 1280.0f, 0.15f).has_value());
}

TEST(LeashSnap, PointsBehindOrRightAtTheEyeAreNeverTouched)
{
	std::array<ScreenPoint, 2> points {{
	    {.pixel = {100.0f, 100.0f}, .depth = 1.0f, .inFront = true},
	    {.pixel = {200.0f, 100.0f}, .depth = 40.0f, .inFront = true},
	}};
	EXPECT_FALSE(TouchDepth(points, {150, 100}, 1280.0f, 0.15f).has_value());
	points[0].depth = 20.0f;
	points[1].inFront = false;
	EXPECT_FALSE(TouchDepth(points, {150, 100}, 1280.0f, 0.15f).has_value());
}

TEST(LeashSnap, LettingGoSetsTheLeashsLength)
{
	const auto far = LengthsOnLettingGo({30.0f, 0.0f, 40.0f}, {0.0f, 0.0f, 0.0f}, 3.0f);
	EXPECT_NEAR(far.max, 50.0f, k_Tolerance);
	EXPECT_NEAR(far.slack, 25.0f, k_Tolerance);
	// Never shorter than twice its radius
	const auto near = LengthsOnLettingGo({1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 3.0f);
	EXPECT_NEAR(near.max, 6.0f, k_Tolerance);
	EXPECT_NEAR(near.slack, 3.0f, k_Tolerance);
}

TEST(LeashSnap, TheHeldRopesLengthsAndThePinch)
{
	const auto collar = CollarRopeLengths({.slack = 150.0f, .max = 300.0f});
	EXPECT_NEAR(collar.slack, 225.0f, k_Tolerance);
	EXPECT_NEAR(collar.max, 450.0f, k_Tolerance);
	EXPECT_EQ(k_TiedRopeLengths.slack, 5.0f);
	EXPECT_EQ(k_TiedRopeLengths.max, 2.0f);
	EXPECT_EQ(PinchShare(Hand::Over), 0.0f);
	EXPECT_EQ(PinchShare(Hand::Holding), 0.5f);
}
