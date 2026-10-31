/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "Commands/TurnCodec.h"
#include "Online/SentBlocks.h"

using namespace openblack;
using namespace openblack::player_commands;
using online::SentBlocks;
using turn_codec::SequenceAfter;

namespace
{

entt::entity Entity(uint32_t value)
{
	return static_cast<entt::entity>(value);
}

/// Sends a block of one tap, and keeps it until a turn holds it
uint16_t SendTap(SentBlocks& sent, uint32_t object, bool keep = true)
{
	const std::vector<Action> actions {HandTap {.object = Entity(object)}};
	auto block = sent.Make(10, actions);
	const auto sequence = block.sequence;
	sent.Sent(std::move(block), keep);
	return sequence;
}

std::vector<uint16_t> KeptNumbers(const SentBlocks& sent)
{
	std::vector<uint16_t> numbers;
	for (const auto& block : sent.Kept())
	{
		numbers.push_back(block.sequence);
	}
	return numbers;
}

} // namespace

TEST(BlockSequences, ANumberIsAfterTheNumbersInTheHalfOfTheCountBeforeIt)
{
	EXPECT_TRUE(SequenceAfter(1, 0));
	EXPECT_FALSE(SequenceAfter(0, 0));
	EXPECT_FALSE(SequenceAfter(0, 1));
	// The count wraps
	EXPECT_TRUE(SequenceAfter(0, 65'535));
	EXPECT_TRUE(SequenceAfter(3, 65'000));
	EXPECT_FALSE(SequenceAfter(65'535, 0));
	EXPECT_TRUE(SequenceAfter(32'767, 0));
	EXPECT_FALSE(SequenceAfter(32'768, 0));
}

TEST(SentBlocks, EachBlockGetsTheNextNumberOnlyWhenItWent)
{
	SentBlocks sent;
	const std::vector<Action> actions {HandTap {.object = Entity(1)}};
	EXPECT_EQ(sent.Make(3, actions).sequence, 0);
	// A block that did not go keeps its number for the next block
	EXPECT_EQ(sent.Make(3, actions).sequence, 0);
	EXPECT_EQ(SendTap(sent, 1), 0);
	EXPECT_EQ(SendTap(sent, 2), 1);
	EXPECT_EQ(sent.NextSequence(), 2);
	const auto block = sent.Make(3, actions);
	EXPECT_EQ(block.turn, 3u);
	ASSERT_EQ(block.actions.size(), 1u);
}

TEST(SentBlocks, AHeldBlockGoesWithTheBlocksBeforeIt)
{
	SentBlocks sent;
	for (uint32_t object = 0; object < 5; ++object)
	{
		SendTap(sent, object);
	}
	EXPECT_EQ(KeptNumbers(sent), (std::vector<uint16_t> {0, 1, 2, 3, 4}));
	sent.Held(2);
	EXPECT_EQ(KeptNumbers(sent), (std::vector<uint16_t> {3, 4}));
	// A turn that the client had tells the same again
	sent.Held(1);
	EXPECT_EQ(KeptNumbers(sent), (std::vector<uint16_t> {3, 4}));
	sent.Held(4);
	EXPECT_TRUE(sent.Kept().empty());
}

TEST(SentBlocks, TheNumbersWrapAndTheKeptBlocksStayInTheirOrder)
{
	SentBlocks sent;
	for (int i = 0; i < 65'534; ++i)
	{
		SendTap(sent, 1, false);
	}
	EXPECT_EQ(SendTap(sent, 1), 65'534);
	EXPECT_EQ(SendTap(sent, 2), 65'535);
	EXPECT_EQ(SendTap(sent, 3), 0);
	EXPECT_EQ(SendTap(sent, 4), 1);
	sent.Held(65'535);
	EXPECT_EQ(KeptNumbers(sent), (std::vector<uint16_t> {0, 1}));
	sent.Held(0);
	EXPECT_EQ(KeptNumbers(sent), (std::vector<uint16_t> {1}));
}

TEST(SentBlocks, OnlyTheBlocksOfTheOwnGodThatThePlayerSentCount)
{
	SentBlocks sent;
	SendTap(sent, 1);
	SendTap(sent, 2);
	turn_codec::TurnBundle turn {.turn = 4, .blocks = {}};
	// The server makes the join of the god with the number 0, and another god sends its block 1
	turn.blocks.push_back({.sender = PlayerNames::PLAYER_ONE, .sequence = 0, .actions = {PlayerJoined {}}});
	turn.blocks.push_back({.sender = PlayerNames::PLAYER_TWO, .sequence = 1, .actions = {HandTap {.object = Entity(5)}}});
	sent.HeldIn(turn, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(KeptNumbers(sent), (std::vector<uint16_t> {0, 1}));

	turn.blocks.push_back({.sender = PlayerNames::PLAYER_ONE, .sequence = 0, .actions = {HandTap {.object = Entity(1)}}});
	sent.HeldIn(turn, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(KeptNumbers(sent), (std::vector<uint16_t> {1}));
}

TEST(SentBlocks, ABlockBeforeTheStartIsNotKeptAndANewSessionCountsFromZero)
{
	SentBlocks sent;
	SendTap(sent, 1, false);
	SendTap(sent, 2);
	EXPECT_EQ(KeptNumbers(sent), (std::vector<uint16_t> {1}));
	sent.Reset();
	EXPECT_TRUE(sent.Kept().empty());
	EXPECT_EQ(sent.NextSequence(), 0);
}
