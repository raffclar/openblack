/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>
#include <cstdint>

#include <optional>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include "Commands/PlayerCommands.h"

using namespace openblack;
using namespace openblack::player_commands;

namespace
{

Action Feedback(uint32_t id, float amount)
{
	return CreatureFeedback {.creature = static_cast<entt::entity>(id), .amount = amount};
}

uint32_t IdOf(const Command& command)
{
	return static_cast<uint32_t>(std::get<CreatureFeedback>(command.action).creature);
}

/// A model of the frames of the game: the input step, the turns that are due, the send of the block and then the hand's
/// frame. It tells the turn at which a command does its work.
class FrameModel
{
public:
	/// Play one frame. The input and the hand can each send one command.
	void Frame(bool turnDue, std::optional<uint32_t> fromInput, std::optional<uint32_t> fromHand)
	{
		if (fromInput.has_value())
		{
			_queue.Submit(PlayerNames::PLAYER_ONE, Feedback(*fromInput, 0.0f));
		}
		if (turnDue)
		{
			++_turn;
			for (const auto& command : _queue.TakeSent())
			{
				_doneAt.push_back({IdOf(command), _turn});
			}
		}
		_queue.Send();
		if (fromHand.has_value())
		{
			_queue.Submit(PlayerNames::PLAYER_ONE, Feedback(*fromHand, 0.0f));
		}
	}

	/// The turn at which the command did its work, if it did
	[[nodiscard]] std::optional<uint32_t> TurnOf(uint32_t id) const
	{
		for (const auto& [done, turn] : _doneAt)
		{
			if (done == id)
			{
				return turn;
			}
		}
		return std::nullopt;
	}

private:
	CommandQueue _queue;
	uint32_t _turn {0};
	std::vector<std::pair<uint32_t, uint32_t>> _doneAt;
};

} // namespace

TEST(PlayerCommands, ACommandWaitsUntilTheQueueSendsItsBlock)
{
	CommandQueue queue;
	queue.Submit(PlayerNames::PLAYER_ONE, Feedback(1, 0.5f));
	EXPECT_EQ(queue.OpenCount(), 1u);
	EXPECT_TRUE(queue.TakeSent().empty());
	queue.Send();
	EXPECT_EQ(queue.OpenCount(), 0u);
	EXPECT_EQ(queue.SentCount(), 1u);
	const auto sent = queue.TakeSent();
	ASSERT_EQ(sent.size(), 1u);
	EXPECT_EQ(IdOf(sent.front()), 1u);
	EXPECT_FLOAT_EQ(std::get<CreatureFeedback>(sent.front().action).amount, 0.5f);
	EXPECT_EQ(queue.SentCount(), 0u);
}

TEST(PlayerCommands, EachCommandHasThePlayerThatSentIt)
{
	CommandQueue queue;
	queue.Submit(PlayerNames::PLAYER_ONE, Feedback(1, 0.0f));
	queue.Submit(PlayerNames::PLAYER_THREE, Feedback(2, 0.0f));
	queue.Send();
	const auto sent = queue.TakeSent();
	ASSERT_EQ(sent.size(), 2u);
	EXPECT_EQ(sent.at(0).player, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(sent.at(1).player, PlayerNames::PLAYER_THREE);
}

TEST(PlayerCommands, TheBlocksAndTheirCommandsComeInTheOrderOfTheSend)
{
	CommandQueue queue;
	queue.Submit(PlayerNames::PLAYER_ONE, Feedback(1, 0.0f));
	queue.Submit(PlayerNames::PLAYER_ONE, Feedback(2, 0.0f));
	queue.Send();
	queue.Submit(PlayerNames::PLAYER_ONE, Feedback(3, 0.0f));
	queue.Send();
	EXPECT_EQ(queue.SentBlocks(), 2u);
	const auto sent = queue.TakeSent();
	ASSERT_EQ(sent.size(), 3u);
	EXPECT_EQ(IdOf(sent.at(0)), 1u);
	EXPECT_EQ(IdOf(sent.at(1)), 2u);
	EXPECT_EQ(IdOf(sent.at(2)), 3u);
}

TEST(PlayerCommands, AFullBlockGoesAtOnce)
{
	CommandQueue queue;
	for (uint32_t i = 0; i < CommandQueue::k_BlockSize - 1; ++i)
	{
		queue.Submit(PlayerNames::PLAYER_ONE, Feedback(i, 0.0f));
	}
	EXPECT_EQ(queue.SentBlocks(), 0u);
	queue.Submit(PlayerNames::PLAYER_ONE, Feedback(99, 0.0f));
	EXPECT_EQ(queue.SentBlocks(), 1u);
	EXPECT_EQ(queue.OpenCount(), 0u);
	EXPECT_EQ(queue.SentCount(), CommandQueue::k_BlockSize);
}

TEST(PlayerCommands, AnEmptyBlockIsNotSent)
{
	CommandQueue queue;
	queue.Send();
	EXPECT_EQ(queue.SentBlocks(), 0u);
}

TEST(PlayerCommands, ClearRemovesTheOpenAndTheSentCommands)
{
	CommandQueue queue;
	queue.Submit(PlayerNames::PLAYER_ONE, Feedback(1, 0.0f));
	queue.Send();
	queue.Submit(PlayerNames::PLAYER_ONE, Feedback(2, 0.0f));
	queue.Clear();
	EXPECT_EQ(queue.OpenCount(), 0u);
	EXPECT_EQ(queue.SentCount(), 0u);
	EXPECT_TRUE(queue.TakeSent().empty());
}

TEST(PlayerCommands, AnInputCommandDoesNotChangeTheTurnOfItsOwnFrame)
{
	FrameModel frames;
	// The input of a frame with a turn: that turn starts before the block goes, so the next turn does the command
	frames.Frame(true, 1, std::nullopt);
	EXPECT_FALSE(frames.TurnOf(1).has_value());
	frames.Frame(false, std::nullopt, std::nullopt);
	frames.Frame(true, std::nullopt, std::nullopt);
	EXPECT_EQ(frames.TurnOf(1), 2u);
}

TEST(PlayerCommands, AnInputCommandOfAFrameWithNoTurnGoesInTheNextTurn)
{
	FrameModel frames;
	frames.Frame(true, std::nullopt, std::nullopt);
	frames.Frame(false, 1, std::nullopt);
	frames.Frame(true, std::nullopt, std::nullopt);
	EXPECT_EQ(frames.TurnOf(1), 2u);
}

TEST(PlayerCommands, AHandCommandWaitsForTheBlockOfTheNextFrame)
{
	FrameModel frames;
	// The hand sends after the block of its frame went. The next frame's turn starts before that frame sends it.
	frames.Frame(true, std::nullopt, 1);
	frames.Frame(true, std::nullopt, std::nullopt);
	EXPECT_FALSE(frames.TurnOf(1).has_value());
	frames.Frame(true, std::nullopt, std::nullopt);
	EXPECT_EQ(frames.TurnOf(1), 3u);
}
