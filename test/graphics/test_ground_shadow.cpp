/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <array>
#include <vector>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Graphics/GroundShadow.h"

using namespace openblack::graphics;

namespace
{
/// A villager's cast: from the fixed far sun, flattened onto its feet, laid on the ground beneath it
GroundShadow::Cast VillagerCast(float feet, float ground)
{
	return {.light = GroundShadow::k_Sun, .level = feet, .ground = ground};
}

GroundShadow::Point Rigid(const glm::vec3& position)
{
	return {.position = position, .keptAboveLevel = true};
}

glm::vec2 Receive(const glm::vec4& receiver, const glm::vec3& point)
{
	return {(point.x * receiver.x) + receiver.y, (point.z * receiver.z) + receiver.w};
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
	// In radii of the body
	EXPECT_EQ(GroundShadow::Alpha(650.0f, 10.0f), 127);
}

TEST(GroundShadow, ACreaturesOpacityIsTheFadeIn255ths)
{
	EXPECT_EQ(GroundShadow::ScaledAlpha(255.0f), 255);
	EXPECT_EQ(GroundShadow::ScaledAlpha(300.0f), 255);
	EXPECT_EQ(GroundShadow::ScaledAlpha(127.5f), 127);
	EXPECT_EQ(GroundShadow::ScaledAlpha(0.0f), 0);
	// Whole fades are kept whole, rounded down otherwise
	EXPECT_EQ(GroundShadow::ScaledAlpha(100.0f), 100);
	EXPECT_EQ(GroundShadow::ScaledAlpha(99.9f), 99);
}

TEST(GroundShadow, ACreaturesLightIsKeptAtLeast45DegreesUp)
{
	const glm::vec3 standsAt {100.0f, 10.0f, 100.0f};
	// A low sun far away, 10 degrees up
	const auto low = GroundShadow::CreatureLight(standsAt + glm::vec3(1000.0f, 176.3f, 0.0f), standsAt, 2.0f);
	EXPECT_NEAR(low.y - standsAt.y, 1000.0f, 1e-2f);
	EXPECT_NEAR(low.x - standsAt.x, 1000.0f, 1e-2f);
	// A high one is left as it is
	const auto high = GroundShadow::CreatureLight(standsAt + glm::vec3(10.0f, 500.0f, 0.0f), standsAt, 2.0f);
	EXPECT_NEAR(high.y - standsAt.y, 500.0f, 1e-3f);
}

TEST(GroundShadow, ANearCreatureLightIsPushedOutToThreeRadii)
{
	const glm::vec3 standsAt {0.0f, 0.0f, 0.0f};
	const auto pushed = GroundShadow::CreatureLight(glm::vec3(1.0f, 0.5f, 0.0f), standsAt, 2.0f);
	const auto across = std::sqrt((pushed.x * pushed.x) + (pushed.z * pushed.z));
	// Out to 6 units along the way to it, its height too, then raised to 45 degrees
	EXPECT_GT(across, 5.0f);
	EXPECT_NEAR(pushed.y, across, 1e-4f);
	// Straight overhead, it is nudged to a side first
	const auto overhead = GroundShadow::CreatureLight(glm::vec3(0.0f, 1.0f, 0.0f), standsAt, 2.0f);
	EXPECT_GT(overhead.x, 0.0f);
	EXPECT_GT(overhead.z, 0.0f);
	// Only within a tenth of a unit across: one a fifth of a unit away keeps its side
	const auto aside = GroundShadow::CreatureLight(glm::vec3(0.2f, 1.0f, 0.0f), standsAt, 2.0f);
	EXPECT_NEAR(aside.z, 0.0f, 1e-6f);
}

TEST(GroundShadow, TheBodyIsFlattenedAwayFromTheSunOntoItsFeet)
{
	const auto cast = VillagerCast(20.0f, 20.0f);
	// The feet stay where they are
	const auto feet = GroundShadow::OnLevel(Rigid({1000.0f, 20.0f, 2000.0f}), cast);
	EXPECT_NEAR(feet.x, 1000.0f, 1e-3f);
	EXPECT_NEAR(feet.y, 2000.0f, 1e-3f);
	// A head 2 up falls about 2 further along x and z, away from the sun, a little more the further from it
	const auto head = GroundShadow::OnLevel(Rigid({1000.0f, 22.0f, 2000.0f}), cast);
	EXPECT_NEAR(head.x - 1000.0f, 2.0f * 501000.0f / 500000.0f, 1e-2f);
	EXPECT_NEAR(head.y - 2000.0f, 2.0f * 502000.0f / 500000.0f, 1e-2f);
	// Nothing moved with the body whole is lower than the feet
	const auto below = GroundShadow::OnLevel(Rigid({1000.0f, 18.0f, 2000.0f}), cast);
	EXPECT_NEAR(below.x, 1000.0f, 1e-3f);
	// A part moved by bones may be, and falls back towards the sun
	const auto boned = GroundShadow::OnLevel({.position = {1000.0f, 18.0f, 2000.0f}, .keptAboveLevel = false}, cast);
	EXPECT_LT(boned.x, 1000.0f - 1.0f);
}

TEST(GroundShadow, TheLightsHeightIsTakenAsItIs)
{
	// A creature at 100 with its light 30 above it: the light's height is taken as 130, not 30
	const GroundShadow::Cast cast {.light = {0.0f, 130.0f, 0.0f}, .level = 100.0f, .ground = 100.0f};
	const auto head = GroundShadow::OnLevel(Rigid({10.0f, 113.0f, 0.0f}), cast);
	EXPECT_NEAR(head.x, 10.0f * 130.0f / 117.0f, 1e-4f);
}

TEST(GroundShadow, TheFootprintSpansTheFlattenedBody)
{
	const glm::vec3 standsAt {100.0f, 5.0f, 100.0f};
	const auto cast = VillagerCast(standsAt.y, standsAt.y);
	const std::array points {Rigid({99.0f, 5.0f, 100.0f}), Rigid({101.0f, 5.0f, 100.0f}), Rigid({100.0f, 7.0f, 100.0f})};
	const auto footprint = GroundShadow::Measure(points, standsAt, cast);
	ASSERT_TRUE(footprint.has_value());
	EXPECT_NEAR(footprint->low.x, 99.0f, 1e-3f);
	// The head, 2 up, reaches furthest
	EXPECT_NEAR(footprint->high.x, 102.0f, 1e-2f);
	EXPECT_NEAR(footprint->low.y, 100.0f, 1e-3f);
	EXPECT_NEAR(footprint->high.y, 102.0f, 1e-2f);
	// The nearest point to the sun across the ground is the one at the lowest x and z
	EXPECT_NEAR(footprint->nearest, GroundShadow::Reach(points[0].position, standsAt, cast.light), 1e-3f);
	// No points, no shadow; nor for a body with no extent across the ground
	EXPECT_FALSE(GroundShadow::Measure(std::vector<GroundShadow::Point> {}, standsAt, cast).has_value());
	EXPECT_FALSE(GroundShadow::Measure(std::array {Rigid(standsAt)}, standsAt, cast).has_value());
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

TEST(GroundShadow, ATriangleCoversTheSamplesBetweenItsEdgesRowByRow)
{
	GroundShadow::Coverage coverage;
	// Corners on rows 0 and 4, counted the one way round: rows 0 to 3, from x 0 to where the slanted edge crosses
	const std::array<glm::vec2, 3> corners {glm::vec2(0.0f, 0.0f), glm::vec2(8.0f, 4.0f), glm::vec2(0.0f, 4.0f)};
	ASSERT_TRUE(GroundShadow::Counted(corners) || GroundShadow::Counted({corners[0], corners[2], corners[1]}));
	GroundShadow::Cover(coverage, corners, true, false);
	EXPECT_EQ(coverage.rows[0].count(), 0); // the slanted edge starts at x 0 on row 0
	EXPECT_EQ(coverage.rows[1].count(), 2);
	EXPECT_EQ(coverage.rows[3].count(), 6);
	EXPECT_TRUE(coverage.rows[3].test(0));
	EXPECT_FALSE(coverage.rows[3].test(6));
	EXPECT_EQ(coverage.rows[4].count(), 0); // the bottom row is not drawn
}

TEST(GroundShadow, AOneSidedTriangleCountsOnlyFacingOneWay)
{
	const std::array<glm::vec2, 3> corners {glm::vec2(0.0f, 0.0f), glm::vec2(8.0f, 4.0f), glm::vec2(0.0f, 4.0f)};
	const std::array<glm::vec2, 3> turned {corners[0], corners[2], corners[1]};
	GroundShadow::Coverage one;
	GroundShadow::Cover(one, corners, false, false);
	GroundShadow::Coverage other;
	GroundShadow::Cover(other, turned, false, false);
	EXPECT_NE(one.rows[3].count(), other.rows[3].count());
	// Drawn from both sides, both count the same
	GroundShadow::Coverage both;
	GroundShadow::Cover(both, turned, true, false);
	EXPECT_EQ(both.rows[3].count(), 6);
}

TEST(GroundShadow, TheHandCoversOnlyTheSecondRowOfEachPair)
{
	GroundShadow::Coverage coverage;
	const std::array<glm::vec2, 3> square {glm::vec2(4.0f, 2.0f), glm::vec2(8.0f, 4.0f), glm::vec2(4.0f, 4.0f)};
	GroundShadow::Cover(coverage, square, true, true);
	GroundShadow::Cover(coverage, {glm::vec2(4.0f, 2.0f), glm::vec2(8.0f, 2.0f), glm::vec2(8.0f, 4.0f)}, true, true);
	EXPECT_EQ(coverage.rows[2].count(), 0);
	EXPECT_EQ(coverage.rows[3].count(), 4);
	// So a texel it fully covers reaches 4 of 15 levels, what it holds 8
	EXPECT_EQ(GroundShadow::Levels(coverage, 255)[(1 * GroundShadow::k_Texels) + 1], 4);
	GroundShadow::Cover(coverage, square, true, false);
	GroundShadow::Cover(coverage, {glm::vec2(4.0f, 2.0f), glm::vec2(8.0f, 2.0f), glm::vec2(8.0f, 4.0f)}, true, false);
	EXPECT_EQ(GroundShadow::Levels(coverage, 255)[(1 * GroundShadow::k_Texels) + 1], 8);
	// Cut back by the fade
	EXPECT_EQ(GroundShadow::Levels(coverage, 128)[(1 * GroundShadow::k_Texels) + 1], 4);
}

TEST(GroundShadow, TheTexturesRingStaysClear)
{
	GroundShadow::Coverage coverage;
	for (auto& row : coverage.rows)
	{
		row.set();
	}
	const auto levels = GroundShadow::Levels(coverage, 255);
	EXPECT_EQ(levels[0], 0);
	EXPECT_EQ(levels[GroundShadow::k_Texels - 1], 0);
	EXPECT_EQ(levels[(GroundShadow::k_Texels * GroundShadow::k_Texels) - 1], 0);
	EXPECT_EQ(levels[GroundShadow::k_Texels + 1], 8);
}

TEST(GroundShadow, TheOuterRingOfTexelsIsClear)
{
	EXPECT_TRUE(GroundShadow::OnBorder(0, 10));
	EXPECT_TRUE(GroundShadow::OnBorder(10, 31));
	EXPECT_TRUE(GroundShadow::OnBorder(31, 10));
	EXPECT_FALSE(GroundShadow::OnBorder(1, 1));
	EXPECT_FALSE(GroundShadow::OnBorder(30, 30));
}

TEST(GroundShadow, TheLandTakesTheTextureStraightDownFromTheFeet)
{
	const glm::vec3 standsAt {100.0f, 5.0f, 100.0f};
	const GroundShadow::Footprint footprint {
	    .low = glm::vec2(99.0f, 99.0f), .high = glm::vec2(103.0f, 103.0f), .nearest = 0.0f};
	const auto receiver = GroundShadow::Receiver(footprint, VillagerCast(5.0f, 5.0f));
	// Where the villager stands
	const auto under = Receive(receiver, standsAt);
	EXPECT_NEAR(under.x, 0.25f, 1e-4f);
	EXPECT_NEAR(under.y, 0.25f, 1e-4f);
	// The land's own height doesn't move it
	const auto raised = Receive(receiver, standsAt + glm::vec3(0.0f, 30.0f, 0.0f));
	EXPECT_NEAR(raised.x, under.x, 1e-6f);
	EXPECT_NEAR(raised.y, under.y, 1e-6f);
	// Further along the sun's way across the ground reaches further
	EXPECT_GT(GroundShadow::Reach(standsAt + glm::vec3(1.0f, 0.0f, 1.0f), standsAt, GroundShadow::k_Sun),
	          GroundShadow::Reach(standsAt, standsAt, GroundShadow::k_Sun));
}

TEST(GroundShadow, ABodyAboveTheGroundCastsItsShadowAwayFromTheSun)
{
	// Held 10 above the ground, the feet's shadow falls about 10 further along x and z
	const GroundShadow::Footprint footprint {
	    .low = glm::vec2(99.0f, 99.0f), .high = glm::vec2(103.0f, 103.0f), .nearest = 0.0f};
	const auto receiver = GroundShadow::Receiver(footprint, VillagerCast(15.0f, 5.0f));
	const auto shadowed = Receive(receiver, {110.0f, 5.0f, 110.0f});
	EXPECT_NEAR(shadowed.x, 0.25f, 1e-2f);
	EXPECT_NEAR(shadowed.y, 0.25f, 1e-2f);
}

TEST(GroundShadow, ACreaturesShadowFallsStraightDownFromItsLevel)
{
	// Laid at the creature's own level, the texture falls on the land straight below its extent, whatever the light
	const GroundShadow::Footprint footprint {.low = glm::vec2(0.0f, 0.0f), .high = glm::vec2(10.0f, 20.0f), .nearest = 0.0f};
	const GroundShadow::Cast cast {.light = {-30.0f, 80.0f, -30.0f}, .level = 50.0f, .ground = 50.0f};
	const auto receiver = GroundShadow::Receiver(footprint, cast);
	EXPECT_NEAR(Receive(receiver, {5.0f, 0.0f, 5.0f}).x, 0.5f, 1e-6f);
	EXPECT_NEAR(Receive(receiver, {5.0f, 0.0f, 5.0f}).y, 0.25f, 1e-6f);
}

TEST(GroundShadow, ALandBlockTakesTheShadowsItsExtentReaches)
{
	const GroundShadow::Footprint footprint {
	    .low = glm::vec2(150.0f, 10.0f), .high = glm::vec2(170.0f, 20.0f), .nearest = 0.0f};
	EXPECT_TRUE(GroundShadow::ReachesBlock(footprint, {0.0f, 0.0f}));
	EXPECT_TRUE(GroundShadow::ReachesBlock(footprint, {160.0f, 0.0f}));
	EXPECT_FALSE(GroundShadow::ReachesBlock(footprint, {320.0f, 0.0f}));
	EXPECT_FALSE(GroundShadow::ReachesBlock(footprint, {0.0f, 160.0f}));
}
