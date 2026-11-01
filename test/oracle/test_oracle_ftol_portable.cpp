/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The game's float to integer conversion, bit for bit against the game's own code. The data has the two paths of the
// conversion. openblack copies the 64-bit path on every target. The other path is different only for a value out of
// the 32-bit range or NaN (deviations row, FTL-1).

#include <cstdint>

#include <bit>
#include <optional>

#include <gtest/gtest.h>

#include "Common/X87.h"
#include "OracleData.h"

using namespace openblack;

namespace
{
constexpr uint32_t k_FloatKind = 0;
constexpr uint32_t k_Indefinite = 0x80000000u;

struct Columns
{
	size_t kind;
	size_t low;
	size_t high;
	size_t wide;
	size_t sse;
};

std::optional<Columns> Find(const oracle::Table& table)
{
	const auto kind = table.Column("kind");
	const auto low = table.Column("low");
	const auto high = table.Column("high");
	const auto wide = table.Column("wide");
	const auto sse = table.Column("sse");
	if (!kind || !low || !high || !wide || !sse)
	{
		return std::nullopt;
	}
	return Columns {.kind = *kind, .low = *low, .high = *high, .wide = *wide, .sse = *sse};
}

/// The value of a row: a float, or a double from its two words
double Value(const oracle::Table& table, const Columns& columns, size_t row)
{
	if (table.Bits(row, columns.kind) == k_FloatKind)
	{
		return static_cast<double>(table.Float(row, columns.low));
	}
	const auto bits = (static_cast<uint64_t>(table.Bits(row, columns.high)) << 32u) | table.Bits(row, columns.low);
	return std::bit_cast<double>(bits);
}
} // namespace

TEST(OracleFtolPortable, DataIsComplete)
{
	const auto table = oracle::Table::Load("ftol_portable.txt");
	ASSERT_TRUE(table.has_value());
	EXPECT_TRUE(Find(*table).has_value());
	EXPECT_GT(table->Rows(), 3000u);
}

TEST(OracleFtolPortable, SameAsTheGameOnItsSixtyFourBitPath)
{
	const auto table = oracle::Table::Load("ftol_portable.txt");
	ASSERT_TRUE(table.has_value());
	const auto columns = Find(*table);
	ASSERT_TRUE(columns.has_value());
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto ours = std::bit_cast<uint32_t>(x87::ToInteger(Value(*table, *columns, row)));
		const auto game = table->Bits(row, columns->wide);
		if (ours != game)
		{
			++different;
			if (different <= 20)
			{
				ADD_FAILURE() << "row " << row << ": the game gives " << std::hex << game << ", ours gives " << ours;
			}
		}
	}
	EXPECT_EQ(different, 0u);
}

TEST(OracleFtolPortable, TheOtherPathIsDifferentOnlyOutOfTheThirtyTwoBitRange)
{
	const auto table = oracle::Table::Load("ftol_portable.txt");
	ASSERT_TRUE(table.has_value());
	const auto columns = Find(*table);
	ASSERT_TRUE(columns.has_value());
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const double value = Value(*table, *columns, row);
		const bool inRange = value > -2147483649.0 && value < 2147483648.0;
		const auto sse = table->Bits(row, columns->sse);
		if (inRange)
		{
			EXPECT_EQ(sse, table->Bits(row, columns->wide)) << "row " << row;
		}
		else
		{
			EXPECT_EQ(sse, k_Indefinite) << "row " << row;
		}
	}
}
