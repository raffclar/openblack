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

#include "3D/MapCoords.h"
#include "Creature/CreaturePlanActions.h"
#include "OracleData.h"

using namespace openblack;

// The creature casts the teleport miracle only when the game's distance from it to the middle of its home's cell is
// more than 150 metres
TEST(OracleCreatureTeleportFromAfar, TheCreatureIsFarEnoughWhereTheGameSaysSo)
{
	const auto table = oracle::Table::Load("creature_teleport_from_afar.txt");
	ASSERT_TRUE(table.has_value());
	const auto x = table->Column("x");
	const auto z = table->Column("z");
	const auto homeX = table->Column("home_x");
	const auto homeZ = table->Column("home_z");
	const auto far = table->Column("far");
	ASSERT_TRUE(x && z && homeX && homeZ && far);
	ASSERT_GT(table->Rows(), 0u);
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const map_coords::MapCoords at {.x = std::bit_cast<int32_t>(table->Bits(row, *x)),
		                                .z = std::bit_cast<int32_t>(table->Bits(row, *z))};
		const map_coords::MapCoords home {.x = std::bit_cast<int32_t>(table->Bits(row, *homeX)),
		                                  .z = std::bit_cast<int32_t>(table->Bits(row, *homeZ))};
		const bool ours = creature_plan_actions::FarEnoughToTeleport(creature_plan_actions::HomeCellDistance(at, home));
		const bool game = table->Bits(row, *far) != 0;
		if (ours != game)
		{
			++different;
			if (different <= 20)
			{
				ADD_FAILURE() << "row " << row << ": the game says far " << game << ", ours says " << ours;
			}
		}
	}
	EXPECT_EQ(different, 0u);
}
