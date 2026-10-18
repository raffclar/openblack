/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <array>
#include <numbers>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureWalkPlanner.h"

using namespace openblack;
using namespace openblack::creature_walk_planner;
using creature_locomotion::Side;

namespace
{
constexpr float k_Tolerance = 1e-4f;
constexpr float k_Pi = std::numbers::pi_v<float>;

/// A made-up species: each step moves 40 mesh units ahead over 21 frames
StepSet Steps()
{
	return std::array {StepClip {.displacement = {0.0f, 0.0f, -40.0f}, .frames = 21},
	                   StepClip {.displacement = {10.0f, 0.0f, -40.0f}, .frames = 21},
	                   StepClip {.displacement = {0.0f, 0.0f, 40.0f}, .frames = 21}};
}

/// A made-up species: a stand of 2 s, a walk of 2.4 s that strides 70 mesh units, a run of 1 s that strides 130
constexpr Clip k_Stand {.durationMs = 2000, .stride = 0.0f};
constexpr Clip k_Walk {.durationMs = 2400, .stride = 70.0f};
constexpr Clip k_Run {.durationMs = 1000, .stride = 130.0f};
} // namespace

TEST(CreatureWalkPlanner, ItSpeedsUpAtTwelveUnitsASecondEachSecond)
{
	EXPECT_NEAR(NextSpeed(0.0f, 10.0f, 100.0f), 1.2f, k_Tolerance);
	EXPECT_NEAR(NextSpeed(9.5f, 10.0f, 100.0f), 10.0f, k_Tolerance);
	// It never slows down towards a lower target by itself, only the cap slows it
	EXPECT_NEAR(NextSpeed(15.0f, 10.0f, 100.0f), 15.0f, k_Tolerance);
	EXPECT_NEAR(NextSpeed(15.0f, 10.0f, 4.0f), 4.0f, k_Tolerance);
	float speed = 0.0f;
	int turns = 0;
	while (speed < 10.0f)
	{
		speed = NextSpeed(speed, 10.0f, 100.0f);
		++turns;
	}
	EXPECT_EQ(turns, 9);
	EXPECT_NEAR(TurnDistance(10.0f), 1.0f, k_Tolerance);
}

TEST(CreatureWalkPlanner, OnTheLastLegItSlowsToStopAtTheEnd)
{
	// sqrt(2 x 12 x 6) = 12
	EXPECT_NEAR(SpeedCap(30.0f, 10.0f, 4.0f, {.kind = NextLeg::Kind::None}), 12.0f, k_Tolerance);
	EXPECT_NEAR(SpeedCap(30.0f, 10.0f, 10.0f, {.kind = NextLeg::Kind::None}), k_LeastCapSpeed, k_Tolerance);
	// Before an arc it slows to the speed whose square is 12 times the radius
	EXPECT_NEAR(SpeedCap(30.0f, 10.0f, 10.0f, {.kind = NextLeg::Kind::Arc, .radius = 3.0f}), 6.0f, k_Tolerance);
	EXPECT_NEAR(SpeedCap(30.0f, 10.0f, 9.0f, {.kind = NextLeg::Kind::Arc, .radius = 3.0f}), std::sqrt(36.0f + 24.0f),
	            k_Tolerance);
	// Before a straight leg only the top speed limits it
	EXPECT_NEAR(SpeedCap(30.0f, 10.0f, 10.0f, {.kind = NextLeg::Kind::Straight}), 30.0f, k_Tolerance);
}

TEST(CreatureWalkPlanner, UphillIsSlowerAndDownhillALittleFaster)
{
	EXPECT_NEAR(SlopeFactor(10.0f, 10.0f, 5.0f), 1.0f, k_Tolerance);
	EXPECT_NEAR(SlopeFactor(12.5f, 10.0f, 5.0f), 0.7f, k_Tolerance);
	EXPECT_NEAR(SlopeFactor(100.0f, 10.0f, 5.0f), 0.3f, k_Tolerance);
	EXPECT_NEAR(SlopeFactor(0.0f, 10.0f, 5.0f), 1.1f, k_Tolerance);
	// The slope is measured the creature's radius ahead
	const auto ahead = SlopePoint({100.0f, 100.0f}, GameHeading(k_Pi / 2.0f), 5.0f);
	EXPECT_NEAR(ahead.x, 105.0f, k_Tolerance);
	EXPECT_NEAR(ahead.y, 100.0f, k_Tolerance);
}

TEST(CreatureWalkPlanner, ASharpTurnStopsTheWalk)
{
	EXPECT_FALSE(WalkTurnsTooFar(GameHeading(0.0f), GameHeading(0.5f)));
	EXPECT_TRUE(WalkTurnsTooFar(GameHeading(0.0f), GameHeading(k_CornerTurn)));
	EXPECT_FALSE(StepTurnsTooFar(GameHeading(0.0f), GameHeading(k_CornerTurn)));
	EXPECT_TRUE(StepTurnsTooFar(GameHeading(0.0f), GameHeading(0.6f)));
	// While it steps, its heading turns with the route
	EXPECT_NEAR(StepHeading(GameHeading(1.0f), GameHeading(0.2f), GameHeading(0.3f)).Radians(), 1.1f, k_Tolerance);
}

