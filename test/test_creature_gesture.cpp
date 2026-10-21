/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <array>
#include <utility>

#include <gtest/gtest.h>

#include "Creature/CreatureGesture.h"
#include "Creature/CreaturePointing.h"

using namespace openblack;

namespace
{
constexpr float k_Near = 1e-4f;
} // namespace

// The arm blends its four animations by where the target is across and up
TEST(CreaturePointing, ATargetStraightAheadAndLevelBlendsAllFourEqually)
{
	using namespace creature_pointing;
	const auto aim = AimAt({0.0f, 15.0f, 0.0f}, {0.0f, -1.0f}, {0.0f, 15.0f, -40.0f});
	EXPECT_NEAR(aim.across, 0.0f, k_Near);
	EXPECT_NEAR(aim.rise, 0.0f, k_Near);
	const auto weights = Weights(aim, false);
	for (const auto weight : weights)
	{
		EXPECT_NEAR(weight, 0.25f, 1e-3f);
	}
}

// Each part of the aim stops at its limit: 30 degrees across, a sine of a half up
TEST(CreaturePointing, TheAimStopsAtItsLimits)
{
	using namespace creature_pointing;
	// Facing along -z, a target far along +x and high above
	const auto aim = AimAt({0.0f, 0.0f, 0.0f}, {0.0f, -1.0f}, {100.0f, 500.0f, 0.0f});
	EXPECT_NEAR(aim.across, -k_AcrossLimit, k_Near);
	EXPECT_NEAR(aim.rise, k_RiseLimit, k_Near);
	// Fully to that side and high: only the high animation of that side counts
	const auto weights = Weights(aim, false);
	EXPECT_NEAR(weights[2], 1.0f, 1e-3f);
	EXPECT_NEAR(weights[0] + weights[1] + weights[3], 0.0f, 1e-3f);
}

// A target nearer than a tenth of a unit gives a level aim straight ahead
TEST(CreaturePointing, ATargetTooNearAimsAhead)
{
	using namespace creature_pointing;
	const auto aim = creature_pointing::AimAt({1.0f, 2.0f, 3.0f}, {1.0f, 0.0f}, {1.05f, 2.0f, 3.0f});
	EXPECT_EQ(aim.across, 0.0f);
	EXPECT_EQ(aim.rise, 0.0f);
}

// Without a turn, the left arm points at a target round to the positive side of the head; after a turn, at a target
// on the positive side of the aim. A creature that holds a thing uses its free hand.
TEST(CreaturePointing, TheArmIsTheFreeOneOrTheOneOnTheTargetsSide)
{
	using namespace creature_pointing;
	EXPECT_TRUE(PointsWithLeft(0.2f, std::nullopt));
	EXPECT_FALSE(PointsWithLeft(-0.2f, std::nullopt));
	EXPECT_FALSE(PointsWithLeft(0.0f, std::nullopt));
	EXPECT_TRUE(PointsWithLeft(-0.2f, false));
	EXPECT_FALSE(PointsWithLeft(0.2f, true));
	EXPECT_TRUE(PointsWithLeftAfterTurn({.across = 0.2f}, std::nullopt));
	EXPECT_FALSE(PointsWithLeftAfterTurn({.across = -0.2f}, std::nullopt));
	EXPECT_TRUE(PointsWithLeftAfterTurn({.across = 0.2f}, false));
	// The left arm turns the aim over: its weights are those of the right arm for the target on the other side
	const auto left = Weights({.across = 0.3f, .rise = 0.1f}, true);
	const auto right = Weights({.across = -0.3f, .rise = 0.1f}, false);
	for (size_t i = 0; i < left.size(); ++i)
	{
		EXPECT_FLOAT_EQ(left.at(i), right.at(i));
	}
}

// The turn angle is seen from the head: a target a quarter turn round along +x, facing along -z, is a quarter turn
TEST(CreaturePointing, TheTurnAngleIsSeenFromTheHead)
{
	using namespace creature_pointing;
	EXPECT_NEAR(TurnAngle({5.0f, 10.0f, 0.0f}, {0.0f, -1.0f}, {15.0f, 0.0f, 0.0f}), 1.5707964f, k_Near);
	EXPECT_NEAR(TurnAngle({0.0f, 10.0f, 0.0f}, {0.0f, -1.0f}, {0.0f, 0.0f, -10.0f}), 0.0f, k_Near);
}

