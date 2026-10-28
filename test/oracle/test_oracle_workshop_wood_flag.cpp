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

#include "ECS/NeedFlagRules.h"
#include "ECS/WorkshopRules.h"
#include "OracleData.h"

using namespace openblack;
using namespace openblack::ecs;

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(OracleWorkshopWoodFlag, TheWoodThatTheFlagShowsMatchesTheGame)
{
	const auto table = oracle::Table::Load("workshop_wood_flag.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_GT(table->Rows(), 0u);
	const auto wood = table->Column("wood").value_or(0);
	const auto scaffoldWood = table->Column("scaffold_wood").value_or(0);
	const auto result = table->Column("result").value_or(0);
	size_t mismatches = 0;
	size_t between = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const float ours = workshop::WoodWant(table->Bits(row, wood), table->Bits(row, scaffoldWood));
		if (ours > 0.0f && ours < 1.0f)
		{
			++between;
		}
		if (std::bit_cast<uint32_t>(ours) != table->Bits(row, result) && ++mismatches <= 10)
		{
			ADD_FAILURE() << "row " << row << ": ours " << ours << " game " << table->Float(row, result);
		}
	}
	EXPECT_EQ(mismatches, 0u);
	// The data must hold many cases between none and all of the wood, else it tests little
	EXPECT_GT(between, table->Rows() / 5);
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(OracleWorkshopWoodFlag, TheShareOfTheFullHeightMatchesTheGame)
{
	const auto table = oracle::Table::Load("need_flag_fill.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_GT(table->Rows(), 0u);
	const auto need = table->Column("need").value_or(0);
	const auto maxNeed = table->Column("max_need").value_or(0);
	const auto result = table->Column("result").value_or(0);
	size_t mismatches = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const float ours = need_flag::Fill(table->Float(row, need), {.maxNeed = table->Float(row, maxNeed)});
		if (std::bit_cast<uint32_t>(ours) != table->Bits(row, result) && ++mismatches <= 10)
		{
			ADD_FAILURE() << "row " << row << ": ours " << ours << " game " << table->Float(row, result);
		}
	}
	EXPECT_EQ(mismatches, 0u);
}
