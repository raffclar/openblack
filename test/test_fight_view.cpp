/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <numbers>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Camera/FightView.h"

using namespace openblack;
using namespace openblack::fight_view;

namespace
{
constexpr float k_Pi = std::numbers::pi_v<float>;
const GroundHeight k_Flat = [](glm::vec2) { return 0.0f; };
constexpr glm::vec3 k_Up {0.0f, 1.0f, 0.0f};
} // namespace

TEST(FightView, HeadingAndPitchUndoPointFrom)
{
	const glm::vec3 focus {10.0f, 2.0f, -4.0f};
	for (const float heading : {0.3f, 1.4f, -2.5f, 3.0f})
	{
		const auto point = PointFrom(focus, 30.0f, heading, 0.6f);
		const auto found = HeadingAndPitch(point, focus);
		EXPECT_NEAR(Wrapped(found.heading - heading), 0.0f, 1e-4f);
		EXPECT_NEAR(found.pitch, 0.6f, 1e-4f);
	}
	// Straight above, it has no heading and looks nearly straight down
	const auto above = HeadingAndPitch(focus + glm::vec3(0.0f, 5.0f, 0.0f), focus);
	EXPECT_EQ(above.heading, 0.0f);
	EXPECT_FLOAT_EQ(above.pitch, 1.5393804f);
}

TEST(FightView, AnglesWrapWithinHalfATurn)
{
	EXPECT_FLOAT_EQ(Wrapped(1.0f), 1.0f);
	EXPECT_NEAR(Wrapped(1.0f + (2.0f * k_Pi)), 1.0f, 1e-5f);
	EXPECT_NEAR(Wrapped(-1.0f - (4.0f * k_Pi)), -1.0f, 1e-4f);
	EXPECT_NEAR(Wrapped(3.5f), 3.5f - (2.0f * k_Pi), 1e-5f);
}

TEST(FightView, OnFlatLandTheBestHeadingIsTheOneGiven)
{
	float pitch = 0.4f;
	const auto heading = BestHeading(1.0f, 40.0f, {0.0f, 0.0f, 0.0f}, pitch, k_Flat, k_Up);
	EXPECT_FLOAT_EQ(heading, 1.0f);
	// The pitch eases: a tenth of the flat land's near upright lean, a fifth of what it was, and a little more
	EXPECT_NEAR(pitch, (1.5393804f * 0.1f) + (0.4f * 0.2f) + 0.37699114f, 1e-5f);
}

TEST(FightView, TheBestHeadingLooksOverLowLand)
{
	// Land falling away towards +z: headings looking from +z, over low land, see the fight best
	const GroundHeight slope = [](glm::vec2 point) { return -point.y; };
	float pitch = 0.4f;
	const auto heading = BestHeading(k_Pi, 40.0f, {0.0f, 0.0f, 0.0f}, pitch, slope, k_Up);
	EXPECT_NEAR(Wrapped(heading), 0.0f, (2.0f * k_Pi / 32.0f) * 3.0f + 1e-4f);
	EXPECT_GE(pitch, k_Pi / 8.0f);
	EXPECT_LE(pitch, k_Pi / 3.0f);
}

TEST(FightView, SuggestedViewsAreEasedBetweenTwentyFiveAndFifty)
{
	const glm::vec3 focus {0.0f, 0.0f, 0.0f};
	const auto near = Suggest({10.0f, 0.0f, 0.0f}, focus, k_Flat, k_Up);
	EXPECT_NEAR(glm::distance(near.origin, focus), 10.0f + (15.0f * 0.8f), 1e-3f);
	const auto far = Suggest({150.0f, 0.0f, 0.0f}, focus, k_Flat, k_Up);
	EXPECT_NEAR(glm::distance(far.origin, focus), 150.0f - (100.0f * 0.1f), 1e-2f);
	const auto start = Start({100.0f, 50.0f}, 30.0f, 5.0f, k_Flat, k_Up);
	EXPECT_EQ(start.focus, glm::vec3(100.0f, 5.0f, 50.0f));
	// From the east, the arena's radius out and half up, a little over 33 away
	EXPECT_NEAR(glm::distance(start.origin, start.focus), std::sqrt((30.0f * 30.0f) + (15.0f * 15.0f)), 1e-3f);
}

