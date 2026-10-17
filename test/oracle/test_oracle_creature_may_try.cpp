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

#include "Creature/CreatureSpellCasting.h"
#include "OracleData.h"

using namespace openblack;

// The creature tries a miracle only at half the sightings it needs or more. A share that is not a number does not pass.
TEST(OracleCreatureMayTry, TheShareOfTheSightingsDecidesAsInTheGame)
{
	const auto table = oracle::Table::Load("creature_may_try.txt");
	ASSERT_TRUE(table.has_value());
	const auto seen = table->Column("seen");
	const auto needed = table->Column("needed");
	const auto mayTry = table->Column("may_try");
	ASSERT_TRUE(seen && needed && mayTry);
	ASSERT_GT(table->Rows(), 0u);
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto count = static_cast<float>(table->Bits(row, *seen));
		const float times = table->Float(row, *needed);
		const bool ours = creature_spell_casting::MayTry(count, times);
		const bool game = table->Bits(row, *mayTry) != 0;
		if (ours != game)
		{
			++different;
			if (different <= 20)
			{
				ADD_FAILURE() << "row " << row << ": seen " << table->Bits(row, *seen) << " needed " << oracle::Hex(times)
				              << ": the game gives " << game << ", ours gives " << ours;
			}
		}
	}
	EXPECT_EQ(different, 0u);
}
