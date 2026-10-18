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
#include <string_view>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureRoute.h"
#include "Creature/CreatureWalkPlanner.h"
#include "OracleData.h"

using namespace openblack;
namespace planner = openblack::creature_walk_planner;
namespace route = openblack::creature_route;

namespace
{
/// The phase numbers of the game's walk
constexpr uint32_t k_TurnPhase = 4;
constexpr uint32_t k_StepPhase = 5;
/// The states of the game's route follower
constexpr uint32_t k_Planning = 2;
constexpr uint32_t k_Walking = 3;
constexpr uint32_t k_Replanning = 4;

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
	[[nodiscard]] glm::vec2 Point(size_t row, std::string_view x, std::string_view z) const
	{
		return {Float(row, x), Float(row, z)};
	}

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

// A creature told to stop on its way: the end of the leg that it is on and the end of its route, from its speed and
// what its turn or step still needs
TEST(OracleCreatureWalkPlanner3, TheStopOnTheWayIsTheGames)
{
	const auto table = oracle::Table::Load("creature_soft_stop.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_GE(table->Rows(), 1500u);
	const Columns c(*table);
	size_t checked = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto state = c.Bits(row, "state");
		route::Leg leg {.from = c.Point(row, "from_x", "from_z"), .to = c.Point(row, "to_x", "to_z")};
		if (c.Bits(row, "arc") == 1)
		{
			leg.arc = true;
			leg.round = route::Circle {.centre = c.Point(row, "centre_x", "centre_z"), .radius = c.Float(row, "radius")};
			leg.roundWay = c.Bits(row, "way") == 2 ? route::Way::Anticlockwise : route::Way::Clockwise;
		}
		if (state != k_Walking && state != k_Replanning)
		{
			// Other states keep the leg as it is
			EXPECT_EQ(c.Bits(row, "end_x"), c.Bits(row, "to_x")) << "row " << row;
			EXPECT_EQ(c.Bits(row, "end_z"), c.Bits(row, "to_z")) << "row " << row;
			EXPECT_EQ(c.Bits(row, "state_after"), state == k_Planning ? 1u : state) << "row " << row;
			continue;
		}
		const auto phase = c.Bits(row, "phase");
		float stillNeeded = 0.0f;
		if (phase == k_TurnPhase)
		{
			stillNeeded = planner::k_TurnJoinAhead;
		}
		else if (phase == k_StepPhase)
		{
			const glm::vec3 move {c.Float(row, "move_x"), c.Float(row, "move_y"), c.Float(row, "move_z")};
			stillNeeded = planner::StepJoinAhead(move, c.Float(row, "scale"), c.Float(row, "travelled"));
		}
		route::Route followed {.legs = {leg, route::Leg {.from = leg.to, .to = leg.to}},
		                       .travelled = c.Float(row, "travelled")};
		const auto end = route::EndAhead(followed, planner::StopAhead(c.Float(row, "speed"), stillNeeded));
		ASSERT_EQ(followed.legs.size(), 1u);
		EXPECT_EQ(Bits(followed.legs[0].to.x), c.Bits(row, "end_x")) << "row " << row;
		EXPECT_EQ(Bits(followed.legs[0].to.y), c.Bits(row, "end_z")) << "row " << row;
		EXPECT_EQ(Bits(end.x), c.Bits(row, "dest_x")) << "row " << row;
		EXPECT_EQ(Bits(end.y), c.Bits(row, "dest_z")) << "row " << row;
		EXPECT_EQ(c.Bits(row, "state_after"), k_Walking) << "row " << row;
		EXPECT_EQ(c.Bits(row, "last"), 1u) << "row " << row;
		++checked;
	}
	EXPECT_GE(checked, 900u);
}
