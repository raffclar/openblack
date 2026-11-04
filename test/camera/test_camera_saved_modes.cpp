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

#include <optional>
#include <tuple>

#include <Camera/CreatureCameraModel.h>
#include <Camera/CreatureFollow.h>
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

namespace
{
constexpr float k_QuarterTurn = 1.5707963705062866f;
const creature_follow::Offsets k_Offsets {.focus = {0.0f, 5.0f, 0.0f}, .eye = {30.0f, 20.0f, -30.0f}};
} // namespace

TEST(SavedFollowCamera, ACreatureWithTheGameAngleZeroTurnsTheHeadingAQuarterTurnOn)
{
	EXPECT_EQ(creature_follow::RelativeHeading(0.5f, 0), 0.5f + k_QuarterTurn);
	// A quarter of the circle cancels the quarter turn, and a half takes it off
	EXPECT_EQ(creature_follow::RelativeHeading(0.5f, 512), 0.5f);
	EXPECT_EQ(creature_follow::RelativeHeading(0.5f, 1024), 0.5f - k_QuarterTurn);
}

TEST(SavedFollowCamera, TheRelativeHeadingRoundsEachStepToAFloat)
{
	const uint16_t angle = 777;
	const float turned = static_cast<float>(angle * 2) * 0.0015339808305725455f;
	const float facing = turned - k_QuarterTurn;
	EXPECT_EQ(creature_follow::RelativeHeading(1.25f, angle), 1.25f - facing);
}

TEST(SavedFollowCamera, TheDistanceOfTheOffsetsIsTheDistanceBetweenTheirPoints)
{
	EXPECT_EQ(creature_follow::OffsetDistance(k_Offsets), 45.0f);
}

TEST(SavedFollowCamera, TheEyeOfTheOffsetsMovesAlongItsLineToADistance)
{
	const auto moved = creature_follow::OffsetsAtDistance(k_Offsets, 90.0f);
	EXPECT_EQ(moved.focus, k_Offsets.focus);
	EXPECT_NEAR(moved.eye.x, 60.0f, 1e-4f);
	EXPECT_NEAR(moved.eye.y, 35.0f, 1e-4f);
	EXPECT_NEAR(moved.eye.z, -60.0f, 1e-4f);
	// An eye on the point that the camera looks at stays on it
	const auto still = creature_follow::OffsetsAtDistance({.focus = {1.0f, 2.0f, 3.0f}, .eye = {1.0f, 2.0f, 3.0f}}, 50.0f);
	EXPECT_EQ(still.eye, glm::vec3(1.0f, 2.0f, 3.0f));
}

TEST(SavedFollowCamera, WithoutTheCursorKeysTheOffsetsKeepTheirLineAtTheDistanceOfTheView)
{
	creature_follow::View view {.yaw = 0.0f, .pitch = 0.5f, .distance = 1200.0f};
	std::optional<creature_follow::Offsets> offsets = creature_follow::OffsetsAtDistance(k_Offsets, 1200.0f);
	ASSERT_EQ(creature_follow::Apply(view, offsets, {}, 1000.0f), creature_follow::KeyOutcome::Stay);
	ASSERT_TRUE(offsets.has_value());
	// The keys bring the camera no further out than 1000
	EXPECT_EQ(view.distance, creature_follow::k_MaxKeyDistance);
	EXPECT_NEAR(creature_follow::OffsetDistance(*offsets), creature_follow::k_MaxKeyDistance, 1e-2f);
}

TEST(SavedFollowCamera, TheCursorKeysStopTheOffsetsAndTurnFromTheirView)
{
	creature_follow::View view {.yaw = 0.0f, .pitch = 0.5f, .distance = 45.0f};
	std::optional<creature_follow::Offsets> offsets = k_Offsets;
	auto keys = creature_follow::KeysFor(0, -1, 0.25f);
	keys.shift = true;
	ASSERT_EQ(creature_follow::Apply(view, offsets, keys, 1000.0f), creature_follow::KeyOutcome::Stay);
	EXPECT_FALSE(offsets.has_value());
	const auto turned = creature_follow::HeadingPitchOf(k_Offsets.eye, k_Offsets.focus);
	EXPECT_FLOAT_EQ(view.yaw, turned.heading);
	EXPECT_NEAR(view.pitch, turned.pitch + 0.2f, 1e-5f);
	EXPECT_FLOAT_EQ(view.distance, 45.0f);
}