TEST(CreatureWalkPlanner, ItLooksAheadAlongTheRoute)
{
	const auto point = LookAhead({10.0f, 10.0f}, GameHeading(0.0f), 20.0f);
	EXPECT_NEAR(point.x, 10.0f, k_Tolerance);
	EXPECT_NEAR(point.y, -10.0f, k_Tolerance);
	// Never nearer than the least distance
	EXPECT_NEAR(LookAhead({0.0f, 0.0f}, GameHeading(0.0f), 0.0f).y, -k_LeastLookAhead, k_Tolerance);
}

TEST(CreatureWalkPlanner, ItStepsOffWhenThePointIsFurtherThanItsSteps)
{
	// Facing -z, the point is far towards +x, which is on the creature's left: it steps off with the left steps,
	// between the 0 and 90 degree ones
	const auto start = ChooseStart({0.0f, 0.0f}, {100.0f, -100.0f}, GameHeading(0.0f), 1.0f, 9.0f, Steps(), Steps());
	EXPECT_EQ(start.phase, Phase::Step);
	EXPECT_EQ(start.side, Side::Left);
	ASSERT_TRUE(start.steps.has_value());
	EXPECT_EQ(start.steps->from, creature_locomotion::animations::k_LeftStep);
	EXPECT_NEAR(start.steps->weight, 0.5f, k_Tolerance);
	EXPECT_NEAR(start.heading.Radians(), k_Pi / 4.0f, k_Tolerance);
	// The step lasts as long as its move along the new heading takes at walking speed
	EXPECT_GT(start.stepMs, 0);
	// On the right, the right steps
	EXPECT_EQ(ChooseStart({0.0f, 0.0f}, {-100.0f, -100.0f}, GameHeading(0.0f), 1.0f, 9.0f, Steps(), Steps()).side, Side::Right);
}

TEST(CreatureWalkPlanner, NearItWalksStraightOnOrTurnsOnTheSpot)
{
	// Nearer than its steps go and nearly facing the point, it walks straight on
	EXPECT_EQ(ChooseStart({0.0f, 0.0f}, {1.0f, -20.0f}, GameHeading(0.0f), 1.0f, 9.0f, Steps(), Steps()).phase, Phase::Walk);
	// Further round, it turns on the spot first
	EXPECT_EQ(ChooseStart({0.0f, 0.0f}, {20.0f, 0.0f}, GameHeading(0.0f), 1.0f, 9.0f, Steps(), Steps()).phase, Phase::Turn);
	// A species without the steps of the side always turns, even when it faces the point
	EXPECT_EQ(ChooseStart({0.0f, 0.0f}, {0.0f, -500.0f}, GameHeading(0.0f), 1.0f, 9.0f, std::nullopt, Steps()).phase,
	          Phase::Turn);
}

TEST(CreatureWalkPlanner, TheWalkStartsWhereTheStepLeftOff)
{
	EXPECT_EQ(WalkTimeAfterStep(900, 1000, 2000, Side::Right), 1800);
	// After a left step the walk is half a cycle on. The float share 0.4 is a little short, so the time truncates to 799
	EXPECT_EQ(WalkTimeAfterStep(900, 1000, 2000, Side::Left), 799);
}

TEST(CreatureWalkPlanner, LoopedTimesWrapOnceAndNeverReachTheEnd)
{
	EXPECT_EQ(AdvanceLoop(100, 0, 1000), 100);
	EXPECT_EQ(AdvanceLoop(900, 200, 1000), 100);
	EXPECT_EQ(AdvanceLoop(900, 2500, 1000), 999);
	EXPECT_EQ(StandTime(2000, 1.0f), 1999);
}

TEST(CreatureWalkPlanner, SlowerThanWalkingBlendsTheStandAndTheWalk)
{
	const auto gait = BlendGait(4.5f, 9.0f, 22.5f, 0.45f, 0.2f, k_Stand, k_Walk, k_Run, 0, 0.25f);
	ASSERT_TRUE(gait.has_value());
	EXPECT_EQ(gait->animations[0], creature_locomotion::animations::k_Stand);
	EXPECT_EQ(gait->animations[1], creature_locomotion::animations::k_Walk);
	EXPECT_NEAR(gait->weights[0], 0.5f, k_Tolerance);
	EXPECT_EQ(gait->timesMs[0], 500);
	// The blend's stride is half the walk's: 0.5 x 70 x 0.2 = 7 units, so 0.45 units is 0.45 / 7 of the walk
	EXPECT_EQ(gait->timesMs[1], 154);
}

