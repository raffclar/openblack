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
#include <numbers>
#include <span>

#include <gtest/gtest.h>

#include "ECS/WallHugRules.h"

using namespace openblack::ecs::wall_hug;

namespace
{
/// A villager's usual walk: 4600 whole map units a turn
constexpr float k_Speed = 4600.0f * 10.0f / 65536.0f * k_TurnsPerSecond;

CircleSweepInput Around(glm::vec2 position, glm::vec2 goal, bool clockwise, std::span<const BlockingCircle> blockers = {})
{
	return {
	    .position = position,
	    .goal = goal,
	    .centre = {0.0f, 0.0f},
	    .radius = 5.0f,
	    .clockwise = clockwise,
	    .speed = k_Speed,
	    .blockers = blockers,
	};
}

glm::vec2 OnCircle(float degrees, float radius = 5.0f)
{
	const float radians = degrees * std::numbers::pi_v<float> / 180.0f;
	return glm::vec2(std::cos(radians), std::sin(radians)) * radius;
}
} // namespace

TEST(WallHugRules, AWalkerGoesATenthOfItsSpeedEachTurn)
{
	EXPECT_EQ(WholeSpeed(k_Speed), 4600);
	EXPECT_FLOAT_EQ(StepMetres(k_Speed), 0.701904296875f);
	// One metre a second is 655 whole units a turn, about a tenth of a metre
	EXPECT_EQ(WholeSpeed(1.0f), 655);
	EXPECT_NEAR(StepMetres(1.0f), 0.1f, 0.0001f);
	EXPECT_EQ(WholeSpeed(-1.0f), 0);
	EXPECT_EQ(WholeSpeed(1000.0f), k_MaxWholeSpeed);
	EXPECT_FLOAT_EQ(OrbitTurn(k_Speed, 5.0f), 0.701904296875f / 5.0f);
}

TEST(WallHugRules, AGoalInsideTheCircleEndsTheWalkRoundAtItsBearing)
{
	// The goal is a quarter turn ahead going anticlockwise, three quarters going clockwise; a turn walks 0.7 m of the
	// arc and the look ahead counts it at two thirds of that
	const auto anticlockwise = SweepCircle(Around(OnCircle(0.0f), {0.0f, 2.0f}, false));
	EXPECT_EQ(anticlockwise.outcome, CircleSweep::Outcome::Hug);
	EXPECT_FALSE(anticlockwise.hugged.has_value());
	EXPECT_EQ(anticlockwise.turnsToObstacle, 7);
	ASSERT_TRUE(anticlockwise.heading.has_value());

	const auto clockwise = SweepCircle(Around(OnCircle(0.0f), {0.0f, 2.0f}, true));
	EXPECT_EQ(clockwise.outcome, CircleSweep::Outcome::Hug);
	EXPECT_EQ(clockwise.turnsToObstacle, 22);
}

TEST(WallHugRules, AtTheGoalsBearingTheWalkerStepsStraightIn)
{
	const auto sweep = SweepCircle(Around(OnCircle(89.5f), {0.0f, 2.0f}, false));
	EXPECT_EQ(sweep.outcome, CircleSweep::Outcome::StepThrough);
	EXPECT_FALSE(sweep.heading.has_value());
}

TEST(WallHugRules, WithTheGoalOutsideAndNothingInTheWayTheWalkerKeepsHugging)
{
	const auto sweep = SweepCircle(Around(OnCircle(0.0f), {20.0f, 20.0f}, false));
	EXPECT_EQ(sweep.outcome, CircleSweep::Outcome::Hug);
	EXPECT_EQ(sweep.turnsToObstacle, k_NoObstacleInReach);
	ASSERT_TRUE(sweep.heading.has_value());
	// On the circle, a tenth of a radius in: it faces a quarter turn round less an eighth of a quarter for each radius
	// out, here anticlockwise from the bearing to the middle (pointing back along -x)
	const float expected = std::numbers::pi_v<float> - (std::numbers::pi_v<float> / 2.0f) * (1.0f - 0.1f / 8.0f);
	EXPECT_NEAR(*sweep.heading, expected, 0.0001f);
}

TEST(WallHugRules, AnOverlappingCircleAheadEndsTheWalkRound)
{
	// The neighbour cuts the hugged circle at a bearing of about 37 degrees; going clockwise from 90 the walker has 53
	// degrees to go
	const std::array<BlockingCircle, 1> neighbour {
	    {{.centre = {8.0f, 0.0f}, .radius = 5.0f, .landscape = false, .landscapeOrFence = false}}};
	const auto sweep = SweepCircle(Around(OnCircle(90.0f), {20.0f, 0.0f}, true, neighbour));
	EXPECT_EQ(sweep.outcome, CircleSweep::Outcome::Hug);
	EXPECT_FALSE(sweep.hugged.has_value());
	EXPECT_EQ(sweep.turnsToObstacle, 4);
}

TEST(WallHugRules, AtTheOverlapTheWalkerIsHandedOverToTheNeighbour)
{
	const std::array<BlockingCircle, 1> neighbour {
	    {{.centre = {8.0f, 0.0f}, .radius = 5.0f, .landscape = false, .landscapeOrFence = false}}};
	const auto sweep = SweepCircle(Around(OnCircle(37.5f), {20.0f, 0.0f}, true, neighbour));
	EXPECT_EQ(sweep.outcome, CircleSweep::Outcome::Hug);
	ASSERT_TRUE(sweep.hugged.has_value());
	EXPECT_EQ(*sweep.hugged, 0U);
	EXPECT_FALSE(sweep.needsLookahead);
}

TEST(WallHugRules, WaterNeverCutsShortTheWalkRoundToAGoalInside)
{
	const std::array<BlockingCircle, 1> water {
	    {{.centre = {6.0f, 6.0f}, .radius = k_LandscapeBlockerRadius, .landscape = true, .landscapeOrFence = true}}};
	const auto sweep = SweepCircle(Around(OnCircle(0.0f), {0.0f, 2.0f}, false, water));
	EXPECT_EQ(sweep.outcome, CircleSweep::Outcome::Hug);
	EXPECT_FALSE(sweep.hugged.has_value());
	EXPECT_EQ(sweep.turnsToObstacle, 7);
}

TEST(WallHugRules, AWalkerLeavesTheCircleOnlyNearerItsGoalAndWithTheGoalAhead)
{
	// 10 m from the goal on starting round: 1279 hundred-and-twenty-eighths
	const auto entry = EntryDistance(10.0f);
	EXPECT_EQ(entry, 1279U);
	EXPECT_EQ(EntryDistance(0.0f), 0U);
	const glm::vec2 goal {0.0f, 0.0f};
	// Nearer, with the goal ahead and off to the side away from an anticlockwise turn: going anticlockwise it leaves
	EXPECT_TRUE(LeavesCircle({-5.0f, 0.0f}, goal, {0.5f, 0.5f}, entry, false));
	EXPECT_FALSE(LeavesCircle({-5.0f, 0.0f}, goal, {0.5f, 0.5f}, entry, true));
	// The goal behind it
	EXPECT_FALSE(LeavesCircle({-5.0f, 0.0f}, goal, {-0.5f, 0.5f}, entry, false));
	// No nearer than on starting round
	EXPECT_FALSE(LeavesCircle({-11.0f, 0.0f}, goal, {0.5f, 0.5f}, entry, false));
}