TEST(SavedFollowCamera, TheCameraWithOffsetsStandsAndLooksAtTheirPointsOnTheThing)
{
	const creature_follow::Loaded loaded {.view = {.yaw = 1.0f, .pitch = 0.4f, .distance = 60.0f}, .offsets = k_Offsets};
	CreatureCameraModel model(loaded, k_Point, 10.0f);
	EXPECT_EQ(model.GetTargetFocus(), k_Point + k_Offsets.focus);
	EXPECT_EQ(model.GetTargetOrigin(), k_Point + k_Offsets.eye);
	const glm::vec3 moved = k_Point + glm::vec3(5.0f, 0.0f, 5.0f);
	model.SetTarget(moved, 10.0f);
	EXPECT_EQ(model.GetTargetOrigin(), moved + k_Offsets.eye);
}

TEST(SavedFollowCamera, TheHeadingTurnsOnlyWithAThingThatWalks)
{
	const creature_follow::Loaded loaded {.view = {.yaw = 1.0f, .pitch = 0.4f, .distance = 60.0f}, .relativeHeading = true};
	CreatureCameraModel model(loaded, k_Point, 10.0f);
	const auto focus = creature_follow::Focus(k_Point, 10.0f);
	// A thing that does not walk keeps the heading of the file
	EXPECT_EQ(model.GetTargetOrigin(), creature_follow::Origin(focus, loaded.view));
	model.SetTarget(k_Point, 10.0f, uint16_t {300});
	auto turned = loaded.view;
	turned.yaw = creature_follow::RelativeHeading(1.0f, 300);
	EXPECT_EQ(model.GetTargetOrigin(), creature_follow::Origin(focus, turned));
	EXPECT_EQ(model.GetView().yaw, 1.0f);
}

TEST(SavedFollowCamera, ACameraOfAPlayerDoesNotTurnWithTheThing)
{
	CreatureCameraModel model({.view = {.yaw = 1.0f, .pitch = 0.4f, .distance = 60.0f}}, k_Point, 10.0f);
	model.SetTarget(k_Point, 10.0f, uint16_t {300});
	EXPECT_EQ(model.GetTargetOrigin(),
	          creature_follow::Origin(creature_follow::Focus(k_Point, 10.0f), {.yaw = 1.0f, .pitch = 0.4f, .distance = 60.0f}));
}

TEST(SavedFollowCamera, AFlockIsFollowedAtTheMapPlaceOfItsLeader)
{
	const auto ground = [](glm::vec2 /*point*/) { return 7.0f; };
	const creature_follow::FlockLeader leader {.position = {1000.00005f, 31.5f, 2000.3f}, .height = 4.0f};
	const auto target = creature_follow::FlockTarget(leader, {.x = 1, .z = 2, .altitude = 3.0f}, ground);
	// x and z go through a map position, the height of the leader stays
	EXPECT_EQ(target.position.x, map_coords::ToMetres(map_coords::ToFixed(1000.00005f)));
	EXPECT_EQ(target.position.y, 31.5f);
	EXPECT_EQ(target.position.z, map_coords::ToMetres(map_coords::ToFixed(2000.3f)));
	EXPECT_EQ(target.height, 4.0f);
	EXPECT_FALSE(target.gameAngle.has_value());
	EXPECT_FALSE(target.hasModel);
}

TEST(SavedFollowCamera, AFlockWithNoLeaderIsFollowedAtItsOwnPlaceOnTheLand)
{
	const auto ground = [](glm::vec2 point) { return point.x * 0.01f; };
	const map_coords::MapCoords place {.x = 0x640000, .z = 0x320000, .altitude = 2.5f};
	const auto target = creature_follow::FlockTarget(std::nullopt, place, ground);
	EXPECT_EQ(target.position, glm::vec3(1000.0f, 12.5f, 500.0f));
	EXPECT_EQ(target.height, 0.0f);
	EXPECT_FALSE(target.hasModel);
}

TEST(SavedFollowCamera, OnAThingWithNoModelTheOffsetsGiveOnlyTheDistance)
{
	const creature_follow::Loaded loaded {.view = {.yaw = 1.0f, .pitch = 0.4f, .distance = 60.0f}, .offsets = k_Offsets};
	CreatureCameraModel model(loaded, k_Point, 10.0f);
	model.SetTarget({.position = k_Point, .height = 10.0f, .gameAngle = std::nullopt, .hasModel = false});
	const auto focus = creature_follow::Focus(k_Point, 10.0f);
	auto view = loaded.view;
	view.distance = creature_follow::OffsetDistance(k_Offsets);
	EXPECT_EQ(model.GetTargetFocus(), focus);
	EXPECT_EQ(model.GetTargetOrigin(), creature_follow::Origin(focus, view));
	// A thing with a model takes the offsets again
	model.SetTarget({.position = k_Point, .height = 10.0f, .gameAngle = std::nullopt, .hasModel = true});
	EXPECT_EQ(model.GetTargetOrigin(), k_Point + k_Offsets.eye);
}
