/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <numbers>

#include <gtest/gtest.h>

#include "Creature/CreatureHeadLook.h"

using namespace openblack::creature_head_look;

namespace
{
constexpr float k_Tolerance = 1e-4f;
constexpr float k_Pi = std::numbers::pi_v<float>;
} // namespace

TEST(CreatureHeadLook, TheHeadTurnsOnlyInSomePoses)
{
	EXPECT_TRUE(AllowsLook(Pose::Standing));
	EXPECT_TRUE(AllowsLook(Pose::Moving));
	EXPECT_TRUE(AllowsLook(Pose::TurningToFace));
	EXPECT_TRUE(AllowsLook(Pose::Sitting));
	EXPECT_TRUE(AllowsLook(Pose::WaitingToCatch));
	EXPECT_FALSE(AllowsLook(Pose::TurningToWalk));
	EXPECT_FALSE(AllowsLook(Pose::SittingDown));
	EXPECT_FALSE(AllowsLook(Pose::GettingUp));
	EXPECT_FALSE(AllowsLook(Pose::FightStance));
	EXPECT_FALSE(AllowsLook(Pose::Other));
}

TEST(CreatureHeadLook, APointHoldsTheHeadStraightInTheMiddleOfTheClip)
{
	// A clip of 1005 ms: two tenths is 201 ms and seven tenths is 703 ms, in whole milliseconds
	EXPECT_TRUE(AllowsLook(Pose::Pointing, 0, 1005));
	EXPECT_TRUE(AllowsLook(Pose::Pointing, 200, 1005));
	EXPECT_FALSE(AllowsLook(Pose::Pointing, 201, 1005));
	EXPECT_FALSE(AllowsLook(Pose::Pointing, 500, 1005));
	EXPECT_FALSE(AllowsLook(Pose::Pointing, 703, 1005));
	EXPECT_TRUE(AllowsLook(Pose::Pointing, 704, 1005));
}

TEST(CreatureHeadLook, InTheFightStanceTheHeadLooksAtTheOpponent)
{
	// The stance does not let the head turn to the look point, but the opponent's head overrides that
	EXPECT_EQ(AimOf({.pose = Pose::FightStance, .hasLookPoint = true, .hasOpponent = true}), Aim::OpponentHead);
	EXPECT_EQ(AimOf({.pose = Pose::FightStance, .hasLookPoint = false, .hasOpponent = true}), Aim::OpponentHead);
	EXPECT_EQ(AimOf({.pose = Pose::FightStance, .hasLookPoint = true, .hasOpponent = false}), Aim::Ahead);
}

TEST(CreatureHeadLook, FightMovesKeepTheHeadStraight)
{
	// A blow, a block or a cast is not the stance: the old look point does not turn the head
	EXPECT_EQ(AimOf({.pose = Pose::Other, .hasLookPoint = true, .hasOpponent = true}), Aim::Ahead);
}

TEST(CreatureHeadLook, TheHeadLooksAtTheLookPointWhenThePoseAllows)
{
	EXPECT_EQ(AimOf({.pose = Pose::Standing, .hasLookPoint = true}), Aim::LookPoint);
	EXPECT_EQ(AimOf({.pose = Pose::Moving, .hasLookPoint = true}), Aim::LookPoint);
	EXPECT_EQ(AimOf({.pose = Pose::Standing, .hasLookPoint = false}), Aim::Ahead);
	EXPECT_EQ(AimOf({.pose = Pose::TurningToWalk, .hasLookPoint = true}), Aim::Ahead);
	EXPECT_EQ(AimOf({.pose = Pose::Pointing, .hasLookPoint = true, .pointTimeMs = 100, .pointDurationMs = 1000}),
	          Aim::LookPoint);
	EXPECT_EQ(AimOf({.pose = Pose::Pointing, .hasLookPoint = true, .pointTimeMs = 400, .pointDurationMs = 1000}), Aim::Ahead);
}

TEST(CreatureHeadLook, ACreatureWaitingToCatchLooksAtTheThing)
{
	EXPECT_EQ(AimOf({.pose = Pose::WaitingToCatch, .hasLookPoint = false, .hasThing = true}), Aim::Thing);
	EXPECT_EQ(AimOf({.pose = Pose::WaitingToCatch, .hasLookPoint = true, .hasThing = false}), Aim::LookPoint);
	// The thing counts only while it waits to catch it
	EXPECT_EQ(AimOf({.pose = Pose::Other, .hasLookPoint = false, .hasThing = true}), Aim::Ahead);
}