// The arm rises to the moment of the point and holds there; when the point ends, it plays on
TEST(CreaturePointing, TheTimeHoldsAtThePointUntilThePointEnds)
{
	using namespace creature_pointing;
	EXPECT_FLOAT_EQ(AdvanceTime(100.0f, 40.0f, 500.0f, false), 140.0f);
	// A step that passes the moment is taken whole
	EXPECT_FLOAT_EQ(AdvanceTime(480.0f, 40.0f, 500.0f, false), 520.0f);
	EXPECT_FLOAT_EQ(AdvanceTime(520.0f, 40.0f, 500.0f, false), 520.0f);
	EXPECT_FLOAT_EQ(AdvanceTime(520.0f, 40.0f, 500.0f, true), 560.0f);
}

// The point at the route: the arm moves on by the clip step until it is at the moment of the point or past it, and
// round the animation once at most when the point ends
TEST(CreaturePointing, ThePointAtTheRouteHoldsThenPlaysToItsEnd)
{
	using namespace creature_pointing;
	EXPECT_FLOAT_EQ(RoutePointTime(100.0f, 117, 500.0f, false, 1000), 217.0f);
	EXPECT_FLOAT_EQ(RoutePointTime(480.0f, 117, 500.0f, false, 1000), 597.0f);
	EXPECT_FLOAT_EQ(RoutePointTime(597.0f, 117, 500.0f, false, 1000), 597.0f);
	EXPECT_FLOAT_EQ(RoutePointTime(597.0f, 117, 500.0f, true, 1000), 714.0f);
	// Past the end it goes round once, and never stops at the end
	EXPECT_FLOAT_EQ(RoutePointTime(950.0f, 117, 500.0f, true, 1000), 67.0f);
	EXPECT_FLOAT_EQ(RoutePointTime(883.0f, 117, 500.0f, true, 1000), 0.0f);
	// The point is over when the arm with the step gets to the end of the animation, once it faces the point
	EXPECT_FALSE(RoutePointOver(true, true, 882.0f, 117, 1000.0f));
	EXPECT_TRUE(RoutePointOver(true, true, 883.0f, 117, 1000.0f));
	EXPECT_FALSE(RoutePointOver(false, true, 990.0f, 117, 1000.0f));
	EXPECT_FALSE(RoutePointOver(true, false, 990.0f, 117, 1000.0f));
}

// The aim and the turn from the heading as the game keeps it agree with the aim from the way the creature faces
TEST(CreaturePointing, TheAimFromAHeading)
{
	using namespace creature_pointing;
	const glm::vec3 origin {0.0f, 15.0f, 0.0f};
	const glm::vec3 target {10.0f, 20.0f, -30.0f};
	const auto heading = GameHeading(0.3f);
	const glm::vec2 facing {std::sin(0.3f), -std::cos(0.3f)};
	const auto byHeading = AimAt(origin, heading, target);
	const auto byFacing = AimAt(origin, facing, target);
	EXPECT_NEAR(byHeading.across, byFacing.across, k_Near);
	EXPECT_FLOAT_EQ(byHeading.rise, byFacing.rise);
	EXPECT_NEAR(TurnAngle(origin, heading, target), TurnAngle(origin, facing, target), k_Near);
	EXPECT_NEAR(TurnAngle(origin, heading, target), -byHeading.across, k_Near);
}

// The game's angles: 0 along -z, a quarter turn along +x, and one full turn at most to bring an angle back
TEST(CreaturePointing, TheGamesAngles)
{
	using namespace creature_pointing;
	EXPECT_NEAR(AngleOf({0.0f, -1.0f}), 0.0f, k_Near);
	EXPECT_NEAR(AngleOf({1.0f, 0.0f}), 1.5707964f, k_Near);
	EXPECT_NEAR(Wrap(4.0f), 4.0f - 6.2831855f, k_Near);
	EXPECT_NEAR(Wrap(-4.0f), -4.0f + 6.2831855f, k_Near);
	EXPECT_FLOAT_EQ(Wrap(1.0f), 1.0f);
}

// A shape's point is on the line between the two points either side of its place, cut towards the start
TEST(CreatureGesture, ThePathIsSampledBetweenItsPoints)
{
	using namespace creature_gesture;
	const std::array<glm::vec3, 3> points {glm::vec3 {0.0f, 0.0f, 0.0f}, glm::vec3 {1.0f, 0.0f, 0.0f},
	                                       glm::vec3 {1.0f, 0.0f, 1.0f}};
	EXPECT_EQ(SamplePath(points, 0.0f), points[0]);
	EXPECT_EQ(SamplePath(points, -1.0f), points[0]);
	EXPECT_EQ(SamplePath(points, 1.0f), points[2]);
	EXPECT_EQ(SamplePath(points, 2.0f), points[2]);
	const auto quarter = SamplePath(points, 0.25f);
	EXPECT_NEAR(quarter.x, 0.5f, k_Near);
	const auto threeQuarters = SamplePath(points, 0.75f);
	EXPECT_NEAR(threeQuarters.x, 1.0f, k_Near);
	EXPECT_NEAR(threeQuarters.z, 0.5f, k_Near);
}

