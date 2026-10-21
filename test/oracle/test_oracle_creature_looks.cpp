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

#include "Creature/CreatureMorph.h"
#include "OracleData.h"

using namespace openblack;

// The looks that the body takes from the creature: the shown fatness a step on, and the evil to good, thin to fat and
// weak to strong looks
TEST(OracleCreatureLooks, TheLooksOfTheBodyAreTheGames)
{
	const auto table = oracle::Table::Load("creature_looks.txt");
	ASSERT_TRUE(table.has_value());
	const auto shown = table->Column("shown");
	const auto fatness = table->Column("fatness");
	const auto alignment = table->Column("alignment");
	const auto strength = table->Column("strength");
	const auto species = table->Column("species_strength");
	const auto shownAfter = table->Column("shown_after");
	const auto evilGood = table->Column("evil_good");
	const auto thinFat = table->Column("thin_fat");
	const auto weakStrong = table->Column("weak_strong");
	ASSERT_TRUE(shown && fatness && alignment && strength && species && shownAfter && evilGood && thinFat && weakStrong);
	ASSERT_GE(table->Rows(), 1500u);
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto looks =
		    creature_morph::TakeLooks(table->Float(row, *shown), table->Float(row, *fatness), table->Float(row, *alignment),
		                              table->Float(row, *strength), table->Float(row, *species));
		EXPECT_EQ(std::bit_cast<uint32_t>(looks.shownFatness), table->Bits(row, *shownAfter)) << "row " << row;
		EXPECT_EQ(std::bit_cast<uint32_t>(looks.evilGood), table->Bits(row, *evilGood)) << "row " << row;
		EXPECT_EQ(std::bit_cast<uint32_t>(looks.weakStrong), table->Bits(row, *weakStrong)) << "row " << row;
		// The body draws its thin to fat look from the shown fatness
		const auto drawn = creature_morph::FromAttributes(0.0f, looks.shownFatness, 0.0f, 0.0f);
		EXPECT_EQ(std::bit_cast<uint32_t>(drawn.thinFat), table->Bits(row, *thinFat)) << "row " << row;
	}
}
