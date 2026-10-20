/******************************************************************************
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
#include <optional>
#include <string>

#include <gtest/gtest.h>

#include "ECS/TownAreaRules.h"
#include "OracleData.h"

using namespace openblack;

// The data comes from tools/oracle/cases/town_area_cases.py: the game's own functions on the real FPU.

namespace
{
ecs::components::TownArea AreaOf(const oracle::Table& table, size_t row, const std::array<size_t, 4>& columns)
{
	return {.min = {static_cast<int32_t>(table.Bits(row, columns[0])), static_cast<int32_t>(table.Bits(row, columns[1]))},
	        .max = {static_cast<int32_t>(table.Bits(row, columns[2])), static_cast<int32_t>(table.Bits(row, columns[3]))}};
}

std::optional<std::array<size_t, 4>> Columns(const oracle::Table& table, const char* prefix)
{
	const std::string start(prefix);
	const auto a = table.Column(start + "min_x");
	const auto b = table.Column(start + "min_z");
	const auto c = table.Column(start + "max_x");
	const auto d = table.Column(start + "max_z");
	if (!a || !b || !c || !d)
	{
		return std::nullopt;
	}
	return std::array<size_t, 4> {*a, *b, *c, *d};
}
} // namespace

TEST(OracleTownArea, ABuildingWidensTheGround)
{
	const auto table = oracle::Table::Load("town_area_widen.txt");
	ASSERT_TRUE(table.has_value());
	const auto before = Columns(*table, "");
	const auto after = Columns(*table, "new_");
	const auto x = table->Column("x");
	const auto z = table->Column("z");
	const auto radius = table->Column("radius");
	ASSERT_TRUE(before && after && x && z && radius);
	ASSERT_GT(table->Rows(), 600u);
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		auto ours = AreaOf(*table, row, *before);
		const map_coords::MapCoords at {.x = static_cast<int32_t>(table->Bits(row, *x)),
		                                .z = static_cast<int32_t>(table->Bits(row, *z))};
		ecs::town_area::Widen(ours, at, table->Float(row, *radius));
		const auto game = AreaOf(*table, row, *after);
		if (ours.min != game.min || ours.max != game.max)
		{
			++different;
			if (different <= 20)
			{
				ADD_FAILURE() << "row " << row << ": the game gives " << game.min.x << " " << game.min.y << " " << game.max.x
				              << " " << game.max.y << ", ours gives " << ours.min.x << " " << ours.min.y << " " << ours.max.x
				              << " " << ours.max.y;
			}
		}
	}
	EXPECT_EQ(different, 0u) << "of " << table->Rows() << " rows";
}

TEST(OracleTownArea, TheMiddleOfTheGround)
{
	const auto table = oracle::Table::Load("town_area_centre.txt");
	ASSERT_TRUE(table.has_value());
	const auto area = Columns(*table, "");
	const auto x = table->Column("x");
	const auto z = table->Column("z");
	ASSERT_TRUE(area && x && z);
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto ours = ecs::town_area::Centre(AreaOf(*table, row, *area));
		if (static_cast<uint32_t>(ours.x) != table->Bits(row, *x) || static_cast<uint32_t>(ours.z) != table->Bits(row, *z))
		{
			++different;
			if (different <= 20)
			{
				ADD_FAILURE() << "row " << row << ": the game gives " << table->Bits(row, *x) << " " << table->Bits(row, *z)
				              << ", ours gives " << ours.x << " " << ours.z;
			}
		}
	}
	EXPECT_EQ(different, 0u) << "of " << table->Rows() << " rows";
}

TEST(OracleTownArea, TheRadiusOfATown)
{
	const auto table = oracle::Table::Load("town_area_radius.txt");
	ASSERT_TRUE(table.has_value());
	const auto area = Columns(*table, "");
	const auto radius = table->Column("radius");
	ASSERT_TRUE(area && radius);
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const float ours = ecs::town_area::Radius(AreaOf(*table, row, *area));
		if (std::bit_cast<uint32_t>(ours) != table->Bits(row, *radius))
		{
			++different;
			if (different <= 20)
			{
				ADD_FAILURE() << "row " << row << ": the game gives " << oracle::Hex(table->Float(row, *radius))
				              << ", ours gives " << oracle::Hex(ours);
			}
		}
	}
	EXPECT_EQ(different, 0u) << "of " << table->Rows() << " rows";
}
