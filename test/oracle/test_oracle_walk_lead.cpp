/*******************************************************************************
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

#include <gtest/gtest.h>

#include "Creature/CreatureLead.h"
#include "Creature/CreatureReach.h"
#include "OracleData.h"

using namespace openblack;
namespace lead = openblack::creature_lead;

namespace
{
lead::Living LivingOfRow(const oracle::Table& table, size_t row)
{
	return {.mapPosition = {static_cast<int32_t>(table.Bits(row, table.Column("map_x").value_or(0))),
	                        static_cast<int32_t>(table.Bits(row, table.Column("map_z").value_or(0)))},
	        .height = table.Float(row, table.Column("offset").value_or(0)),
	        .speed = static_cast<uint16_t>(table.Bits(row, table.Column("speed").value_or(0))),
	        .gameAngle = static_cast<uint16_t>(table.Bits(row, table.Column("angle").value_or(0))),
	        .moved = table.Bits(row, table.Column("moved").value_or(0)) != 0};
}

void ExpectAim(const oracle::Table& table, size_t row, const glm::vec3& aim)
{
	EXPECT_EQ(std::bit_cast<uint32_t>(aim.x), table.Bits(row, table.Column("aim_x").value_or(0))) << "row " << row;
	EXPECT_EQ(std::bit_cast<uint32_t>(aim.y), table.Bits(row, table.Column("aim_y").value_or(0))) << "row " << row;
	EXPECT_EQ(std::bit_cast<uint32_t>(aim.z), table.Bits(row, table.Column("aim_z").value_or(0))) << "row " << row;
}
} // namespace

// A creature out of reach of a villager or an animal walks to where the thing will be when it gets there
TEST(OracleWalkLead, TheWalkGoesToTheGamesPlace)
{
	const auto table = oracle::Table::Load("creature_walk_lead.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_GE(table->Rows(), 1500u);
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto living = LivingOfRow(*table, row);
		const lead::Walker walker {.position = {table->Float(row, table->Column("pos_x").value_or(0)),
		                                        table->Float(row, table->Column("pos_y").value_or(0)),
		                                        table->Float(row, table->Column("pos_z").value_or(0))},
		                           .size = table->Float(row, table->Column("size").value_or(0)),
		                           .runSpeed = table->Float(row, table->Column("run").value_or(0)),
		                           .fraction = table->Float(row, table->Column("fraction").value_or(0))};
		ExpectAim(*table, row, living.moved ? lead::WalkTo(walker, living) : lead::PlaceOf(living));
	}
}

// When a pick-up or a stomp starts, the creature reaches for where the thing will be at the moment of the hold
TEST(OracleWalkLead, TheReachGoesToTheGamesPlace)
{
	const auto table = oracle::Table::Load("creature_reach_lead.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_GE(table->Rows(), 1500u);
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto living = LivingOfRow(*table, row);
		const auto seconds = lead::ReachSeconds(table->Float(row, table->Column("size").value_or(0)),
		                                        static_cast<int32_t>(table->Bits(row, table->Column("ms").value_or(0))));
		ExpectAim(*table, row, living.moved ? lead::WhereAfter(living, seconds) : lead::PlaceOf(living));
	}
}

// A creature on its way has a thing in reach when the thing is nearer than the middle depth of the four corners
TEST(OracleWalkLead, TheMovingReachIsTheGames)
{
	const auto table = oracle::Table::Load("creature_moving_reach.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_GE(table->Rows(), 1500u);
	const auto first = table->Column("c0x").value_or(0);
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		creature_reach::Points points {};
		for (size_t i = 0; i < points.size(); ++i)
		{
			points.at(i) = {table->Float(row, first + (i * 3)), table->Float(row, first + (i * 3) + 1),
			                table->Float(row, first + (i * 3) + 2)};
		}
		const auto reach = creature_reach::MovingReach(points, table->Float(row, table->Column("scale").value_or(0)));
		EXPECT_EQ(std::bit_cast<uint32_t>(reach), table->Bits(row, table->Column("reach").value_or(0))) << "row " << row;
	}
}
