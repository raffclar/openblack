/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <algorithm>
#include <array>
#include <optional>
#include <string_view>

#include <gtest/gtest.h>

#include "Creature/CreatureCatch.h"
#include "OracleData.h"

using namespace openblack;

namespace
{

struct Columns
{
	size_t thingX, thingZ, velocityX, velocityZ, creatureX, creatureZ, size, modelScale, catchMs, stepMs, stepTravel, reaches;
};

std::optional<Columns> Find(const oracle::Table& table)
{
	const auto c = [&table](std::string_view name) { return table.Column(name); };
	const std::array names {c("thing_x"), c("thing_z"),     c("velocity_x"), c("velocity_z"), c("creature_x"),  c("creature_z"),
	                        c("size"),    c("model_scale"), c("catch_ms"),   c("step_ms"),    c("step_travel"), c("reaches")};
	if (std::ranges::any_of(names, [](const auto& name) { return !name.has_value(); }))
	{
		return std::nullopt;
	}
	return Columns {*names[0], *names[1], *names[2], *names[3], *names[4],  *names[5],
	                *names[6], *names[7], *names[8], *names[9], *names[10], *names[11]};
}

creature_catch::Approach ApproachOf(const oracle::Table& table, const Columns& columns, size_t row)
{
	return {
	    .thing = {table.Float(row, columns.thingX), 0.0f, table.Float(row, columns.thingZ)},
	    .velocity = {table.Float(row, columns.velocityX), 0.0f, table.Float(row, columns.velocityZ)},
	    .creature = {table.Float(row, columns.creatureX), 0.0f, table.Float(row, columns.creatureZ)},
	    .size = table.Float(row, columns.size),
	    .modelScale = table.Float(row, columns.modelScale),
	    .catchMs = static_cast<float>(static_cast<int32_t>(table.Bits(row, columns.catchMs))),
	    .stepMs = static_cast<float>(static_cast<int32_t>(table.Bits(row, columns.stepMs))),
	    .stepTravel = table.Float(row, columns.stepTravel),
	};
}

} // namespace

TEST(OracleCatch, DataIsComplete)
{
	const auto table = oracle::Table::Load("catch_reaches.txt");
	ASSERT_TRUE(table.has_value());
	EXPECT_TRUE(Find(*table).has_value());
	EXPECT_GT(table->Rows(), 400u);
}

// The game does the catch maths in a different order (the way as a unit vector over the speed, the sums z first, and
// other points where values are rounded), so near an edge our answer can be different. Turn this test on when
// Reaches does the maths in the game's order.
TEST(OracleCatch, DISABLED_ReachesAsTheGameDoes)
{
	const auto table = oracle::Table::Load("catch_reaches.txt");
	ASSERT_TRUE(table.has_value());
	const auto columns = Find(*table);
	ASSERT_TRUE(columns.has_value());
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const bool expected = table->Bits(row, columns->reaches) != 0;
		const bool ours = creature_catch::Reaches(ApproachOf(*table, *columns, row));
		if (ours != expected)
		{
			++different;
			ADD_FAILURE() << "row " << row << ": the game gives " << expected << ", ours gives " << ours;
		}
	}
	EXPECT_EQ(different, 0u) << "of " << table->Rows() << " rows";
}
