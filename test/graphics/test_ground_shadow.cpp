/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <gtest/gtest.h>

#include "Graphics/GroundShadow.h"

using namespace openblack::graphics;

namespace
{
glm::vec3 Receive(const glm::mat4& matrix, const glm::vec3& point)
{
	return glm::vec3(matrix * glm::vec4(point, 1.0f));
}
} // namespace

TEST(GroundShadow, FadesBetween50And80Radii)
{
	EXPECT_EQ(GroundShadow::Alpha(10.0f, 1.0f), 255);
	EXPECT_EQ(GroundShadow::Alpha(49.9f, 1.0f), 255);
	EXPECT_EQ(GroundShadow::Alpha(65.0f, 1.0f), 127);
	EXPECT_EQ(GroundShadow::Alpha(80.0f, 1.0f), 0);
	EXPECT_EQ(GroundShadow::Alpha(81.0f, 1.0f), 0);
	EXPECT_EQ(GroundShadow::Alpha(10.0f, 0.0f), 0);
}

TEST(GroundShadow, TheBodyIsFlattenedAwayFromTheSunOntoItsFeet)
{
	// The feet stay where they are
	const auto feet = GroundShadow::OnGround({1000.0f, 20.0f, 2000.0f}, 20.0f);
	EXPECT_NEAR(feet.x, 1000.0f, 1e-3f);
	EXPECT_NEAR(feet.y, 2000.0f, 1e-3f);
	// A head 2 up falls about 2 further along x and z, away from the sun, a little more the further from it
	const auto head = GroundShadow::OnGround({1000.0f, 22.0f, 2000.0f}, 20.0f);
	EXPECT_NEAR(head.x - 1000.0f, 2.0f * 501000.0f / 500000.0f, 1e-2f);
	EXPECT_NEAR(head.y - 2000.0f, 2.0f * 502000.0f / 500000.0f, 1e-2f);
	// Nothing is lower than the feet
	const auto below = GroundShadow::OnGround({1000.0f, 18.0f, 2000.0f}, 20.0f);
	EXPECT_NEAR(below.x, 1000.0f, 1e-3f);
}

TEST(GroundShadow, TheFootprintSpansTheFlattenedBody)
{
	const glm::vec3 standsAt {100.0f, 5.0f, 100.0f};
	const std::array points {glm::vec3(99.0f, 5.0f, 100.0f), glm::vec3(101.0f, 5.0f, 100.0f), glm::vec3(100.0f, 7.0f, 100.0f)};
	const auto footprint = GroundShadow::Measure(points, standsAt);
	ASSERT_TRUE(footprint.has_value());
	EXPECT_NEAR(footprint->low.x, 99.0f, 1e-3f);
	// The head, 2 up, reaches furthest
	EXPECT_NEAR(footprint->high.x, 102.0f, 1e-2f);
	EXPECT_NEAR(footprint->low.y, 100.0f, 1e-3f);
	EXPECT_NEAR(footprint->high.y, 102.0f, 1e-2f);
	// The nearest point to the sun across the ground is the one at the lowest x and z
	EXPECT_NEAR(footprint->nearest, GroundShadow::Reach(points[0], standsAt), 1e-3f);
	// No points, no shadow; nor for a body with no extent across the ground
	EXPECT_FALSE(GroundShadow::Measure(std::vector<glm::vec3> {}, standsAt).has_value());
	EXPECT_FALSE(GroundShadow::Measure(std::array {standsAt}, standsAt).has_value());
}

TEST(GroundShadow, SamplesAreClampedIntoTheGridAndRowsRoundedDown)
{
	const GroundShadow::Footprint footprint {.low = glm::vec2(0.0f), .high = glm::vec2(128.0f, 64.0f), .nearest = 0.0f};
	const auto inside = GroundShadow::Sample({10.25f, 20.75f}, footprint);
	EXPECT_FLOAT_EQ(inside.x, 10.25f);
	EXPECT_FLOAT_EQ(inside.y, 20.0f);
	// The far edges are pulled in to the last sample and row
	const auto far = GroundShadow::Sample({128.0f, 64.0f}, footprint);
	EXPECT_FLOAT_EQ(far.x, 127.0f);
	EXPECT_FLOAT_EQ(far.y, 63.0f);
	const auto before = GroundShadow::Sample({-3.0f, -3.0f}, footprint);
	EXPECT_FLOAT_EQ(before.x, 0.0f);
	EXPECT_FLOAT_EQ(before.y, 0.0f);
}

