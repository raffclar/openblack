/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Input/HandPointer.h"

using openblack::input::HandPointer;

TEST(HandPointer, BeforeTheMouseMovesTheHandIsUnderTheGamesPointerNotTheCorner)
{
	const HandPointer pointer;
	// Nothing reported yet: the window's report is still the top left corner
	EXPECT_EQ(pointer.Frame({0, 0}, {640, 512}), glm::ivec2(640, 512));
	EXPECT_FALSE(pointer.HasMoved());
}

TEST(HandPointer, FollowsTheGamesPointerEveryFrameUntilTheMouseMoves)
{
	const HandPointer pointer;
	// Resting in the middle, then where the cursor is once the window has had it over it
	EXPECT_EQ(pointer.Frame({0, 0}, {640, 512}), glm::ivec2(640, 512));
	EXPECT_EQ(pointer.Frame({0, 0}, {300, 200}), glm::ivec2(300, 200));
}

TEST(HandPointer, OnceTheMouseHasMovedTheHandFollowsWhatTheWindowReports)
{
	HandPointer pointer;
	pointer.Moved();
	EXPECT_TRUE(pointer.HasMoved());
	EXPECT_EQ(pointer.Frame({120, 340}, {640, 512}), glm::ivec2(120, 340));
	// Even when the report is the corner, it is where the mouse went
	EXPECT_EQ(pointer.Frame({0, 0}, {640, 512}), glm::ivec2(0, 0));
}
