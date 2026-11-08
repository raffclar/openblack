/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "ECS/Components/HandSendWait.h"
#include "Hand/HandSendWait.h"

using namespace openblack;
using ecs::components::HandSendWait;

TEST(HandSendWait, ASendWaitsUntilTheTurnPassAfterTheTurnThatDidIt)
{
	HandSendWait wait;
	EXPECT_FALSE(hand_send_wait::Waits(wait));
	hand_send_wait::MarkSent(wait, 40);
	EXPECT_TRUE(hand_send_wait::Waits(wait));
	// The pass of a turn with no command of the player keeps the wait
	hand_send_wait::TurnPass(wait);
	EXPECT_TRUE(hand_send_wait::Waits(wait));
	// The turn that does the command ends the wait at its pass, not before
	hand_send_wait::MarkDone(wait, 41);
	EXPECT_TRUE(hand_send_wait::Waits(wait));
	hand_send_wait::TurnPass(wait);
	EXPECT_FALSE(hand_send_wait::Waits(wait));
	EXPECT_EQ(wait.doneTurn, 0u);
}

TEST(HandSendWait, ACommandDoneBeforeTheSendDoesNotEndTheWait)
{
	HandSendWait wait;
	hand_send_wait::MarkSent(wait, 40);
	hand_send_wait::MarkDone(wait, 39);
	hand_send_wait::TurnPass(wait);
	EXPECT_TRUE(hand_send_wait::Waits(wait));
	// A command done in the same turn as the send ends it
	hand_send_wait::MarkDone(wait, 40);
	hand_send_wait::TurnPass(wait);
	EXPECT_FALSE(hand_send_wait::Waits(wait));
}

TEST(HandSendWait, ThePassClearsTheTurnOfACommandThatNoSendWaitedFor)
{
	HandSendWait wait;
	hand_send_wait::MarkDone(wait, 50);
	hand_send_wait::TurnPass(wait);
	EXPECT_EQ(wait.doneTurn, 0u);
	// A later send in the same turn thus waits for the next command
	hand_send_wait::MarkSent(wait, 50);
	hand_send_wait::TurnPass(wait);
	EXPECT_TRUE(hand_send_wait::Waits(wait));
}

TEST(HandSendWait, ASendAtTurnZeroDoesNotWait)
{
	HandSendWait wait;
	hand_send_wait::MarkSent(wait, 0);
	EXPECT_FALSE(hand_send_wait::Waits(wait));
}
