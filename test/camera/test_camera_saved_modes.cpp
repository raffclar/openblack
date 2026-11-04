/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <tuple>

#include <Camera/FollowHeading.h>
#include <Camera/FreeCamera.h>
#include <Camera/ScriptCameraModel.h>
#include <glm/geometric.hpp>
#include <gtest/gtest.h>

using namespace openblack;

namespace
{
constexpr glm::vec3 k_Point {1000.0f, 20.0f, 2000.0f};
constexpr float k_Distance = 50.0f;
constexpr float k_Pitch = 0.4f;
} // namespace

TEST(FollowHeadingCamera, OnlyTheTurningActionsOfACreatureTurnTooFast)
{
	for (const uint32_t action : {8U, 82U, 88U, 89U, 90U, 92U})
	{
		EXPECT_TRUE(follow_heading::TurnsTooFast(action)) << action;
	}
	for (const uint32_t action : {0U, 7U, 9U, 81U, 83U, 87U, 91U, 93U})
	{
		EXPECT_FALSE(follow_heading::TurnsTooFast(action)) << action;
	}
}

TEST(FollowHeadingCamera, TheGlidesTakeTwoSecondsAtTheStartAndOneSecondAfterTwoSeconds)
{
	EXPECT_EQ(follow_heading::GlideSeconds(0.0f), 2.0f);
	EXPECT_EQ(follow_heading::GlideSeconds(1.0f), 1.5f);
	EXPECT_EQ(follow_heading::GlideSeconds(2.0f), 1.0f);
	EXPECT_EQ(follow_heading::GlideSeconds(5.0f), 1.0f);
}

TEST(FollowHeadingCamera, TheCameraStandsBehindTheWayTheThingFacesPlusTheAddedHeading)
{
	follow_heading::Settings settings {.heading = 0.5f, .pitch = k_Pitch, .distance = k_Distance, .headingSpeed = 0.25f};
	float held = 0.0f;
	const auto view = follow_heading::Step(settings, {.point = k_Point, .facing = 1.0f, .turnsTooFast = false}, 3.0f, held);
	// The added heading turns by its speed first
	EXPECT_EQ(settings.heading, 0.75f);
	EXPECT_EQ(held, follow_heading::k_FullTurn - (1.0f + 0.75f));
	EXPECT_EQ(view.focus, k_Point);
	EXPECT_EQ(view.origin, script_camera::PointFromDistanceHeadingAndPitch(k_Point, k_Distance, held, k_Pitch));
	EXPECT_EQ(view.seconds, 1.0f);
}

TEST(FollowHeadingCamera, TheAddedHeadingAndTheTurnComeBackByAFullTurn)
{
	follow_heading::Settings settings {.heading = 6.2f, .pitch = k_Pitch, .distance = k_Distance, .headingSpeed = 0.2f};
	float held = 0.0f;
	std::ignore = follow_heading::Step(settings, {.point = k_Point, .facing = 7.0f, .turnsTooFast = false}, 0.0f, held);
	const float added = (0.2f + 6.2f) - follow_heading::k_FullTurn;
	EXPECT_EQ(settings.heading, added);
	const float facing = 7.0f - follow_heading::k_FullTurn;
	float turn = facing + added;
	if (turn > follow_heading::k_FullTurn)
	{
		turn = turn - follow_heading::k_FullTurn;
	}
	EXPECT_EQ(held, follow_heading::k_FullTurn - turn);
}

TEST(FollowHeadingCamera, AThingThatTurnsTooFastKeepsTheHeldHeading)
{
	follow_heading::Settings settings {.heading = 0.0f, .pitch = k_Pitch, .distance = k_Distance, .headingSpeed = 0.1f};
	float held = 2.5f;
	const auto view = follow_heading::Step(settings, {.point = k_Point, .facing = 1.0f, .turnsTooFast = true}, 1.0f, held);
	EXPECT_EQ(held, 2.5f);
	// The added heading still turns
	EXPECT_EQ(settings.heading, 0.1f);
	EXPECT_EQ(view.origin, script_camera::PointFromDistanceHeadingAndPitch(k_Point, k_Distance, 2.5f, k_Pitch));
	EXPECT_EQ(view.seconds, 1.5f);
}

TEST(FollowHeadingCamera, TheCameraLooksDownAtLeastAHundredthOfAFullTurn)
{
	follow_heading::Settings settings {.heading = 0.0f, .pitch = -0.3f, .distance = k_Distance, .headingSpeed = 0.0f};
	float held = 0.0f;
	const auto view = follow_heading::Step(settings, {.point = k_Point, .facing = 0.0f, .turnsTooFast = false}, 0.0f, held);
	EXPECT_EQ(view.origin,
	          script_camera::PointFromDistanceHeadingAndPitch(k_Point, k_Distance, held, follow_heading::k_LowestPitch));
	// The saved pitch does not change
	EXPECT_EQ(settings.pitch, -0.3f);
}

