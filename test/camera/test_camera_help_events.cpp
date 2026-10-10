/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <chrono>
#include <optional>
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

#include "Camera/CameraHelp.h"
#include "Camera/CameraHelpEvents.h"
#include "Camera/ZoomToPlaces.h"
#include "Help/HelpProfile.h"

using namespace openblack::camera_help;
using namespace openblack::camera_help::events;
namespace profile = openblack::help::profile;

namespace
{
constexpr uint32_t k_AllFeatures = 0x1FF & ~feature::k_AutoPitch;

std::vector<uint32_t> List(EventSet events)
{
	std::vector<uint32_t> list;
	events.ForEach([&list](uint32_t event) { list.push_back(event); });
	return list;
}

ControlsFrame Frame()
{
	return {.features = k_AllFeatures};
}
} // namespace

TEST(CameraHelpEvents, TheSetKeepsOnlyTheCameraEventsLowestFirst)
{
	EventSet events;
	events.Add(profile::k_CameraDrag);
	events.Add(profile::k_CameraTurn);
	events.Add(profile::k_CameraTurn);
	events.Add(24);
	events.Add(34);
	EXPECT_EQ(List(events), (std::vector<uint32_t> {25, 33}));
	EXPECT_TRUE(events.Contains(33));
	EXPECT_FALSE(events.Contains(34));
	EXPECT_TRUE(EventSet {}.Empty());
}

TEST(CameraHelpEvents, NothingAskedForCountsNothing)
{
	EXPECT_TRUE(InputEvents(Frame()).Empty());
	EXPECT_FALSE(IsMoving(Frame()));
}

TEST(CameraHelpEvents, TurningCountsWithItsWay)
{
	auto frame = Frame();
	frame.turn = 2.0f;
	EXPECT_EQ(List(InputEvents(frame)), (std::vector<uint32_t> {25, 26}));
	frame.turn = -2.0f;
	EXPECT_EQ(List(InputEvents(frame)), (std::vector<uint32_t> {25, 27}));
}

TEST(CameraHelpEvents, ATurnOfAHundredthOrLessIsNoTurn)
{
	auto frame = Frame();
	frame.turn = 0.01f;
	EXPECT_TRUE(InputEvents(frame).Empty());
	frame.turn = 0.0101f;
	EXPECT_TRUE(InputEvents(frame).Contains(profile::k_CameraTurn));
}

TEST(CameraHelpEvents, FeaturesTakenAwayAreNotCounted)
{
	auto frame = Frame();
	frame.turn = 2.0f;
	frame.tilt = 2.0f;
	frame.zoom = 2.0f;
	frame.move = {1.0f, 0.0f};
	EXPECT_EQ(List(InputEvents(frame)), (std::vector<uint32_t> {25, 26, 28, 29, 33}));
	frame.features = feature::k_WatchFights;
	EXPECT_TRUE(InputEvents(frame).Empty());
}

TEST(CameraHelpEvents, TheSelfTiltingCameraTakesTheTiltOverUnlessTheLandIsGripped)
{
	auto frame = Frame();
	frame.features |= feature::k_AutoPitch;
	frame.tilt = 2.0f;
	EXPECT_TRUE(InputEvents(frame).Empty());
	frame.landGripped = true;
	EXPECT_EQ(List(InputEvents(frame)), (std::vector<uint32_t> {28}));
}

TEST(CameraHelpEvents, GrippingWithoutTurningTiltingOrZoomingGivesTheKeysMovementUp)
{
	auto frame = Frame();
	frame.landGripped = true;
	frame.move = {1.0f, 0.0f};
	EXPECT_FALSE(AnyInput(frame));
	EXPECT_TRUE(GripDrags(frame));
	EXPECT_TRUE(InputEvents(frame).Empty());
	// Turning while gripping keeps the keys and their movement, and the grip doesn't drag
	frame.turn = 2.0f;
	EXPECT_TRUE(AnyInput(frame));
	EXPECT_FALSE(GripDrags(frame));
	EXPECT_EQ(List(InputEvents(frame)), (std::vector<uint32_t> {25, 26, 33}));
}

TEST(CameraHelpEvents, TurningRoundTheMouseOrBothButtonsLetTheGripGo)
{
	auto frame = Frame();
	frame.landGripped = true;
	frame.bothButtons = true;
	EXPECT_FALSE(GripDrags(frame));
	frame.bothButtons = false;
	frame.rotateAroundMouse = true;
	EXPECT_FALSE(GripDrags(frame));
	// Turning round the mouse counts as asking for something, so the keys' movement counts
	frame.move = {0.0f, 1.0f};
	EXPECT_EQ(List(InputEvents(frame)), (std::vector<uint32_t> {33}));
}

