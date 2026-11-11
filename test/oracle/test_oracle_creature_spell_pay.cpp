/*******************************************************************************
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
#include <string_view>

#include <gtest/gtest.h>

#include "Creature/CreatureSpellCasting.h"
#include "OracleData.h"

using namespace openblack;

// A creature pays for its miracles with its body. The chants it has, the energy that chants cost it, the test of
// whether it may cast, and what paying gives and does to its energy and exhaustion are the same bits as in the game.
TEST(OracleCreatureSpellPay, PayingForAMiracleGivesTheBitsOfTheGame)
{
	const auto table = oracle::Table::Load("creature_spell_pay.txt");
	ASSERT_TRUE(table.has_value());
	constexpr std::array<std::string_view, 16> k_Names {
	    "size", "strength", "energy", "exhaustion", "chants_per_energy", "energy_floor", "size_factor",  "amount",
	    "cost", "share",    "most",   "to_energy",  "can_pay",           "paid",         "energy_after", "exhaustion_after"};
	std::array<size_t, k_Names.size()> column {};
	for (size_t i = 0; i < k_Names.size(); ++i)
	{
		const auto found = table->Column(k_Names.at(i));
		ASSERT_TRUE(found.has_value()) << k_Names.at(i);
		column.at(i) = *found;
	}
	ASSERT_GT(table->Rows(), 0u);
	const auto bits = [](float value) { return std::bit_cast<uint32_t>(value); };
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto in = [&](size_t i) { return table->Float(row, column.at(i)); };
		creature_spell_casting::Body body {.size = in(0), .strength = in(1), .energy = in(2), .exhaustion = in(3)};
		const creature_spell_casting::Rates rates {.chantsPerEnergy = in(4), .energyFloor = in(5), .sizeFactor = in(6)};
		const float amount = in(7);
		const float cost = in(8);
		const float share = in(9);

		const std::array<uint32_t, 6> ours {
		    bits(creature_spell_casting::MostChants(body, rates)),
		    bits(creature_spell_casting::ChantsToEnergy(amount, body, rates)),
		    creature_spell_casting::CanCast(body, rates, cost) ? 1u : 0u,
		    bits(creature_spell_casting::MaintainSpell(body, rates, amount, share)),
		    bits(body.energy),
		    bits(body.exhaustion),
		};
		for (size_t i = 0; i < ours.size(); ++i)
		{
			const uint32_t game = table->Bits(row, column.at(10 + i));
			if (ours.at(i) != game)
			{
				++different;
				if (different <= 20)
				{
					ADD_FAILURE() << "row " << row << " " << k_Names.at(10 + i) << ": the game gives " << std::hex << game
					              << ", ours gives " << ours.at(i);
				}
			}
		}
	}
	EXPECT_EQ(different, 0u);
}