TEST(FreeCamera, TheKeysCountAWholeNumberAndLeftAndForwardsWin)
{
	free_camera::State state;
	free_camera::ReadKeys(state, {.left = true, .right = true, .forwards = true, .backwards = true}, 16);
	// 0.016 seconds times 400 is 6.4, cut to 6
	EXPECT_EQ(state.across, -6);
	EXPECT_EQ(state.along, -6);
	free_camera::ReadKeys(state, {.right = true, .backwards = true, .shift = true}, 17);
	EXPECT_EQ(state.across, 6);
	EXPECT_EQ(state.along, 6);
	EXPECT_TRUE(state.shift);
	EXPECT_FALSE(state.ctrl);
	free_camera::ReadKeys(state, {}, 17);
	EXPECT_EQ(state.across, 0);
	EXPECT_EQ(state.along, 0);
}

TEST(FreeCamera, AStillCameraKeepsItsPlaceAndItsView)
{
	const free_camera::View now {.origin = {1000.0f, 100.0f, 1000.0f}, .focus = {1000.0f, 50.0f, 1100.0f}};
	free_camera::State state;
	const auto next = free_camera::Step(now, state, {.milliseconds = 16, .screen = {800, 600}});
	EXPECT_EQ(next.origin, now.origin);
	EXPECT_NEAR(glm::distance(next.focus, now.focus), 0.0f, 1e-3f);
}

TEST(FreeCamera, TheSpeedFallsWithTimeAndMovesTheCameraAlongItsView)
{
	const free_camera::View now {.origin = {1000.0f, 100.0f, 1000.0f}, .focus = {1000.0f, 100.0f, 1100.0f}};
	free_camera::State state {.speed = 10.0f};
	const auto next = free_camera::Step(now, state, {.milliseconds = 100, .screen = {800, 600}});
	// e to the power of -0.5, times 10
	EXPECT_NEAR(state.speed, 6.0653066f, 1e-5f);
	EXPECT_NEAR(next.origin.z, 1000.0f + (state.speed * free_camera::k_SpeedDistance), 1e-3f);
	EXPECT_NEAR(next.origin.x, 1000.0f, 1e-4f);
	EXPECT_NEAR(glm::distance(next.origin, next.focus), 100.0f, 1e-3f);
}

TEST(FreeCamera, AGripOfTheLandSpeedsTheCameraUpAndTheMouseTurnsIt)
{
	const free_camera::View now {.origin = {1000.0f, 100.0f, 1000.0f}, .focus = {1000.0f, 100.0f, 1100.0f}};
	free_camera::State state;
	// The mouse at the right edge of the screen, level with its middle
	const auto next =
	    free_camera::Step(now, state, {.milliseconds = 100, .gripsLand = true, .mouse = {800, 300}, .screen = {800, 600}});
	EXPECT_NEAR(state.speed, 0.1f, 1e-6f);
	const auto before = script_camera::HeadingAndPitchFromPoints(now.focus, now.origin);
	const auto after = script_camera::HeadingAndPitchFromPoints(next.focus, next.origin);
	// Half the screen, for a tenth of a second, times 1.1
	const float turned = std::atan2(std::sin(after.heading - before.heading), std::cos(after.heading - before.heading));
	EXPECT_NEAR(turned, 0.5f * 0.1f * free_camera::k_MouseTurn, 1e-4f);
	EXPECT_NEAR(after.pitch, before.pitch, 1e-4f);
}

TEST(FreeCamera, CtrlAndTheKeysMoveTheCameraToTheSide)
{
	const free_camera::View now {.origin = {1000.0f, 100.0f, 1000.0f}, .focus = {1000.0f, 100.0f, 1100.0f}};
	free_camera::State state {.across = 20, .ctrl = true};
	const auto next = free_camera::Step(now, state, {.milliseconds = 16, .screen = {800, 600}});
	// Looking along +z, the right is +x
	EXPECT_NEAR(next.origin.x, 1000.0f + (20.0f * free_camera::k_KeyMove), 1e-4f);
	EXPECT_NEAR(next.origin.z, 1000.0f, 1e-4f);
	EXPECT_NEAR(next.focus.x, 1000.0f + (20.0f * free_camera::k_KeyMove), 1e-3f);
	EXPECT_EQ(state.across, 0);
}

TEST(FreeCamera, ShiftAndTheKeysTiltTheCameraNoFurtherThanThree)
{
	const free_camera::View now {.origin = {1000.0f, 100.0f, 1000.0f}, .focus = {1000.0f, 100.0f, 1100.0f}};
	free_camera::State state {.along = -2000, .shift = true};
	const auto next = free_camera::Step(now, state, {.milliseconds = 16, .screen = {800, 600}});
	EXPECT_EQ(next.focus, script_camera::PointFromDistanceHeadingAndPitch(
	                          next.origin, 100.0f, script_camera::HeadingAndPitchFromPoints(now.focus, now.origin).heading,
	                          free_camera::k_HighestPitch));
	EXPECT_EQ(state.along, 0);
}