// The square stands two radii ahead of the head; x goes to the left side, z from the top down to the head's height
TEST(CreatureGesture, TheShapeStandsInFrontOfTheHead)
{
	using namespace creature_gesture;
	const glm::vec3 head {10.0f, 20.0f, 30.0f};
	// Facing along -z (the zero heading) with a radius of 5: the square is 6.5 wide and 10 ahead
	const auto topStart = HandTarget(head, GameHeading(0.0f), 5.0f, {0.0f, 0.0f, 0.0f});
	EXPECT_NEAR(topStart.x, 10.0f, k_Near);
	EXPECT_NEAR(topStart.y, 26.5f, k_Near);
	EXPECT_NEAR(topStart.z, 20.0f, k_Near);
	const auto bottomSide = HandTarget(head, GameHeading(0.0f), 5.0f, {1.0f, 0.0f, 1.0f});
	EXPECT_NEAR(bottomSide.x, 3.5f, k_Near);
	EXPECT_NEAR(bottomSide.y, 20.0f, k_Near);
	EXPECT_NEAR(bottomSide.z, 20.0f, k_Near);
}

// A shape as long as the circle takes thirty turns; twice as long, sixty
TEST(CreatureGesture, TheShareEachTurnFollowsTheLengthOfTheShape)
{
	using namespace creature_gesture;
	EXPECT_FLOAT_EQ(SharePerTurn(3.0f, 3.0f), 1.0f / 30.0f);
	EXPECT_FLOAT_EQ(SharePerTurn(6.0f, 3.0f), 1.0f / 60.0f);
}

// The drawing waits for the arm, draws the shape, holds at its end for eight more turns, then is done
TEST(CreatureGesture, ADrawingWaitsDrawsHoldsAndEnds)
{
	using namespace creature_gesture;
	Drawing drawing {.drawn = 0.0f, .turns = 100};
	EXPECT_EQ(Advance(drawing, 4, false, 0.25f), Outcome::Waiting);
	EXPECT_EQ(drawing.drawn, 0.0f);
	uint32_t drawingTurns = 0;
	auto outcome = Outcome::Drawing;
	while ((outcome = Advance(drawing, 4, true, 0.25f)) == Outcome::Drawing)
	{
		++drawingTurns;
		ASSERT_LT(drawingTurns, 100u);
	}
	EXPECT_EQ(outcome, Outcome::Done);
	// Four turns draw the shape; the fourth reaches its end, then eight more turns hold it
	EXPECT_EQ(drawingTurns, 4u + 7u);
	EXPECT_FLOAT_EQ(drawing.drawn, 1.0f);
}

// Soon after the counter went back to 0, the drawing waits at its start until the counter gets to eight
TEST(CreatureGesture, ADrawingStartsOnlyOnceTheCounterGetsToEight)
{
	using namespace creature_gesture;
	Drawing drawing {.drawn = 0.0f, .turns = 0};
	for (uint32_t turn = 1; turn < k_HoldTurns; ++turn)
	{
		EXPECT_EQ(Advance(drawing, 4, true, 0.1f), Outcome::Waiting);
	}
	EXPECT_EQ(Advance(drawing, 4, true, 0.1f), Outcome::Drawing);
}

// A gesture past the last that a creature can draw fails
TEST(CreatureGesture, AGesturePastTheLastFails)
{
	using namespace creature_gesture;
	Drawing drawing {.drawn = 0.0f, .turns = 100};
	EXPECT_EQ(Advance(drawing, k_LastGesture + 1, true, 0.1f), Outcome::Failed);
}

// In the turn that the point at the route ends, the events of the end animation from the arm's time to the end sound
TEST(CreaturePointing, TheEndOfAPointAtTheRouteSoundsTheRestOfItsLastAnimation)
{
	using namespace creature_pointing;
	EXPECT_EQ(RoutePointEndSounds(883.9f, 117, 1000.0f), std::pair(883, 1000));
	EXPECT_EQ(RoutePointEndSounds(500.0f, 117, 1000.0f), std::pair(500, 617));
}
