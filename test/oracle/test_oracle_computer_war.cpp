/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstddef>
#include <cstdint>

#include <bit>
#include <vector>

#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "ECS/ComputerMindWarRules.h"
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

/// A map position from its 16.16 parts, at an altitude
[[nodiscard]] map_coords::MapCoords Coords(uint32_t x, uint32_t z, float altitude)
{
	return {.x = static_cast<int32_t>(x), .z = static_cast<int32_t>(z), .altitude = altitude};
}

} // namespace

// The thing in a town that a computer god most wants to destroy: its building or its field
TEST(OracleComputerWar, TownTargetAsInTheGame)
{
	const auto table = oracle::Table::Load("computer_war_town_target.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_GT(table->Rows(), 0u);
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto at = [&](const char* name) { return table->Bits(row, *table->Column(name)); };
		const auto floatAt = [&](const char* name) { return table->Float(row, *table->Column(name)); };
		const auto hand = Coords(at("hand_x"), at("hand_z"), 0.0f);
		std::vector<float> values;
		std::vector<uint32_t> kinds;
		if (at("has_abode") != 0)
		{
			const float value = BuildingTargetValue(floatAt("built"), floatAt("life"), floatAt("abode_desire"));
			const float distance = gutils::GetDistanceInMetres(hand, Coords(at("abode_x"), at("abode_z"), 0.0f));
			values.push_back(NearTargetValue(value, gutils::GetDistanceModifier(distance, k_TownTargetReach)));
			kinds.push_back(1);
		}
		if (at("has_field") != 0)
		{
			const float value = FieldTargetValue(floatAt("food"), floatAt("total_food"), floatAt("field_desire"));
			const float distance = gutils::GetDistanceInMetres(hand, Coords(at("field_x"), at("field_z"), 0.0f));
			values.push_back(NearTargetValue(value, gutils::GetDistanceModifier(distance, k_TownTargetReach)));
			kinds.push_back(2);
		}
		const auto best = MostWantedTarget(values);
		const uint32_t ours = best.has_value() ? kinds.at(*best) : 0;
		if (ours != at("chosen") && ++different <= 20)
		{
			ADD_FAILURE() << "row " << row << ": the game chose " << at("chosen") << ", ours " << ours;
		}
	}
	EXPECT_EQ(different, 0u);
}

// How much a computer god wants to attack the creature of another player
TEST(OracleComputerWar, AttackCreatureWishAsInTheGame)
{
	const auto table = oracle::Table::Load("computer_war_attack_creature.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_GT(table->Rows(), 0u);
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto at = [&](const char* name) { return table->Bits(row, *table->Column(name)); };
		const auto hand = Coords(at("hand_x"), at("hand_z"), 0.0f);
		const auto creature = Coords(at("creature_x"), at("creature_z"), table->Float(row, *table->Column("altitude")));
		const float distance = gutils::GetDistanceInMetres(hand, creature);
		const float ours = AttackCreatureWish(distance, gutils::GetDistanceModifier(distance, k_AttackCreatureReach));
		const auto game = at("wish");
		if (!Same(ours, game) && ++different <= 20)
		{
			ADD_FAILURE() << "row " << row << ": the game wants " << oracle::Hex(std::bit_cast<float>(game)) << ", ours "
			              << oracle::Hex(ours);
		}
	}
	EXPECT_EQ(different, 0u);
}
