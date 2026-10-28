/******************************************************************************
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

#include "ECS/ComputerMindCreatureRules.h"
#include "OracleData.h"

using namespace openblack;
using namespace openblack::computer_mind_rules;

// How much a computer god wants to teach its creature a kind of miracle, from the times that it sent the creature while
// the creature did not know that kind
TEST(OracleComputerCreatureTeach, LessonWishAsInTheGame)
{
	const auto table = oracle::Table::Load("computer_creature_lesson_wish.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_GT(table->Rows(), 0u);
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto sends = table->Bits(row, *table->Column("sends"));
		const float ours = MiracleLessonWish(sends);
		const auto game = table->Bits(row, *table->Column("wish"));
		if (std::bit_cast<uint32_t>(ours) != game && ++different <= 20)
		{
			ADD_FAILURE() << "row " << row << ": the game gives " << oracle::Hex(std::bit_cast<float>(game)) << ", ours gives "
			              << oracle::Hex(ours);
		}
	}
	EXPECT_EQ(different, 0u);
}