TEST(FightView, NearnessToTheArena)
{
	const glm::vec2 centre {0.0f, 0.0f};
	EXPECT_TRUE(WithinArena({5.0f, 0.0f}, {0.0f, 5.0f}, centre, 10.0f));
	EXPECT_FALSE(WithinArena({11.0f, 0.0f}, {0.0f, 5.0f}, centre, 10.0f));
	EXPECT_FALSE(TooFar({30.0f, 0.0f}, {40.0f, 0.0f}, centre, 10.0f, 1.0f));
	EXPECT_TRUE(TooFar({33.0f, 0.0f}, {43.0f, 0.0f}, centre, 10.0f, 1.0f));
	EXPECT_FALSE(TooFar({33.0f, 0.0f}, {41.0f, 0.0f}, centre, 10.0f, 1.0f));
	EXPECT_TRUE(TooFar({0.0f, 0.0f}, {61.0f, 0.0f}, centre, 10.0f, 1.0f));
	EXPECT_FALSE(TooFar({33.0f, 0.0f}, {43.0f, 0.0f}, centre, 10.0f, 2.0f));
}

TEST(FightView, ZoomSpeedsUpWithHeight)
{
	EXPECT_FLOAT_EQ(ZoomDistance(60.0f, 10.0f), 60.0f * 0.0015f * 60.0f);
	EXPECT_FLOAT_EQ(ZoomDistance(60.0f, 100.0f), 60.0f * 0.0015f * 300.0f);
	EXPECT_FLOAT_EQ(ZoomDistance(60.0f, 1000.0f), 60.0f * 0.0015f * 2000.0f);
	EXPECT_FLOAT_EQ(ZoomDistance(60.0f, -1000.0f), 60.0f * 0.0015f * 240.0f);
}

TEST(FightView, TheTrackerKeepsTheFightersSideOn)
{
	Tracker tracker;
	const Fighter first {.position = {0.0f, 0.0f, 0.0f}, .radius = 4.0f, .height = 20.0f};
	const Fighter second {.position = {20.0f, 0.0f, 0.0f}, .radius = 6.0f, .height = 30.0f};
	const auto step = tracker.Follow(first, second, {}, 0.02f);
	EXPECT_FALSE(step.zoomedOut);
	// It looks at their middle, raised by a quarter of their heights
	EXPECT_NEAR(step.view.focus.x, 10.0f, 1e-4f);
	EXPECT_NEAR(step.view.focus.y, 12.5f, 1e-4f);
	EXPECT_NEAR(step.view.focus.z, 0.0f, 1e-4f);
	// As far back as 5.5 of the first's radii, the gap and the second's radius
	EXPECT_NEAR(glm::distance(step.view.origin, step.view.focus), (4.0f * 5.5f) + 20.0f + 6.0f, 1e-3f);
	// Across the line between them: level, the camera is off to the side, not along the line
	const auto away = step.view.origin - step.view.focus;
	EXPECT_NEAR(away.x, 0.0f, 1e-3f);
	EXPECT_GT(std::abs(away.z), 1.0f);
	EXPECT_NEAR(std::asin(away.y / glm::length(away)), k_StartPitch, 1e-4f);
}

TEST(FightView, ZoomingFarOutLeavesTheFight)
{
	Tracker tracker;
	const Fighter first {.position = {0.0f, 0.0f, 0.0f}, .radius = 4.0f, .height = 20.0f};
	const Fighter second {.position = {20.0f, 0.0f, 0.0f}, .radius = 6.0f, .height = 30.0f};
	auto step = tracker.Follow(first, second, {.zoom = 4.0f * 0.3f}, 0.02f);
	EXPECT_FALSE(step.zoomedOut);
	EXPECT_NEAR(tracker.GetScale(), 5.5f + (4.0f * 0.3f * 0.3f / 4.0f), 1e-4f);
	step = tracker.Follow(first, second, {.zoom = 1000.0f}, 0.02f);
	EXPECT_TRUE(step.zoomedOut);
	EXPECT_FLOAT_EQ(tracker.GetScale(), k_MaxScale);
	// Tilting is kept within its bounds
	tracker.Follow(first, second, {.tilt = -10000.0f}, 0.02f);
	EXPECT_FLOAT_EQ(tracker.GetPitch(), k_MaxPitch);
	tracker.Follow(first, second, {.turn = 640.0f, .screenWidth = 1280.0f}, 0.02f);
	EXPECT_NEAR(tracker.GetHeadingOffset(), k_StartHeadingOffset + (k_Pi / 2.0f), 1e-5f);
}
