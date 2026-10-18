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

#include "Creature/CreatureWalkPlanner.h"
#include "OracleData.h"

using namespace openblack;
namespace planner = openblack::creature_walk_planner;

// The milliseconds that a creature lives in a turn at its pace, and the milliseconds that its animations play in them
TEST(OracleCreatureWalkPlanner4, TheTurnAndAnimationMillisecondsAreTheGames)
{
	const auto table = oracle::Table::Load("creature_turn_ms.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_GE(table->Rows(), 1500u);
	const auto pace = table->Column("pace").value_or(0);
	const auto gameMs = table->Column("game_ms").value_or(0);
	const auto turnMs = table->Column("turn_ms").value_or(0);
	const auto size = table->Column("size").value_or(0);
	const auto clipMs = table->Column("clip_ms").value_or(0);
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto ms = planner::TurnMs(table->Float(row, pace), static_cast<int32_t>(table->Bits(row, gameMs)));
		EXPECT_EQ(std::bit_cast<uint32_t>(ms), table->Bits(row, turnMs)) << "row " << row;
		const auto step = planner::ClipStepMs(static_cast<int32_t>(table->Bits(row, turnMs)), table->Float(row, size));
		EXPECT_EQ(std::bit_cast<uint32_t>(step), table->Bits(row, clipMs)) << "row " << row;
	}
}
