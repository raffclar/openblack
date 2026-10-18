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
#include <utility>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Creature/CreatureLocomotion.h"
#include "Creature/CreatureObstacles.h"
#include "Creature/CreatureRoute.h"
#include "Creature/CreatureStomp.h"
#include "OracleData.h"

using namespace openblack;

namespace
{
using creature_route::Ground;
constexpr int32_t k_Cells = creature_route::k_CellsPerSide;

/// The land of the case script's LandSort
Ground LandSort(int32_t x, int32_t z)
{
	if (x >= 100 && x < 120 && z >= 100 && z < 130)
	{
		return Ground::Blocked;
	}
	if (x >= 300 && x < 340 && z >= 300 && z < 305)
	{
		return Ground::Blocked;
	}
	if (x >= 400 && x < 410 && z >= 400 && z < 420)
	{
		return Ground::CutOff;
	}
	if ((x * 31 + z * 17) % 97 == 0)
	{
		return Ground::Blocked;
	}
	if ((x + z) % 5 == 0)
	{
		return Ground::Water;
	}
	return Ground::Open;
}

/// The clear cells of the case script's ClearCell
bool ClearCell(int32_t x, int32_t z)
{
	if (x >= 150 && x < 170 && z >= 150 && z < 170)
	{
		return false;
	}
	return (x * 7 + z * 3) % 4 != 0;
}

creature_route::WalkableLand TestLand()
{
	std::vector<Ground> cells;
	cells.reserve(static_cast<size_t>(k_Cells * k_Cells));
	for (int32_t z = 0; z < k_Cells; ++z)
	{
		for (int32_t x = 0; x < k_Cells; ++x)
		{
			cells.push_back(LandSort(x, z));
		}
	}
	return creature_route::WalkableLand::OfCells(std::move(cells));
}

uint32_t Bits(float value)
{
	return std::bit_cast<uint32_t>(value);
}
} // namespace

// Whether a creature may stand at a point: the cell, the cells next to it and the squared distances as the game has them
TEST(OracleCreatureFilesOrder3, TheStandTestIsTheGames)
{
	const auto table = oracle::Table::Load("creature_stand_valid.txt");
	ASSERT_TRUE(table.has_value());
	const auto x = table->Column("x");
	const auto radius = table->Column("radius");
	const auto valid = table->Column("valid");
	ASSERT_TRUE(x && radius && valid);
	ASSERT_GE(table->Rows(), 1600u);
	const auto land = TestLand();
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const glm::vec2 point {table->Float(row, *x), table->Float(row, *x + 1)};
		EXPECT_EQ(land.IsValid(point, table->Float(row, *radius)), table->Bits(row, *valid) != 0) << "row " << row;
	}
}

// The search for the nearest place to stand: the directions, the distances and the first place that passes
TEST(OracleCreatureFilesOrder3, TheNearestStandSearchIsTheGames)
{
	const auto table = oracle::Table::Load("creature_nearest_stand.txt");
	ASSERT_TRUE(table.has_value());
	const auto x = table->Column("x");
	const auto found = table->Column("found");
	const auto outX = table->Column("out_x");
	ASSERT_TRUE(x && found && outX);
	ASSERT_GE(table->Rows(), 360u);
	const auto land = TestLand();
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto nearest = land.NearestStandable({table->Float(row, *x), table->Float(row, *x + 1)});
		ASSERT_EQ(nearest.has_value(), table->Bits(row, *found) != 0) << "row " << row;
		if (nearest.has_value())
		{
			EXPECT_EQ(Bits(nearest->x), table->Bits(row, *outX)) << "row " << row;
			EXPECT_EQ(Bits(nearest->y), table->Bits(row, *outX + 2)) << "row " << row;
		}
	}
}

// The point that a creature runs away to, from its map position, the threat, the draw and the land
TEST(OracleCreatureFilesOrder3, TheRunAwayPointIsTheGames)
{
	const auto table = oracle::Table::Load("creature_run_away.txt");
	ASSERT_TRUE(table.has_value());
	const auto layout = table->Column("layout");
	const auto mapX = table->Column("map_x");
	const auto altitude = table->Column("altitude");
	const auto threatX = table->Column("threat_x");
	const auto draw = table->Column("draw");
	const auto runAway = table->Column("run_away");
	const auto ground = table->Column("ground");
	const auto pointX = table->Column("point_x");
	ASSERT_TRUE(layout && mapX && altitude && threatX && draw && runAway && ground && pointX);
	ASSERT_GE(table->Rows(), 800u);
	const auto land = TestLand();
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const bool noneClear = table->Bits(row, *layout) != 0;
		const float height = table->Float(row, *ground);
		const creature_locomotion::RunAwayLand runAwayLand {
		    .cellClear = [noneClear](int32_t cx, int32_t cz) { return !noneClear && ClearCell(cx, cz); },
		    .ground = [height](const map_coords::MapCoords&) { return height; },
		    .canStand = [&land](glm::vec2 at) { return land.IsValid(at, creature_route::k_DestinationClearance); },
		    .nearestStand = [&land](glm::vec2 at) { return land.NearestStandable(at); },
		};
		const map_coords::MapCoords creature {.x = static_cast<int32_t>(table->Bits(row, *mapX)),
		                                      .z = static_cast<int32_t>(table->Bits(row, *mapX + 1)),
		                                      .altitude = table->Float(row, *altitude)};
		const auto point =
		    creature_locomotion::RunAwayPoint(creature, {table->Float(row, *threatX), table->Float(row, *threatX + 1)},
		                                      table->Bits(row, *draw), table->Float(row, *runAway), runAwayLand);
		EXPECT_EQ(Bits(point.x), table->Bits(row, *pointX)) << "row " << row;
		EXPECT_EQ(Bits(point.y), table->Bits(row, *pointX + 1)) << "row " << row;
		EXPECT_EQ(Bits(point.z), table->Bits(row, *pointX + 2)) << "row " << row;
	}
}

// Whether a corner of a model, through the thing's world rows, is in a sphere of the body
TEST(OracleCreatureFilesOrder3, TheModelCornerTestIsTheGames)
{
	const auto table = oracle::Table::Load("creature_model_touches.txt");
	ASSERT_TRUE(table.has_value());
	const auto rowX = table->Column("row0_x");
	const auto posX = table->Column("pos_x");
	const auto cornerX = table->Column("corner_x");
	const auto centreX = table->Column("centre_x");
	const auto radius = table->Column("radius");
	const auto touches = table->Column("touches");
	ASSERT_TRUE(rowX && posX && cornerX && centreX && radius && touches);
	ASSERT_GE(table->Rows(), 1500u);
	const auto vec = [&](size_t row, size_t column) {
		return glm::vec3(table->Float(row, column), table->Float(row, column + 1), table->Float(row, column + 2));
	};
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const creature_obstacles::PlacementRows rows {vec(row, *rowX), vec(row, *rowX + 3), vec(row, *rowX + 6)};
		EXPECT_EQ(creature_stomp::CornerInSphere(rows, vec(row, *posX), vec(row, *cornerX), vec(row, *centreX),
		                                         table->Float(row, *radius)),
		          table->Bits(row, *touches) != 0)
		    << "row " << row;
	}
}
