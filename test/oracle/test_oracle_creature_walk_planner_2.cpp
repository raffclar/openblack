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
#include <string_view>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureLocomotion.h"
#include "Creature/CreatureWalkPlanner.h"
#include "OracleData.h"

using namespace openblack;
namespace planner = openblack::creature_walk_planner;

namespace
{
/// The phase numbers of the game's walk
constexpr uint32_t k_TurnPhase = 4;
constexpr uint32_t k_StepPhase = 5;

uint32_t Bits(float value)
{
	return std::bit_cast<uint32_t>(value);
}

/// The columns of a table, looked up by name once
class Columns
{
public:
	explicit Columns(const oracle::Table& table)
	    : _table(table)
	{
	}
	[[nodiscard]] uint32_t Bits(size_t row, std::string_view name) const { return _table.Bits(row, Index(name)); }
	[[nodiscard]] float Float(size_t row, std::string_view name) const { return _table.Float(row, Index(name)); }

private:
	[[nodiscard]] size_t Index(std::string_view name) const
	{
		const auto column = _table.Column(name);
		EXPECT_TRUE(column.has_value()) << name;
		return column.value_or(0);
	}
	const oracle::Table& _table;
};
} // namespace

// How far ahead a creature joins a route found while it turns on the spot, steps, or does anything else
TEST(OracleCreatureWalkPlanner2, TheJoinAheadIsTheGames)
{
	const auto table = oracle::Table::Load("creature_join_ahead.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_GE(table->Rows(), 1500u);
	const Columns c(*table);
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto phase = c.Bits(row, "phase");
		const glm::vec3 move {c.Float(row, "move_x"), c.Float(row, "move_y"), c.Float(row, "move_z")};
		float ahead = 0.0f;
		if (phase == k_TurnPhase)
		{
			ahead = planner::k_TurnJoinAhead;
		}
		else if (phase == k_StepPhase)
		{
			ahead = planner::StepJoinAhead(move, c.Float(row, "scale"), c.Float(row, "travelled"));
		}
		EXPECT_EQ(Bits(ahead), c.Bits(row, "ahead")) << "row " << row;
	}
}

// The walking and running speeds of a creature of any size, also one outside the sizes that it keeps
TEST(OracleCreatureWalkPlanner2, TheSpeedsOfASizeAreTheGames)
{
	const auto table = oracle::Table::Load("creature_size_speeds.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_GE(table->Rows(), 1500u);
	const Columns c(*table);
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto speeds = creature_locomotion::SpeedsFor(c.Float(row, "size"));
		EXPECT_EQ(Bits(speeds.walk), c.Bits(row, "walk")) << "row " << row;
		EXPECT_EQ(Bits(speeds.run), c.Bits(row, "run")) << "row " << row;
	}
}
