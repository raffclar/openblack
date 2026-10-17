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

#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "Common/GUtilsDistance.h"
#include "Creature/CreatureCastMoves.h"
#include "OracleData.h"

using namespace openblack;

// A creature that walks to a thing heads for it when the game angle to it is near its heading, without going round,
// or when it stands in the same cell of the land
TEST(OracleCastingDistance, HeadsForAsInTheGame)
{
	const auto table = oracle::Table::Load("creature_heads_for.txt");
	ASSERT_TRUE(table.has_value());
	const auto cx = table->Column("creature_x");
	const auto cz = table->Column("creature_z");
	const auto px = table->Column("point_x");
	const auto pz = table->Column("point_z");
	const auto heading = table->Column("heading");
	const auto close = table->Column("close");
	ASSERT_TRUE(cx && cz && px && pz && heading && close);
	ASSERT_GT(table->Rows(), 0u);
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const glm::vec2 creature {table->Float(row, *cx), table->Float(row, *cz)};
		const glm::vec2 point {table->Float(row, *px), table->Float(row, *pz)};
		const bool ours = creature_cast_moves::HeadsFor(creature, point, table->Float(row, *heading));
		const bool game = table->Bits(row, *close) != 0;
		if (ours != game)
		{
			++different;
			if (different <= 20)
			{
				ADD_FAILURE() << "row " << row << ": the game gives " << game << ", ours gives " << ours;
			}
		}
	}
	EXPECT_EQ(different, 0u);
}

// The distance between two map positions in metres, through the table of inverse square roots
TEST(OracleCastingDistance, MapDistanceAsInTheGame)
{
	const auto table = oracle::Table::Load("map_distance.txt");
	ASSERT_TRUE(table.has_value());
	const auto ax = table->Column("a_x");
	const auto az = table->Column("a_z");
	const auto bx = table->Column("b_x");
	const auto bz = table->Column("b_z");
	const auto distance = table->Column("distance");
	ASSERT_TRUE(ax && az && bx && bz && distance);
	ASSERT_GT(table->Rows(), 0u);
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const glm::vec2 a {table->Float(row, *ax), table->Float(row, *az)};
		const glm::vec2 b {table->Float(row, *bx), table->Float(row, *bz)};
		const float ours = gutils::GetDistanceInMetres(a, b);
		const float game = table->Float(row, *distance);
		if (std::bit_cast<uint32_t>(ours) != std::bit_cast<uint32_t>(game))
		{
			++different;
			if (different <= 20)
			{
				ADD_FAILURE() << "row " << row << ": the game gives " << oracle::Hex(game) << ", ours gives "
				              << oracle::Hex(ours);
			}
		}
	}
	EXPECT_EQ(different, 0u);
}
