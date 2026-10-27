/******************************************************************************
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
#include <string>

#include <gtest/gtest.h>

#include "ECS/ComputerMindRules.h"
#include "OracleData.h"

using namespace openblack;
using namespace openblack::computer_mind_rules;

// The threat to a town from the attack that it fears most, against the game's own function
TEST(OracleComputerTownThreat, ThreatAsInTheGame)
{
	const auto table = oracle::Table::Load("computer_town_threat.txt");
	ASSERT_TRUE(table.has_value());
	const auto turn = table->Column("turn");
	const auto threat = table->Column("threat");
	const auto fear = table->Column("fear");
	const auto miracle = table->Column("miracle");
	const auto thing = table->Column("thing");
	ASSERT_TRUE(turn && threat && fear && miracle && thing);
	ASSERT_GT(table->Rows(), 0u);
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		std::array<TownAttack, k_TownAttackPlayers> attacks {};
		for (size_t index = 0; index < k_TownAttackPlayers; ++index)
		{
			const auto n = std::to_string(index);
			attacks.at(index) = {.turn = table->Bits(row, *table->Column("last" + n)),
			                     .miracleHarm = table->Float(row, *table->Column("miracle" + n)),
			                     .thingHarm = table->Float(row, *table->Column("thing" + n)),
			                     .otherThingHarm = table->Float(row, *table->Column("other" + n))};
		}
		const auto ours = ThreatOf(attacks, table->Bits(row, *turn));
		bool same = ours.has_value() == (table->Bits(row, *threat) != 0);
		if (same && ours.has_value())
		{
			same = std::bit_cast<uint32_t>(ours->fear) == table->Bits(row, *fear) &&
			       std::bit_cast<uint32_t>(ours->miracleHarm) == table->Bits(row, *miracle) &&
			       std::bit_cast<uint32_t>(ours->thingHarm) == table->Bits(row, *thing);
		}
		if (!same)
		{
			++different;
			if (different <= 20)
			{
				ADD_FAILURE() << "row " << row << ": the game gives " << table->Bits(row, *threat) << " "
				              << oracle::Hex(table->Float(row, *fear)) << ", ours gives " << ours.has_value() << " "
				              << (ours.has_value() ? oracle::Hex(ours->fear) : std::string("-"));
			}
		}
	}
	EXPECT_EQ(different, 0u);
}
