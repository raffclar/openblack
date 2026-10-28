/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <bit>

#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "ECS/ComputerMindCreatureRules.h"
#include "OracleData.h"

using namespace openblack;
using namespace openblack::computer_mind_rules;

namespace
{

/// The floats are the same bits, or both are not a number
[[nodiscard]] bool Same(float ours, uint32_t game)
{
	return std::bit_cast<uint32_t>(ours) == game || (std::isnan(ours) && std::isnan(std::bit_cast<float>(game)));
}

} // namespace

// Where the hand of a computer god goes before it throws food at its creature, and where it aims the food
TEST(OracleComputerCreaturePlay, ThrowFoodAsInTheGame)
{
	const auto table = oracle::Table::Load("computer_creature_throw_food.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_GT(table->Rows(), 0u);
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto at = [&](const char* name) { return table->Bits(row, *table->Column(name)); };
		const auto floatAt = [&](const char* name) { return table->Float(row, *table->Column(name)); };
		const glm::vec3 hand {floatAt("hand_x"), floatAt("hand_y"), floatAt("hand_z")};
		// The place of the creature: its map position in metres, at the land height plus its altitude
		const glm::vec3 creature {map_coords::ToMetres(static_cast<int32_t>(at("coords_x"))),
		                          floatAt("land") + floatAt("altitude"),
		                          map_coords::ToMetres(static_cast<int32_t>(at("coords_z")))};
		const auto go = ThrowFoodHandPlace(hand, creature);
		const float draw = floatAt("draw");
		const auto aim = ThrowFoodAim(creature, floatAt("size"), draw, draw);
		const bool same = Same(go.x, at("go_x")) && Same(go.y, at("go_y")) && Same(go.z, at("go_z")) &&
		                  Same(aim.x, at("aim_x")) && Same(aim.y, at("aim_y")) && Same(aim.z, at("aim_z"));
		if (!same && ++different <= 20)
		{
			ADD_FAILURE() << "row " << row << ": the game goes to " << oracle::Hex(floatAt("go_x")) << " "
			              << oracle::Hex(floatAt("go_y")) << " " << oracle::Hex(floatAt("go_z")) << ", ours to "
			              << oracle::Hex(go.x) << " " << oracle::Hex(go.y) << " " << oracle::Hex(go.z);
		}
	}
	EXPECT_EQ(different, 0u);
}

// The wish of a computer god to keep the leash of its creature tied when it starts to cast a miracle
TEST(OracleComputerCreaturePlay, CastWishAsInTheGame)
{
	const auto table = oracle::Table::Load("computer_creature_cast_wish.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_GT(table->Rows(), 0u);
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto seen = static_cast<float>(table->Bits(row, *table->Column("seen")));
		const float ours =
		    CastLetGoWish(table->Float(row, *table->Column("freedom")), seen, table->Float(row, *table->Column("needed")));
		const auto game = table->Bits(row, *table->Column("wish"));
		if (!Same(ours, game) && ++different <= 20)
		{
			ADD_FAILURE() << "row " << row << ": the game gives " << oracle::Hex(std::bit_cast<float>(game)) << ", ours gives "
			              << oracle::Hex(ours);
		}
	}
	EXPECT_EQ(different, 0u);
}

// The let-go of the tied leash of the creature of a computer god when it starts to cast a miracle
TEST(OracleComputerCreaturePlay, CastLetGoAsInTheGame)
{
	const auto table = oracle::Table::Load("computer_creature_cast_let_go.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_GT(table->Rows(), 0u);
	size_t different = 0;
	size_t letGo = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto at = [&](const char* name) { return static_cast<int32_t>(table->Bits(row, *table->Column(name))); };
		const map_coords::MapCoords hand {.x = at("hand_x"), .z = at("hand_z"), .altitude = 0.0f};
		const map_coords::MapCoords creature {.x = at("coords_x"), .z = at("coords_z"), .altitude = 0.0f};
		const bool ours = LetsGoAtCast(table->Float(row, *table->Column("freedom")), table->Float(row, *table->Column("wish")),
		                               gutils::GetDistanceInMetres(hand, creature), table->Float(row, *table->Column("size")));
		const bool game = at("let_go") != 0;
		letGo += game ? 1 : 0;
		if (ours != game && ++different <= 20)
		{
			ADD_FAILURE() << "row " << row << ": the game lets go " << game << ", ours " << ours;
		}
	}
	EXPECT_EQ(different, 0u);
	EXPECT_GT(letGo, 0u);
}