TEST(CreatureWalkPlanner, FasterThanWalkingBlendsTheWalkAndTheRunInStep)
{
	const auto gait = BlendGait(15.0f, 10.0f, 20.0f, 1.5f, 0.1f, k_Stand, k_Walk, k_Run, 2000, 0.0f);
	ASSERT_TRUE(gait.has_value());
	EXPECT_EQ(gait->animations[0], creature_locomotion::animations::k_Run);
	EXPECT_NEAR(gait->weights[0], 0.5f, k_Tolerance);
	// The stride is (0.5 x 70 + 0.5 x 130) x 0.1 = 10 units, so 1.5 units is 15% of the walk
	EXPECT_EQ(gait->timesMs[1], 2000 + 360);
	// The run keeps the walk's phase
	EXPECT_EQ(gait->timesMs[0], (1000 * gait->timesMs[1]) / 2400);
	// Past running speed the run plays past its full weight
	const auto flatOut = BlendGait(22.0f, 10.0f, 20.0f, 2.2f, 0.1f, k_Stand, k_Walk, k_Run, 0, 0.0f);
	ASSERT_TRUE(flatOut.has_value());
	EXPECT_NEAR(flatOut->weights[0], 1.2f, k_Tolerance);
	EXPECT_NEAR(flatOut->weights[1], -0.2f, k_Tolerance);
	// Too slow, nothing changes
	EXPECT_FALSE(BlendGait(0.0f, 10.0f, 20.0f, 0.0f, 0.1f, k_Stand, k_Walk, k_Run, 0, 0.0f).has_value());
}

TEST(CreatureWalkPlanner, ARouteFoundDuringAStepIsJoinedFromTheEndOfTheStep)
{
	// The step's whole move, with its height, at the creature's scale, less the part of it walked: 5 x 2 - 3
	EXPECT_EQ(StepJoinAhead({3.0f, 0.0f, 4.0f}, 2.0f, 3.0f), 7.0f);
	EXPECT_EQ(StepJoinAhead({2.0f, 3.0f, 6.0f}, 1.0f, 0.0f), 7.0f);
	// Past the end of the step it is below 0, and the route is joined where the creature is
	EXPECT_LT(StepJoinAhead({3.0f, 0.0f, 4.0f}, 1.0f, 6.0f), 0.0f);
	// A turn on the spot joins from a little way ahead
	EXPECT_EQ(k_TurnJoinAhead, 0.05f);
}

TEST(CreatureWalkPlanner, AStopSlowsFromTheSpeedToAStand)
{
	// At 12 units a second the walk's slow-down of 12 a second each second needs 6 units
	EXPECT_EQ(StopAhead(12.0f, 0.0f), 6.0f);
	EXPECT_EQ(StopAhead(0.0f, 0.0f), 0.0f);
	// In single precision, in the game's order
	const float speed = 7.3f;
	const float share = speed / 12.0f;
	EXPECT_EQ(StopAhead(speed, 0.0f), ((12.0f * share) * share) * 0.5f);
}

TEST(CreatureWalkPlanner, AStopGoesOnForWhatTheTurnOrStepStillNeeds)
{
	EXPECT_EQ(StopAhead(0.0f, k_TurnJoinAhead), k_TurnJoinAhead);
	EXPECT_EQ(StopAhead(12.0f, 9.0f), 9.0f);
	EXPECT_EQ(StopAhead(12.0f, 3.0f), 6.0f);
	// A distance that is not a number does not count
	EXPECT_EQ(StopAhead(12.0f, std::nanf("")), 6.0f);
}

// A frozen creature lives fewer whole milliseconds in a turn, and a bigger creature plays its animations more slowly
TEST(CreatureWalkPlanner, TheTurnMillisecondsFollowThePaceAndTheSize)
{
	EXPECT_EQ(TurnMs(1.0f, 100), 100);
	EXPECT_EQ(TurnMs(0.0f, 100), 0);
	EXPECT_EQ(TurnMs(0.5f, 100), 50);
	// Cut towards 0, not rounded
	EXPECT_EQ(TurnMs(0.999f, 100), 99);
	EXPECT_EQ(ClipStepMs(100, 0.05f), 157);
	EXPECT_EQ(ClipStepMs(100, 1.0f), 117);
	// Above a size of about 3.76 the rate is less than 0, as in the game
	EXPECT_EQ(ClipStepMs(100, 4.0f), -10);
	EXPECT_EQ(ClipStepMs(0, 1.0f), 0);
	EXPECT_EQ(ClipStepMs(50, 1.0f), 58);
}

// A turn on the spot does not start while the creature moves, or while its body plays an action
TEST(CreatureWalkPlanner, ATurnOnTheSpotWaitsForTheBodyAndTheWalk)
{
	EXPECT_TRUE(MayTurnOnTheSpot(false, false));
	EXPECT_FALSE(MayTurnOnTheSpot(true, false));
	EXPECT_FALSE(MayTurnOnTheSpot(false, true));
	EXPECT_FALSE(MayTurnOnTheSpot(true, true));
}
