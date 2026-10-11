/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <Input/MouseMotion.h>
#include <gtest/gtest.h>

using openblack::input::MouseMotion;

TEST(MouseMotion, AddsUpTheMovesOfAFrame)
{
	MouseMotion motion;
	motion.NewFrame();
	motion.Moved({3, -1});
	motion.Moved({2, 4});
	EXPECT_EQ(motion.Delta(), glm::ivec2(5, 3));
	motion.NewFrame();
	EXPECT_EQ(motion.Delta(), glm::ivec2(0, 0));
}

TEST(MouseMotion, TheCursorPutSomewhereIsNotTheMouseMoving)
{
	MouseMotion motion;
	motion.NewFrame();
	motion.Moved({6, 0});
	// The game puts the cursor back by 6 across and 2 up; the window reports the jump with the next frame's events,
	// among the player's own movement of 1 down
	motion.Put({-6, -2});
	motion.NewFrame();
	motion.Moved({-6, -2});
	motion.Moved({0, 1});
	EXPECT_EQ(motion.Delta(), glm::ivec2(0, 1));
	// Taken off once only
	motion.NewFrame();
	motion.Moved({1, 1});
	EXPECT_EQ(motion.Delta(), glm::ivec2(1, 1));
}

TEST(MouseMotion, PutDuringTheFrameBeforeItsEventsCountsInThatFrame)
{
	// Put before the frame starts reading events (as letting go of the camera's turning puts the cursor back)
	MouseMotion motion;
	motion.Put({10, 0});
	motion.NewFrame();
	motion.Moved({10, 0});
	EXPECT_EQ(motion.Delta(), glm::ivec2(0, 0));
}
