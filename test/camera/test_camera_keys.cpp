/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Camera/CameraKeys.h"

using namespace openblack::camera_keys;

TEST(CameraKeys, AKeyStepsFourHundredInASecondOfWholeMilliseconds)
{
	EXPECT_EQ(k_StepPerSecond, 400.0f);
	EXPECT_EQ(FrameSeconds(0), 0.0f);
	const float seconds = FrameSeconds(16);
	EXPECT_EQ(seconds, 16.0f * 0.001f);
	EXPECT_EQ(Step({.turnRight = true}, seconds).turn, seconds * 400.0f);
	EXPECT_EQ(Step({.turnLeft = true}, seconds).turn, -(seconds * 400.0f));
	EXPECT_EQ(Step({.tiltUp = true}, seconds).tilt, seconds * 400.0f);
	EXPECT_EQ(Step({.tiltDown = true}, seconds).tilt, -(seconds * 400.0f));
}

TEST(CameraKeys, OppositeKeysCancel)
{
	const float seconds = FrameSeconds(20);
	const auto steps = Step({.turnLeft = true,
	                         .turnRight = true,
	                         .tiltUp = true,
	                         .tiltDown = true,
	                         .forwards = true,
	                         .backwards = true,
	                         .left = true,
	                         .right = true},
	                        seconds);
	EXPECT_EQ(steps.turn, 0.0f);
	EXPECT_EQ(steps.tilt, 0.0f);
	EXPECT_EQ(steps.forwards, 0.0f);
	EXPECT_EQ(steps.strafe, 0.0f);
}

TEST(CameraKeys, TheArrowsMoveZoomOrTiltByTheKeysHeld)
{
	const float seconds = FrameSeconds(10);
	const float step = seconds * 400.0f;
	EXPECT_EQ(Step({.forwards = true}, seconds).forwards, -step);
	EXPECT_EQ(Step({.right = true}, seconds).strafe, step);
	// Ctrl: up and down zoom, left and right turn
	EXPECT_EQ(Step({.forwards = true, .zoom = true}, seconds).zoom, -step);
	EXPECT_EQ(Step({.left = true, .zoom = true}, seconds).turn, step);
	// Shift: up and down tilt, left and right turn
	EXPECT_EQ(Step({.backwards = true, .rotate = true}, seconds).tilt, step);
	EXPECT_EQ(Step({.right = true, .rotate = true}, seconds).turn, -step);
	// Ctrl wins over Shift
	EXPECT_EQ(Step({.forwards = true, .zoom = true, .rotate = true}, seconds).zoom, -step);
	EXPECT_EQ(Step({.forwards = true, .zoom = true, .rotate = true}, seconds).tilt, 0.0f);
}

TEST(CameraKeys, InAFightTheArrowsTiltAndTurnUnlessCtrlIsHeld)
{
	const float seconds = FrameSeconds(10);
	const float step = seconds * 400.0f;
	EXPECT_TRUE(ArrowsRotate({.fight = true}));
	EXPECT_FALSE(ArrowsRotate({.zoom = true, .fight = true}));
	EXPECT_EQ(Step({.forwards = true, .fight = true}, seconds).tilt, -step);
	EXPECT_EQ(Step({.forwards = true, .fight = true}, seconds).forwards, 0.0f);
	EXPECT_EQ(Step({.left = true, .fight = true}, seconds).turn, step);
}

TEST(CameraKeys, TheTurnKeyAndAnArrowAddUp)
{
	const float seconds = FrameSeconds(7);
	const float step = seconds * 400.0f;
	const auto steps = Step({.turnLeft = true, .left = true, .rotate = true}, seconds);
	// The turn key turns left, and Shift with the left arrow turns right
	EXPECT_EQ(steps.turn, 0.0f);
	EXPECT_EQ(Step({.turnRight = true, .right = true, .rotate = true}, seconds).turn, 0.0f);
	EXPECT_EQ(Step({.turnRight = true, .left = true, .rotate = true}, seconds).turn, step + step);
}
