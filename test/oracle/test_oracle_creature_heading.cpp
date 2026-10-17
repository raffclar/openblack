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

#include "Common/GameHeading.h"
#include "Creature/CreatureCastMoves.h"
#include "OracleData.h"

using namespace openblack;

// The game moves an angle one full turn at most to bring it into the half turns each side of 0
TEST(OracleCreatureHeading, WrapAsInTheGame)
{
	const auto table = oracle::Table::Load("heading_wrap.txt");
	ASSERT_TRUE(table.has_value());
	const auto angle = table->Column("angle");
	const auto wrapped = table->Column("wrapped");
	ASSERT_TRUE(angle && wrapped);
	ASSERT_GT(table->Rows(), 0u);
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const float ours = GameHeading::Wrap(table->Float(row, *angle));
		const float game = table->Float(row, *wrapped);
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

// A creature faces a thing when the heading from its map position to the thing's is near its own heading, or when it
// stands on top of the thing
TEST(OracleCreatureHeading, FacingAsInTheGame)
{
	constexpr uint32_t k_GameFaces = 2;
	const auto table = oracle::Table::Load("creature_facing.txt");
	ASSERT_TRUE(table.has_value());
	const auto cx = table->Column("creature_x");
	const auto cz = table->Column("creature_z");
	const auto px = table->Column("point_x");
	const auto pz = table->Column("point_z");
	const auto heading = table->Column("heading");
	const auto result = table->Column("result");
	ASSERT_TRUE(cx && cz && px && pz && heading && result);
	ASSERT_GT(table->Rows(), 0u);
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const glm::vec2 creature {table->Float(row, *cx), table->Float(row, *cz)};
		const glm::vec2 point {table->Float(row, *px), table->Float(row, *pz)};
		const bool ours = creature_cast_moves::Facing(creature, GameHeading(table->Float(row, *heading)), point);
		const bool game = table->Bits(row, *result) == k_GameFaces;
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