TEST(CreatureHeadLook, AnglesTowardsATarget)
{
	const glm::vec3 eyes {0.0f, 15.0f, 0.0f};
	// The heading 0 faces -z
	const auto straight = AnglesTowards(eyes, openblack::GameHeading(0.0f), {0.0f, 15.0f, -10.0f});
	EXPECT_NEAR(straight.yaw, 0.0f, k_Tolerance);
	EXPECT_NEAR(straight.pitch, 0.0f, k_Tolerance);
	// Facing -z in the left-handed world, +x is on the creature's left
	const auto left = AnglesTowards(eyes, openblack::GameHeading(0.0f), {10.0f, 15.0f, -10.0f});
	EXPECT_NEAR(left.yaw, k_Pi / 4.0f, k_Tolerance);
	const auto right = AnglesTowards(eyes, openblack::GameHeading(0.0f), {-10.0f, 15.0f, -10.0f});
	EXPECT_NEAR(right.yaw, -k_Pi / 4.0f, k_Tolerance);
	const auto up = AnglesTowards(eyes, openblack::GameHeading(0.0f), {0.0f, 25.0f, -10.0f});
	EXPECT_NEAR(up.pitch, k_Pi / 4.0f, k_Tolerance);
}

TEST(CreatureHeadLook, ATargetAlongAMapAxisGivesNoSidewaysTurn)
{
	const glm::vec3 eyes {0.0f, 15.0f, 0.0f};
	// Straight to the side along x, the head does not turn sideways, but it still turns down
	const auto side = AnglesTowards(eyes, openblack::GameHeading(0.0f), {-10.0f, 5.0f, 0.0f});
	EXPECT_FLOAT_EQ(side.yaw, 0.0f);
	EXPECT_NEAR(side.pitch, -k_Pi / 4.0f, k_Tolerance);
	// Facing along x, a target along z is also on an axis
	const auto behind = AnglesTowards(eyes, openblack::GameHeading::FromYaw(k_Pi / 2.0f), {0.0f, 15.0f, 10.0f});
	EXPECT_FLOAT_EQ(behind.yaw, 0.0f);
}

TEST(CreatureHeadLook, TheYawIsFromTheHeadingTheBodyFaces)
{
	const glm::vec3 eyes {0.0f, 15.0f, 0.0f};
	// The same target seen from a body turned a quarter turn
	const auto turned = AnglesTowards(eyes, openblack::GameHeading::FromYaw(k_Pi / 4.0f), {10.0f, 15.0f, -10.0f});
	EXPECT_NEAR(turned.yaw, k_Pi / 2.0f, k_Tolerance);
}

TEST(CreatureHeadLook, BiggerCreaturesTurnTheirHeadsMoreSlowly)
{
	EXPECT_NEAR(Acceleration(0.0f, 28.0f), 1.6f * 28.0f, k_Tolerance);
	EXPECT_NEAR(Acceleration(1.0f, 28.0f), (1.6f - 0.425f) * 28.0f, k_Tolerance);
	EXPECT_LT(Acceleration(2.0f, 28.0f), Acceleration(1.0f, 28.0f));
}

TEST(CreatureHeadLook, SittingClipsPlayForTheWholeSit)
{
	EXPECT_TRUE(UsesSittingClips(Pose::SittingDown));
	EXPECT_TRUE(UsesSittingClips(Pose::Sitting));
	EXPECT_TRUE(UsesSittingClips(Pose::GettingUp));
	EXPECT_FALSE(UsesSittingClips(Pose::Standing));
}

TEST(CreatureHeadLook, HeadClipsPlayWhenTurnedOrWithALookPoint)
{
	EXPECT_TRUE(PlaysHeadClip(0.1f, false));
	EXPECT_TRUE(PlaysHeadClip(0.0f, true));
	EXPECT_FALSE(PlaysHeadClip(0.0f, false));
	EXPECT_FALSE(PlaysHeadClip(5e-5f, false));
}

TEST(CreatureHeadLook, WaitingForItsRouteItLooksAheadAlongTheSearch)
{
	const auto ground = [](glm::vec2 at) { return at.x * 0.5f; };
	// A new walk looks at once: with no search route yet, at the destination's first coordinate on both axes
	auto look = StepRouteLook(RouteLook {}, 16, {.destination = {100.0f, 300.0f}}, ground);
	EXPECT_EQ(look.countdownMs, k_RouteLookMs);
	EXPECT_FLOAT_EQ(look.point.x, 100.0f);
	EXPECT_FLOAT_EQ(look.point.z, 100.0f);
	EXPECT_FLOAT_EQ(look.point.y, 50.0f);
	// The point holds until its time is up, then moves to where the search has got to
	look = StepRouteLook(look, 2000, {.searchHead = glm::vec2(40.0f, 60.0f)}, ground);
	EXPECT_FLOAT_EQ(look.point.x, 100.0f);
	EXPECT_EQ(look.countdownMs, 0);
	look = StepRouteLook(look, 1, {.searchHead = glm::vec2(40.0f, 60.0f)}, ground);
	EXPECT_FLOAT_EQ(look.point.x, 40.0f);
	EXPECT_FLOAT_EQ(look.point.z, 60.0f);
	EXPECT_FLOAT_EQ(look.point.y, 20.0f);
}
