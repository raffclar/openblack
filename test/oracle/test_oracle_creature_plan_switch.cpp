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

#include "Creature/CreatureDesireSources.h"
#include "Creature/CreaturePlanner.h"
#include "OracleData.h"

using namespace openblack;

// The creature takes up the best plan of the round for the same pairs of priorities as the game
TEST(OracleCreaturePlanSwitch, TheChangeOfPlanHasTheTestOfTheGame)
{
	const auto table = oracle::Table::Load("creature_plan_switch.txt");
	ASSERT_TRUE(table.has_value());
	const auto best = table->Column("best");
	const auto current = table->Column("current");
	const auto switches = table->Column("switches");
	ASSERT_TRUE(best && current && switches);
	ASSERT_GT(table->Rows(), 0u);
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const bool ours = creature_planner::ShouldSwitch(table->Float(row, *best), table->Float(row, *current));
		const bool game = table->Bits(row, *switches) != 0;
		if (ours != game)
		{
			++different;
			if (different <= 20)
			{
				ADD_FAILURE() << "row " << row << ": best " << oracle::Hex(table->Float(row, *best)) << ", current "
				              << oracle::Hex(table->Float(row, *current)) << ": the game gives " << game << ", ours gives "
				              << ours;
			}
		}
	}
	EXPECT_EQ(different, 0u);
}

// The source of the desire to show how the creature feels has the same bits as the game's, from the sum of its desires
TEST(OracleCreaturePlanSwitch, TheWishToShowHowItFeelsHasTheBitsOfTheGame)
{
	const auto table = oracle::Table::Load("creature_manifest_state.txt");
	ASSERT_TRUE(table.has_value());
	const auto sum = table->Column("sum");
	const auto value = table->Column("value");
	ASSERT_TRUE(sum && value);
	ASSERT_GT(table->Rows(), 0u);
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const float ours = creature_desire_sources::ManifestState(table->Float(row, *sum));
		const auto game = table->Bits(row, *value);
		if (std::bit_cast<uint32_t>(ours) != game)
		{
			++different;
			if (different <= 20)
			{
				ADD_FAILURE() << "row " << row << ": sum " << oracle::Hex(table->Float(row, *sum)) << ": the game gives "
				              << oracle::Hex(std::bit_cast<float>(game)) << ", ours gives " << oracle::Hex(ours);
			}
		}
	}
	EXPECT_EQ(different, 0u);
}
