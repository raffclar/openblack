/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <bit>

#include <gtest/gtest.h>

#define LOCATOR_IMPLEMENTATIONS
#include "Common/GameRandomProduction.h"
#include "OracleData.h"

using namespace openblack;

namespace
{
constexpr uint32_t k_GameRand = 0;
constexpr uint32_t k_GameFloatRand = 1;
constexpr uint32_t k_LocalRand = 2;
constexpr uint32_t k_LocalFloatRand = 3;

/// One draw of the row's kind on a fresh random state, and the seed of the draw's stream after it
struct Drawn
{
	uint32_t result;
	uint32_t seedAfter;
	GameRandomSeeds seeds;
};

Drawn Draw(uint32_t draw, uint32_t seed, uint32_t range)
{
	// The other stream starts on a value of its own, to show that a draw does not move it
	constexpr uint32_t k_OtherSynced = 0x13579BDF;
	constexpr uint32_t k_OtherLocal = 0x2468ACE0;
	const bool synced = draw == k_GameRand || draw == k_GameFloatRand;
	GameRandomProduction game;
	game.SetSeeds({.synced = synced ? seed : k_OtherSynced, .local = synced ? k_OtherLocal : seed});
	uint32_t result = 0;
	switch (draw)
	{
	case k_GameRand:
		result = game.GameRand(range);
		break;
	case k_GameFloatRand:
		result = std::bit_cast<uint32_t>(game.GameFloatRand(std::bit_cast<float>(range)));
		break;
	case k_LocalRand:
		result = game.LocalRand(static_cast<int32_t>(range));
		break;
	case k_LocalFloatRand:
	default:
		result = std::bit_cast<uint32_t>(game.LocalFloatRand(std::bit_cast<float>(range)));
		break;
	}
	const auto seeds = game.GetSeeds();
	return {.result = result, .seedAfter = synced ? seeds.synced : seeds.local, .seeds = seeds};
}
} // namespace

// The four draws of the game give the game's numbers, bit for bit, and leave the seed as the game leaves it. A range of
// 0, a float range of 0 or -0 and a NaN range draw nothing. A draw on one stream does not move the other stream.
TEST(OracleGameRandom, TheDrawsAreTheGames)
{
	const auto table = oracle::Table::Load("game_random.txt");
	ASSERT_TRUE(table.has_value());
	const auto draw = table->Column("draw");
	const auto seed = table->Column("seed");
	const auto range = table->Column("range");
	const auto result = table->Column("result");
	const auto seedAfter = table->Column("seed_after");
	ASSERT_TRUE(draw && seed && range && result && seedAfter);
	ASSERT_GT(table->Rows(), 0u);
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto kind = table->Bits(row, *draw);
		const auto drawn = Draw(kind, table->Bits(row, *seed), table->Bits(row, *range));
		EXPECT_EQ(drawn.result, table->Bits(row, *result)) << "row " << row;
		EXPECT_EQ(drawn.seedAfter, table->Bits(row, *seedAfter)) << "row " << row;
		const bool synced = kind == k_GameRand || kind == k_GameFloatRand;
		EXPECT_EQ(synced ? drawn.seeds.local : drawn.seeds.synced, synced ? 0x2468ACE0u : 0x13579BDFu) << "row " << row;
	}
}

// A system that the test makes alone, with no random state in the locator, draws the game's numbers on seeds of its
// own that start where the game's seeds start
TEST(OracleGameRandom, ASourceAloneDrawsAsTheGameFromTheStartingSeeds)
{
	const auto table = oracle::Table::Load("game_random.txt");
	ASSERT_TRUE(table.has_value());
	const auto draw = table->Column("draw");
	const auto seed = table->Column("seed");
	const auto range = table->Column("range");
	const auto result = table->Column("result");
	ASSERT_TRUE(draw && seed && range && result);
	size_t compared = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		if (table->Bits(row, *seed) != game_random::k_InitialSeed)
		{
			continue;
		}
		const auto kind = table->Bits(row, *draw);
		const auto bits = table->Bits(row, *range);
		GameRandomSource source;
		uint32_t drawn = 0;
		switch (kind)
		{
		case k_GameRand:
			drawn = source.GameRand(bits);
			break;
		case k_GameFloatRand:
			drawn = std::bit_cast<uint32_t>(source.GameFloatRand(std::bit_cast<float>(bits)));
			break;
		case k_LocalRand:
			drawn = source.LocalRand(static_cast<int32_t>(bits));
			break;
		default:
			drawn = std::bit_cast<uint32_t>(source.LocalFloatRand(std::bit_cast<float>(bits)));
			break;
		}
		EXPECT_EQ(drawn, table->Bits(row, *result)) << "row " << row;
		++compared;
	}
	EXPECT_GT(compared, 0u);
}
