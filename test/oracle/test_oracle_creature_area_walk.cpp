/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <gtest/gtest.h>

#include "Creature/LeashRules.h"
#include "OracleData.h"

using namespace openblack;

// The area that holds a creature refuses a walk of a new agenda to a point further than its radius from its centre,
// with the steps of the distance rounded as the game rounds them
TEST(OracleCreatureAreaWalk, TheAreaRefusesTheWalksThatTheGameRefuses)
{
	const auto table = oracle::Table::Load("creature_area_walk.txt");
	ASSERT_TRUE(table.has_value());
	const auto centreX = table->Column("centre_x");
	const auto centreZ = table->Column("centre_z");
	const auto radius = table->Column("radius");
	const auto pointX = table->Column("point_x");
	const auto pointZ = table->Column("point_z");
	const auto refused = table->Column("refused");
	ASSERT_TRUE(centreX && centreZ && radius && pointX && pointZ && refused);
	ASSERT_GT(table->Rows(), 0u);
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const creature_leash::AgendaArea area {
		    .radius = table->Float(row, *radius),
		    .centre = {table->Float(row, *centreX), table->Float(row, *centreZ)},
		};
		const glm::vec2 point {table->Float(row, *pointX), table->Float(row, *pointZ)};
		const bool ours = creature_leash::RefusesWalk(area, point);
		const bool game = table->Bits(row, *refused) != 0;
		if (ours != game)
		{
			++different;
			if (different <= 20)
			{
				ADD_FAILURE() << "row " << row << ": the game refuses " << game << ", ours refuses " << ours;
			}
		}
	}
	EXPECT_EQ(different, 0u);
}
