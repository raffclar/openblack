/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <optional>

#include <gtest/gtest.h>

#include "Input/HandPointer.h"

using openblack::input::HandPointer;

TEST(HandPointer, BeforeTheCursorHasBeenOverTheWindowTheHandIsUnderTheTopLeftCorner)
{
	const HandPointer pointer;
	// The game's pointer only rests in the middle; the original's pointer was the corner until the mouse was heard from
	EXPECT_EQ(pointer.Frame({0, 0}, std::nullopt), glm::ivec2(0, 0));
	EXPECT_FALSE(pointer.HasMoved());
}

TEST(HandPointer, FollowsTheGamesPointerEveryFrameOnceKnownUntilTheMouseMoves)
{
	const HandPointer pointer;
	// The corner until the cursor comes over the window, then where the cursor is, though the mouse hasn't moved
	EXPECT_EQ(pointer.Frame({0, 0}, std::nullopt), glm::ivec2(0, 0));
	EXPECT_EQ(pointer.Frame({0, 0}, glm::ivec2 {300, 200}), glm::ivec2(300, 200));
}

TEST(HandPointer, AnAgentsPointerIsFollowedBeforeTheMouseMoves)
{
	const HandPointer pointer;
	// An agent holding the input rests its pointer in the middle of the window, and the hand goes under it
	EXPECT_EQ(pointer.Frame({0, 0}, glm::ivec2 {640, 512}), glm::ivec2(640, 512));
}

TEST(HandPointer, OnceTheMouseHasMovedTheHandFollowsWhatTheWindowReports)
{
	HandPointer pointer;
	pointer.Moved();
	EXPECT_TRUE(pointer.HasMoved());
	EXPECT_EQ(pointer.Frame({120, 340}, glm::ivec2 {640, 512}), glm::ivec2(120, 340));
	// Even when the report is the corner, it is where the mouse went
	EXPECT_EQ(pointer.Frame({0, 0}, glm::ivec2 {640, 512}), glm::ivec2(0, 0));
}
