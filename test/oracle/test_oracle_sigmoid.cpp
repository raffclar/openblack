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
#include <functional>
#include <string_view>

#include <gtest/gtest.h>

#include "Common/GUtilsDistance.h"
#include "Creature/CreatureDesires.h"
#include "OracleData.h"

using namespace openblack;

namespace
{

/// Compares a sigmoid with a column of the game's results, bit for bit, and gives the count of different rows
size_t CountDifferent(std::string_view column, const std::function<float(float, float)>& sigmoid)
{
	const auto table = oracle::Table::Load("sigmoid.txt");
	EXPECT_TRUE(table.has_value());
	if (!table)
	{
		return 1;
	}
	const auto a = table->Column("a");
	const auto b = table->Column("b");
	const auto expected = table->Column(column);
	EXPECT_TRUE(a && b && expected);
	if (!a || !b || !expected)
	{
		return 1;
	}
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const float ours = sigmoid(table->Float(row, *a), table->Float(row, *b));
		if (std::bit_cast<uint32_t>(ours) != table->Bits(row, *expected))
		{
			++different;
			if (different <= 20)
			{
				ADD_FAILURE() << column << " row " << row << ": a " << oracle::Hex(table->Float(row, *a)) << " b "
				              << oracle::Hex(table->Float(row, *b)) << ": the game gives "
				              << oracle::Hex(table->Float(row, *expected)) << ", ours gives " << oracle::Hex(ours);
			}
		}
	}
	return different;
}

} // namespace

TEST(OracleSigmoid, DataIsComplete)
{
	const auto table = oracle::Table::Load("sigmoid.txt");
	ASSERT_TRUE(table.has_value());
	EXPECT_TRUE(table->Column("a") && table->Column("b") && table->Column("creature") && table->Column("shared"));
	EXPECT_GT(table->Rows(), 1000u);
}

TEST(OracleSigmoid, SharedSigmoidAsTheGameDoes)
{
	EXPECT_EQ(CountDifferent("shared", gutils::SigmoidThreshold), 0u);
}

TEST(OracleSigmoid, CreatureSigmoidAsTheGameDoes)
{
	EXPECT_EQ(CountDifferent("creature", gutils::CreatureSigmoidThreshold), 0u);
}

// The desires use the shared sigmoid, so all 41 steps have the game's values
TEST(OracleSigmoid, DesireSigmoidAsTheGameDoes)
{
	EXPECT_EQ(CountDifferent("creature", creature_desires::Sigmoid), 0u);
}

TEST(OracleSigmoid, DesireSigmoidStepAsTheGameDoes)
{
	EXPECT_EQ(CountDifferent("shared", creature_desires::SigmoidStep), 0u);
}
