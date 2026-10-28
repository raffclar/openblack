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

TEST(MouseMoves, AStillMouseSendsNoMessagesWhileTheInputLockKeepsThePlayerOut)
{
	using openblack::help::SendsStillMessages;
	EXPECT_TRUE(SendsStillMessages({.windowHasKeys = true}));
	EXPECT_FALSE(SendsStillMessages({.windowHasKeys = false}));
	// The desktop can give the keyboard to the window of a game that an agent drives. The lock keeps the messages out.
	EXPECT_FALSE(SendsStillMessages({.windowHasKeys = true, .playerInputBlocked = true}));
	EXPECT_FALSE(SendsStillMessages({.windowHasKeys = true, .cursorFrozen = true}));
	EXPECT_FALSE(SendsStillMessages({.windowHasKeys = true, .playerMouseBlocked = true}));
}

TEST(MouseMoves, AWindowThatGetsTheKeysUnderTheLockCountsNoMove)
{
	// The pointer rests in the middle of the window. A still message would send that place, which is not the top left
	// corner, so the help would count one move of the mouse.
	MouseMoves moves;
	const bool sends = openblack::help::SendsStillMessages({.windowHasKeys = true, .playerInputBlocked = true});
	for (uint32_t ms = 0; ms <= 1000; ms += 17)
	{
		moves.Wait(ms, {640, 512}, sends);
	}
	EXPECT_EQ(moves.TakeMoves(), 0u);
}

TEST(MouseMoves, AFixedFrameGivesEachMessageTheClocksTime)
{
	using openblack::help::MessageTime;
	// The window's own times follow the wall clock and are different in each run. The clock's time is the same.
	EXPECT_EQ(MessageTime(51234u, 51240u, 300u, true), 300u);
	EXPECT_EQ(MessageTime(98765u, 98770u, 300u, true), 300u);
}

TEST(MouseMoves, AFrameOnTheWallClockKeepsTheGapsBetweenMessages)
{
	using openblack::help::MessageTime;
	// The clock is 1000 ms behind the window's count. The message keeps its place 6 ms before now.
	EXPECT_EQ(MessageTime(5994u, 6000u, 5000u, false), 4994u);
	// The difference wraps as the counts do
	EXPECT_EQ(MessageTime(10u, 20u, 0xFFFFFFF0u, false), 0xFFFFFFE6u);
}

TEST(MouseMoves, TwoRunsWithFixedFramesCountTheSameMoves)
{
	using openblack::help::MessageTime;
	// The same moves come at other wall clock times in two runs, in frames of 20 ms on the clock
	auto count = [](uint32_t wallStart, uint32_t wallStep) {
		MouseMoves moves;
		uint32_t total = 0;
		for (uint32_t frame = 0; frame < 50; ++frame)
		{
			const uint32_t clock = frame * 20u;
			const uint32_t wall = wallStart + frame * wallStep;
			moves.Message(MessageTime(wall, wall, clock, true), {static_cast<int>(frame), 7});
			moves.Wait(clock, {static_cast<int>(frame), 7}, false);
			total += moves.TakeMoves();
		}
		return total;
	};
	EXPECT_EQ(count(1000u, 3u), count(777777u, 41u));
}
