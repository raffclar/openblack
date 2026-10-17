/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <gtest/gtest.h>

#define LOCATOR_IMPLEMENTATIONS
#include "Common/GameRandomProduction.h"
#include "Creature/CreatureIdleMind.h"
#include "OracleData.h"

using namespace openblack;
using creature_mind::Step;

namespace
{
constexpr uint32_t k_Static = 0;
constexpr uint32_t k_Individual = 1;
constexpr uint32_t k_Faint = 2;
constexpr uint32_t k_NotStarted = 0xFFFFFFFF;

Step StepOf(uint32_t start)
{
	switch (start)
	{
	case k_Static:
		return {.kind = Step::Kind::Static, .seconds = 5.0f, .sequence = {27, 28, 29}};
	case k_Individual:
		return {.kind = Step::Kind::Action, .animation = 28};
	case k_Faint:
	default:
		return {.kind = Step::Kind::Static, .seconds = 5.0f, .sequence = {28, 28, 29}, .holdLoop = true};
	}
}
} // namespace

// The mind starts a step as the game's body does: it picks the side from the game's random numbers with the same draw,
// and it makes the draw also when the busy body refuses the start. A faint starts also on a busy body. On a body that a
// script set to real time, a static action starts on the right side. A step with a thing in the hand is not compared:
// these steps start with empty hands.
TEST(OracleCreatureStartSide, TheMindPicksTheSideAsTheGameDoes)
{
	const auto table = oracle::Table::Load("creature_start_side.txt");
	ASSERT_TRUE(table.has_value());
	const auto start = table->Column("start");
	const auto seed = table->Column("seed");
	const auto busy = table->Column("busy");
	const auto slowed = table->Column("slowed");
	const auto holds = table->Column("holds");
	const auto started = table->Column("started");
	const auto side = table->Column("side");
	const auto seedAfter = table->Column("seed_after");
	ASSERT_TRUE(start && seed && busy && slowed && holds && started && side && seedAfter);
	size_t compared = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto kind = table->Bits(row, *start);
		const bool isBusy = table->Bits(row, *busy) != 0;
		if (table->Bits(row, *holds) != 0)
		{
			continue;
		}
		GameRandomProduction game;
		game.SetSeeds({.synced = table->Bits(row, *seed)});
		const creature_mind::Random gameRandom = [&game](uint32_t range) { return game.GameRand(range); };
		const creature_mind::Random mindRandom = [](uint32_t) { return 0u; };
		creature_mind::IdleMind mind;
		mind.activity = creature_mind::Activity::Told;
		mind.agenda = {StepOf(kind)};
		const creature_mind::Senses senses {.seconds = 0.1f, .bodyBusy = isBusy, .realTime = table->Bits(row, *slowed) != 0};
		const auto commands = creature_mind::Think(mind, senses, mindRandom, gameRandom);
		const bool gameStarted = table->Bits(row, *started) != 0;
		EXPECT_EQ(mind.stepStarted, gameStarted) << "row " << row;
		EXPECT_EQ(commands.playOnce.has_value() || commands.startSequence.has_value(), gameStarted) << "row " << row;
		if (gameStarted)
		{
			ASSERT_NE(table->Bits(row, *side), k_NotStarted);
			EXPECT_EQ(commands.mirrored, table->Bits(row, *side) == 1) << "row " << row;
		}
		EXPECT_EQ(game.GetSeeds().synced, table->Bits(row, *seedAfter)) << "row " << row;
		++compared;
	}
	EXPECT_GE(compared, 70u);
}