TEST(GroundShadow, OnlyTrianglesFacingOneWayAreCounted)
{
	const std::array<glm::vec2, 3> one {glm::vec2(0.0f, 0.0f), glm::vec2(10.0f, 0.0f), glm::vec2(0.0f, 10.0f)};
	const std::array<glm::vec2, 3> other {one[0], one[2], one[1]};
	EXPECT_NE(GroundShadow::Counted(one), GroundShadow::Counted(other));
	// A triangle with no area is counted
	EXPECT_TRUE(GroundShadow::Counted({glm::vec2(0.0f), glm::vec2(1.0f, 1.0f), glm::vec2(2.0f, 2.0f)}));
}

TEST(GroundShadow, ATexelReachesEightOfFifteenLevelsCutBackByTheFade)
{
	EXPECT_EQ(GroundShadow::Level(8, 255), 8);
	EXPECT_EQ(GroundShadow::Level(3, 255), 3);
	EXPECT_EQ(GroundShadow::Level(8, 128), 4);
	EXPECT_EQ(GroundShadow::Level(8, 127), 3);
	EXPECT_EQ(GroundShadow::Level(1, 254), 0);
	EXPECT_EQ(GroundShadow::Level(0, 255), 0);
}

TEST(GroundShadow, TheOuterRingOfTexelsIsClear)
{
	EXPECT_TRUE(GroundShadow::OnBorder(0, 10));
	EXPECT_TRUE(GroundShadow::OnBorder(10, 31));
	EXPECT_TRUE(GroundShadow::OnBorder(31, 10));
	EXPECT_FALSE(GroundShadow::OnBorder(1, 1));
	EXPECT_FALSE(GroundShadow::OnBorder(30, 30));
}

TEST(GroundShadow, SampleCornersLandOnTheirSlot)
{
	// The first sample's row runs along the top of the slot, half a texel in; the corner half a texel to the left
	const auto first = GroundShadow::SlotClip({0.5f, 0.0f}, 0, 4, false);
	EXPECT_FLOAT_EQ(first.x, -1.0f);
	EXPECT_FLOAT_EQ(first.y, 1.0f - (1.0f / 64.0f));
	// Upside down where the texture's origin is at its bottom
	EXPECT_FLOAT_EQ(GroundShadow::SlotClip({0.5f, 0.0f}, 0, 4, true).y, -1.0f + (1.0f / 64.0f));
	// The second slot starts a quarter of the way across a target four slots wide
	EXPECT_FLOAT_EQ(GroundShadow::SlotClip({0.5f, 0.0f}, 1, 4, false).x, -0.5f);
}

TEST(GroundShadow, TheLandTakesTheTextureStraightDownFromTheFeet)
{
	const glm::vec3 standsAt {100.0f, 5.0f, 100.0f};
	const GroundShadow::Footprint footprint {
	    .low = glm::vec2(99.0f, 99.0f), .high = glm::vec2(103.0f, 103.0f), .nearest = 0.0f};
	const auto matrix = GroundShadow::ReceiverMatrix(footprint, standsAt, 5.0f);
	// Where the villager stands
	const auto under = Receive(matrix, standsAt);
	EXPECT_NEAR(under.x, 0.25f, 1e-4f);
	EXPECT_NEAR(under.y, 0.25f, 1e-4f);
	// The land's own height doesn't move it
	const auto raised = Receive(matrix, standsAt + glm::vec3(0.0f, 30.0f, 0.0f));
	EXPECT_NEAR(raised.x, under.x, 1e-6f);
	EXPECT_NEAR(raised.y, under.y, 1e-6f);
	// Further along the sun's way across the ground reaches further
	EXPECT_GT(Receive(matrix, standsAt + glm::vec3(1.0f, 0.0f, 1.0f)).z, under.z);
	EXPECT_NEAR(under.z, GroundShadow::Reach(standsAt, standsAt), 1e-3f);
}

TEST(GroundShadow, ABodyAboveTheGroundCastsItsShadowAwayFromTheSun)
{
	// Held 10 above the ground, the feet's shadow falls about 10 further along x and z
	const glm::vec3 standsAt {100.0f, 15.0f, 100.0f};
	const GroundShadow::Footprint footprint {
	    .low = glm::vec2(99.0f, 99.0f), .high = glm::vec2(103.0f, 103.0f), .nearest = 0.0f};
	const auto matrix = GroundShadow::ReceiverMatrix(footprint, standsAt, 5.0f);
	const auto shadowed = Receive(matrix, {110.0f, 5.0f, 110.0f});
	EXPECT_NEAR(shadowed.x, 0.25f, 1e-2f);
	EXPECT_NEAR(shadowed.y, 0.25f, 1e-2f);
}
