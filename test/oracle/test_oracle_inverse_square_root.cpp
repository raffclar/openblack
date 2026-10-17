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

#include "Common/ModelInverseSquareRoot.h"
#include "OracleData.h"

using namespace openblack;

TEST(OracleInverseSquareRoot, AsTheGameDoes)
{
	const auto table = oracle::Table::Load("inverse_square_root.txt");
	ASSERT_TRUE(table.has_value());
	const auto x = table->Column("x");
	const auto result = table->Column("result");
	const auto wide = table->Column("wide");
	ASSERT_TRUE(x && result && wide);
	ASSERT_GT(table->Rows(), 1000u);
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		// When the game's result is outside the float range, the result depends on how the caller keeps it
		if (table->Bits(row, *wide) != 0)
		{
			continue;
		}
		const float ours = gutils::ModelInverseSquareRoot(table->Float(row, *x));
		if (std::bit_cast<uint32_t>(ours) != table->Bits(row, *result))
		{
			++different;
			if (different <= 20)
			{
				ADD_FAILURE() << "x " << oracle::Hex(table->Float(row, *x)) << ": the game gives "
				              << oracle::Hex(table->Float(row, *result)) << ", ours gives " << oracle::Hex(ours);
			}
		}
	}
	EXPECT_EQ(different, 0u) << "of " << table->Rows() << " rows";
}
