/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <bit>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/ComputerMindDefenceRules.h"
#include "OracleData.h"

using namespace openblack;
using namespace openblack::computer_mind_rules;

namespace
{
constexpr size_t k_Players = 8;
constexpr size_t k_Towns = 2;
} // namespace

// The attack on a town that the god wants most to defend against, against the game's own function
TEST(OracleComputerDefence, DefendTownAsInTheGame)
{
	const auto table = oracle::Table::Load("computer_defend_town.txt");
	ASSERT_TRUE(table.has_value());
	const auto turn = table->Column("turn");
	const auto god = table->Column("god");
	const auto wish = table->Column("wish");
	const auto town = table->Column("town");
	const auto attacker = table->Column("attacker");
	ASSERT_TRUE(turn && god && wish && town && attacker);
	ASSERT_GT(table->Rows(), 0u);
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		// The game's list of players gives a human player for every number, so all players but the god attack
		std::vector<TownAttackFacts> attacks;
		for (size_t index = 0; index < k_Towns; ++index)
		{
			for (size_t slot = 0; slot < k_Players; ++slot)
			{
				if (slot == table->Bits(row, *god))
				{
					continue;
				}
				const auto name = std::to_string(index) + "_" + std::to_string(slot);
				attacks.push_back({.town = index,
				                   .attacker = static_cast<PlayerNames>(slot),
				                   .lastTurn = table->Bits(row, *table->Column("last" + name)),
				                   .aggression = table->Float(row, *table->Column("aggression" + name))});
			}
		}
		const auto ours = ChooseDefence(attacks, table->Bits(row, *turn));
		const auto oursWish = ours.has_value() ? std::bit_cast<uint32_t>(ours->value) : 0u;
		const auto oursTown = ours.has_value() ? attacks[ours->index].town : k_Towns;
		const bool same = oursWish == table->Bits(row, *wish) && oursTown == table->Bits(row, *town) &&
		                  ours.has_value() == (table->Bits(row, *attacker) != 0);
		if (!same)
		{
			++different;
			if (different <= 20)
			{
				ADD_FAILURE() << "row " << row << ": the game gives town " << table->Bits(row, *town) << " "
				              << oracle::Hex(table->Float(row, *wish)) << ", ours gives town " << oursTown << " "
				              << oracle::Hex(std::bit_cast<float>(oursWish));
			}
		}
	}
	EXPECT_EQ(different, 0u);
}

// How much a town is under an attack on its belief, against the game's own function
TEST(OracleComputerDefence, BeliefAttackAsInTheGame)
{
	const auto table = oracle::Table::Load("computer_belief_attack.txt");
	ASSERT_TRUE(table.has_value());
	const auto god = table->Column("god");
	const auto value = table->Column("value");
	ASSERT_TRUE(god && value);
	ASSERT_GT(table->Rows(), 0u);
	const std::array<bool, static_cast<size_t>(PlayerNames::_COUNT)> allies {};
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		TownBeliefFacts town;
		for (size_t slot = 0; slot < k_Players; ++slot)
		{
			town.belief.at(slot) = table->Float(row, *table->Column("belief" + std::to_string(slot)));
			town.recent.at(slot) = table->Float(row, *table->Column("recent" + std::to_string(slot)));
		}
		const float ours = BeliefAttack(town, static_cast<PlayerNames>(table->Bits(row, *god)), allies);
		if (std::bit_cast<uint32_t>(ours) != table->Bits(row, *value))
		{
			++different;
			if (different <= 20)
			{
				ADD_FAILURE() << "row " << row << ": the game gives " << oracle::Hex(table->Float(row, *value))
				              << ", ours gives " << oracle::Hex(ours);
			}
		}
	}
	EXPECT_EQ(different, 0u);
}

// How much the god wants to react to a thing that hit its temple, against the game's own function
TEST(OracleComputerDefence, TempleHitAsInTheGame)
{
	const auto table = oracle::Table::Load("computer_temple_hit.txt");
	ASSERT_TRUE(table.has_value());
	const auto turn = table->Column("turn");
	const auto last = table->Column("last");
	const auto value = table->Column("value");
	const auto taken = table->Column("taken");
	ASSERT_TRUE(turn && last && value && taken);
	ASSERT_GT(table->Rows(), 0u);
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto ours = HitReaction(table->Bits(row, *turn), table->Bits(row, *last));
		const auto oursValue = ours.has_value() ? std::bit_cast<uint32_t>(*ours) : 0u;
		const bool same = oursValue == table->Bits(row, *value) && ours.has_value() == (table->Bits(row, *taken) != 0);
		if (!same)
		{
			++different;
			if (different <= 20)
			{
				ADD_FAILURE() << "row " << row << ": the game gives " << oracle::Hex(table->Float(row, *value)) << " "
				              << table->Bits(row, *taken) << ", ours gives " << oracle::Hex(std::bit_cast<float>(oursValue))
				              << " " << ours.has_value();
			}
		}
	}
	EXPECT_EQ(different, 0u);
}
