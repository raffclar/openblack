/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "ECS/BeingEatenRules.h"

namespace
{
namespace rules = openblack::ecs::being_eaten_rules;
} // namespace

TEST(BeingEatenRules, AVillagerDiesAfterThreeHundredTurnsOfBeingEaten)
{
	uint16_t left = rules::k_BeingEatenTurns;
	for (int turn = 1; turn < 300; ++turn)
	{
		const auto step = rules::BeingEatenTurn(left, false);
		ASSERT_EQ(step.result, rules::EatenTurn::Eaten) << turn;
		left = step.turnsLeft;
	}
	EXPECT_EQ(left, 1);
	const auto last = rules::BeingEatenTurn(left, false);
	EXPECT_EQ(last.result, rules::EatenTurn::Dies);
	EXPECT_EQ(last.turnsLeft, rules::k_EatenAgainTurns);
}

TEST(BeingEatenRules, AVillagerThatNothingCanKillGetsUpAndTheCountStartsAgain)
{
	const auto step = rules::BeingEatenTurn(1, true);
	EXPECT_EQ(step.result, rules::EatenTurn::GetsUp);
	EXPECT_EQ(step.turnsLeft, 50);
}

TEST(BeingEatenRules, ACountThatStartsAtZeroGoesRoundToTheHighestNumber)
{
	const auto step = rules::BeingEatenTurn(0, false);
	EXPECT_EQ(step.result, rules::EatenTurn::Eaten);
	EXPECT_EQ(step.turnsLeft, 0xFFFF);
}

TEST(BeingEatenRules, AHunterLeavesAVillagerWithASmallLife)
{
	EXPECT_FLOAT_EQ(rules::k_BroughtDownLife, 0.05f);
}
