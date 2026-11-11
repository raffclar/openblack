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
#include <optional>

#include <gtest/gtest.h>

#include "Creature/CreaturePlanner.h"
#include "OracleData.h"

using namespace openblack;

// The priority of a plan has the same bits as the priority that the game gives
TEST(OracleCreaturePlanPriority, ThePriorityHasTheBitsOfTheGame)
{
	const auto table = oracle::Table::Load("creature_plan_priority.txt");
	ASSERT_TRUE(table.has_value());
	const auto desire = table->Column("desire");
	const auto whole = table->Column("whole");
	const auto hasWhole = table->Column("has_whole");
	const auto action = table->Column("action");
	const auto actedOn = table->Column("acted_on");
	const auto used = table->Column("used");
	const auto hasUsed = table->Column("has_used");
	const auto priority = table->Column("priority");
	ASSERT_TRUE(desire && whole && hasWhole && action && actedOn && used && hasUsed && priority);
	ASSERT_GT(table->Rows(), 0u);
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto wholeUsefulness = table->Bits(row, *hasWhole) != 0 ? std::optional(table->Float(row, *whole)) : std::nullopt;
		const auto usedUsefulness = table->Bits(row, *hasUsed) != 0 ? std::optional(table->Float(row, *used)) : std::nullopt;
		const float ours = creature_planner::Priority(table->Float(row, *desire), wholeUsefulness, table->Float(row, *action),
		                                              table->Float(row, *actedOn), usedUsefulness);
		const auto game = table->Bits(row, *priority);
		if (std::bit_cast<uint32_t>(ours) != game)
		{
			++different;
			if (different <= 20)
			{
				ADD_FAILURE() << "row " << row << ": the game gives " << oracle::Hex(std::bit_cast<float>(game))
				              << ", ours gives " << oracle::Hex(ours);
			}
		}
	}
	EXPECT_EQ(different, 0u);
}
