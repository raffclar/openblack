/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>
#include <cstdint>

#include <bit>
#include <vector>

#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/PlayerRelations.h"
#include "OracleData.h"

using namespace openblack;

namespace
{

map_coords::MapCoords Place(const oracle::Table& table, size_t row, size_t x, size_t z)
{
	return {.x = static_cast<int32_t>(table.Bits(row, x)), .z = static_cast<int32_t>(table.Bits(row, z))};
}

} // namespace

// How much a computer god wants to attack a rival: the nearness of the two sides, the abodes near the god's hand, the
// difference of alignment, the harm, the weakness and the strength of the rival, and the growth of its power
TEST(OracleComputerAttitudes, DesireToAttackAsInTheGame)
{
	const auto table = oracle::Table::Load("computer_desire_to_attack.txt");
	ASSERT_TRUE(table.has_value());
	const auto column = [&table](const char* name) {
		const auto found = table->Column(name);
		EXPECT_TRUE(found.has_value()) << name;
		return found.value_or(0);
	};
	const auto godPower = column("god_power");
	const auto godAverage = column("god_average");
	const auto godX = column("god_x");
	const auto godZ = column("god_z");
	const auto godAlignment = column("god_alignment");
	const auto godHarm = column("god_harm");
	const auto rivalPower = column("rival_power");
	const auto rivalAverage = column("rival_average");
	const auto rivalX = column("rival_x");
	const auto rivalZ = column("rival_z");
	const auto rivalAlignment = column("rival_alignment");
	const auto rivalHarm = column("rival_harm");
	const auto handX = column("hand_x");
	const auto handZ = column("hand_z");
	const auto abodes = column("abodes");
	const auto firstAbode = column("abode1_x");
	const auto desire = column("desire");
	ASSERT_GT(table->Rows(), 0u);
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const player_relations::AttackSide god {
		    .power = table->Float(row, godPower),
		    .averagePower = table->Float(row, godAverage),
		    .place = Place(*table, row, godX, godZ),
		    .alignment = table->Float(row, godAlignment),
		    .harmFromOther = table->Float(row, godHarm),
		};
		const player_relations::AttackSide rival {
		    .power = table->Float(row, rivalPower),
		    .averagePower = table->Float(row, rivalAverage),
		    .place = Place(*table, row, rivalX, rivalZ),
		    .alignment = table->Float(row, rivalAlignment),
		    .harmFromOther = table->Float(row, rivalHarm),
		};
		std::vector<map_coords::MapCoords> places;
		for (size_t abode = 0; abode < table->Bits(row, abodes); ++abode)
		{
			places.push_back(Place(*table, row, firstAbode + 2 * abode, firstAbode + 2 * abode + 1));
		}
		const float nearest = player_relations::NearestAbode(Place(*table, row, handX, handZ), places);
		const float ours = player_relations::DesireToAttack(god, rival, nearest);
		const float game = table->Float(row, desire);
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
