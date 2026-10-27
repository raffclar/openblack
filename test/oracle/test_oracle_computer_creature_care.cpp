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

#include "ECS/ComputerMindCreatureRules.h"
#include "OracleData.h"

using namespace openblack;
using namespace openblack::computer_mind_rules;

namespace
{

/// The desire of a plan that restores the health of the creature
constexpr uint32_t k_RestoreHealthDesire = 15;

} // namespace

// How free the creature of a computer god is, from its leash, its fight, its life, its rest, its energy and its plan
TEST(OracleComputerCreatureCare, FreedomAsInTheGame)
{
	const auto table = oracle::Table::Load("computer_creature_freedom.txt");
	ASSERT_TRUE(table.has_value());
	const auto leashed = table->Column("leashed");
	const auto turns = table->Column("turns_since_tied");
	const auto fighting = table->Column("fighting");
	const auto life = table->Column("life");
	const auto exhaustion = table->Column("exhaustion");
	const auto energy = table->Column("energy");
	const auto desire = table->Column("plan_desire");
	const auto freedom = table->Column("freedom");
	ASSERT_TRUE(leashed && turns && fighting && life && exhaustion && energy && desire && freedom);
	ASSERT_GT(table->Rows(), 0u);
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto ours = CreatureFreedom({
		    .leashed = table->Bits(row, *leashed) != 0,
		    .turnsSinceTied = table->Bits(row, *turns),
		    .fighting = table->Bits(row, *fighting) != 0,
		    .life = table->Float(row, *life),
		    .exhaustion = table->Float(row, *exhaustion),
		    .energy = table->Float(row, *energy),
		    .restoringHealth = table->Bits(row, *desire) == k_RestoreHealthDesire,
		});
		if (std::bit_cast<uint32_t>(ours) != table->Bits(row, *freedom))
		{
			++different;
			if (different <= 20)
			{
				ADD_FAILURE() << "row " << row << ": the game gives " << oracle::Hex(table->Float(row, *freedom))
				              << ", ours gives " << oracle::Hex(ours);
			}
		}
	}
	EXPECT_EQ(different, 0u);
}