TEST(CameraHelpEvents, TheMouseMovesTheCameraOnceItMovesMoreThanTwoPixels)
{
	auto frame = Frame();
	frame.landGripped = true;
	frame.mouseDelta = {2, -2};
	EXPECT_FALSE(IsMoving(frame));
	frame.mouseDelta = {0, -3};
	EXPECT_TRUE(IsMoving(frame));
	// Without a grip, round the mouse turning or both buttons the mouse moves nothing
	frame.landGripped = false;
	EXPECT_FALSE(IsMoving(frame));
	frame.bothButtons = true;
	EXPECT_TRUE(IsMoving(frame));
}

TEST(CameraHelpEvents, TurningRoundTheEdgeCountsItsWayEvenWhenStill)
{
	auto frame = Frame();
	frame.landGripped = true;
	EXPECT_EQ(List(EdgeTurnEvents(frame, 0.2f)), (std::vector<uint32_t> {26}));
	// A still cursor counts as turning the negative way
	EXPECT_EQ(List(EdgeTurnEvents(frame, 0.0f)), (std::vector<uint32_t> {27}));
	EXPECT_EQ(List(EdgeTurnEvents(frame, 0.01f)), (std::vector<uint32_t> {27}));
	frame.mouseDelta = {5, 0};
	EXPECT_EQ(List(EdgeTurnEvents(frame, -0.2f)), (std::vector<uint32_t> {25, 27}));
	frame.features &= ~feature::k_Rotate;
	EXPECT_TRUE(EdgeTurnEvents(frame, -0.2f).Empty());
}

TEST(CameraHelpEvents, DragsCountOnlyWhileMovingWithTheirFeature)
{
	auto frame = Frame();
	frame.landGripped = true;
	EXPECT_TRUE(TiltDragEvents(frame).Empty());
	EXPECT_TRUE(LandDragEvents(frame).Empty());
	frame.mouseDelta = {0, 4};
	EXPECT_EQ(List(TiltDragEvents(frame)), (std::vector<uint32_t> {28}));
	EXPECT_EQ(List(LandDragEvents(frame)), (std::vector<uint32_t> {33}));
	frame.features &= ~(feature::k_Pitch | feature::k_Strafe);
	EXPECT_TRUE(TiltDragEvents(frame).Empty());
	EXPECT_TRUE(LandDragEvents(frame).Empty());
}

TEST(CameraHelpEvents, ADragWhileTurningIsNotADrag)
{
	auto frame = Frame();
	frame.landGripped = true;
	frame.mouseDelta = {0, 4};
	frame.zoom = 1.0f;
	EXPECT_TRUE(TiltDragEvents(frame).Empty());
	EXPECT_TRUE(LandDragEvents(frame).Empty());
	EXPECT_TRUE(EdgeTurnEvents(frame, 0.5f).Empty());
}

TEST(CameraHelpEvents, ADoubleClickCountsTheObjectFirstThenTheLand)
{
	EXPECT_EQ(List(DoubleClickEvents(k_AllFeatures, true, true)), (std::vector<uint32_t> {31}));
	EXPECT_EQ(List(DoubleClickEvents(k_AllFeatures, false, true)), (std::vector<uint32_t> {30}));
	EXPECT_TRUE(DoubleClickEvents(k_AllFeatures, false, false).Empty());
	EXPECT_TRUE(DoubleClickEvents(k_AllFeatures & ~feature::k_WatchFights, true, true).Empty());
}

TEST(CameraHelpEvents, TheTempleKeysSecondTapWithinHalfASecondIsADoubleTap)
{
	using namespace std::chrono_literals;
	openblack::zoom_to::ZoomToPlaces zoomTo;
	EXPECT_FALSE(zoomTo.IsDoubleTap(1000ms));
	const openblack::zoom_to::CameraView view {.origin = {0.0f, 100.0f, -100.0f}, .focus = {0.0f, 0.0f, 0.0f}};
	std::ignore = zoomTo.PressTemple(1000ms, view, {}, std::nullopt, {});
	EXPECT_TRUE(zoomTo.IsDoubleTap(1500ms));
	EXPECT_FALSE(zoomTo.IsDoubleTap(1501ms));
}
