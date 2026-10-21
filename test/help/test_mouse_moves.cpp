/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Help/MouseMoves.h"

using openblack::help::MouseMoves;

TEST(MouseMoves, APlaceGoesToTheInterfaceOnlyAfterMoreThanTheLimitOfMessageTime)
{
	MouseMoves moves;
	// The first message has no time before it, so nothing goes
	moves.Message(1000, {0, 0});
	// 28 milliseconds is not more than the limit
	moves.Message(1014, {1, 0});
	moves.Message(1028, {2, 0});
	EXPECT_EQ(moves.TakeMoves(), 0u);
	// One more millisecond sends the place, which is the first that the interface gets. It is not the top left corner,
	// so it counts.
	moves.Message(1029, {3, 0});
	EXPECT_EQ(moves.TakeMoves(), 1u);
	// The time starts again: the next place goes after more than 28 milliseconds again
	moves.Message(1057, {4, 0});
	EXPECT_EQ(moves.TakeMoves(), 0u);
	moves.Message(1058, {5, 0});
	EXPECT_EQ(moves.TakeMoves(), 1u);
}

TEST(MouseMoves, MessagesAtTheSameTimeAddNothing)
{
	MouseMoves moves;
	moves.Message(0, {0, 0});
	moves.Message(29, {1, 0});
	for (int i = 0; i < 10; ++i)
	{
		moves.Message(29, {2 + i, 0});
	}
	EXPECT_EQ(moves.TakeMoves(), 1u);
	moves.Message(58, {20, 0});
	EXPECT_EQ(moves.TakeMoves(), 1u);
}

TEST(MouseMoves, ThePlaceThatIsTheSameAsTheOneBeforeCountsNothing)
{
	MouseMoves moves;
	moves.Message(0, {5, 5});
	moves.Message(30, {5, 5});
	// The first place counts, as it is not the top left corner
	EXPECT_EQ(moves.TakeMoves(), 1u);
	moves.Message(60, {5, 5});
	moves.Message(90, {6, 5});
	moves.Message(120, {6, 5});
	// Each frame takes what came since the frame before
	EXPECT_EQ(moves.TakeMoves(), 1u);
	EXPECT_EQ(moves.TakeMoves(), 0u);
}

TEST(MouseMoves, AStillMouseSendsAMessageAfterEachPeriodWithoutOne)
{
	MouseMoves moves;
	moves.Message(0, {5, 5});
	moves.Message(30, {5, 5});
	EXPECT_EQ(moves.TakeMoves(), 1u);
	// The still messages come at 70, 110 and 150, each 40 milliseconds after the one before. Each sends the place,
	// which is the same.
	moves.Wait(150, {5, 5}, true);
	EXPECT_EQ(moves.TakeMoves(), 0u);
	// The time starts again after the last still message: the move 10 milliseconds later sends nothing yet
	moves.Message(160, {9, 5});
	EXPECT_EQ(moves.TakeMoves(), 0u);
	moves.Message(180, {9, 5});
	EXPECT_EQ(moves.TakeMoves(), 1u);
}

TEST(MouseMoves, WithoutStillMessagesTheWaitStartsAgain)
{
	MouseMoves moves;
	moves.Message(0, {5, 5});
	moves.Wait(500, {5, 5}, false);
	// The window takes the messages of a still mouse again: the first comes 40 milliseconds later, so none comes yet
	moves.Wait(539, {5, 5}, true);
	// The time since the first message is more than the limit, so the interface gets its first place
	moves.Message(539, {5, 5});
	EXPECT_EQ(moves.TakeMoves(), 1u);
	moves.Message(560, {6, 5});
	EXPECT_EQ(moves.TakeMoves(), 0u);
	moves.Message(590, {7, 5});
	EXPECT_EQ(moves.TakeMoves(), 1u);
}

TEST(MouseMoves, TheFirstPlaceCountsNothingOnlyAtTheTopLeftCorner)
{
	MouseMoves moves;
	moves.Message(0, {0, 0});
	moves.Message(29, {0, 0});
	EXPECT_EQ(moves.TakeMoves(), 0u);
	moves.Message(58, {0, 1});
	EXPECT_EQ(moves.TakeMoves(), 1u);
}
