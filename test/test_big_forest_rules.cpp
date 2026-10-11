/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <gtest/gtest.h>

#include "Nature/BigForestRules.h"

using namespace openblack;

TEST(BigForestRules, ABigForestStartsWithItsSizeTimesItsKindsWoodUncut)
{
	EXPECT_FLOAT_EQ(big_forest::StartWood(1.5f, 25u), 37.5f);
	EXPECT_FLOAT_EQ(big_forest::WoodValue(0.5f, 37.5f), 18.75f);
}

TEST(BigForestRules, TakingLessThanTheDriftLeavesItsSize)
{
	// A forest of size 1 of a kind worth 1000 gives 100: 900 left, a drift of 100, so it keeps its size
	const auto taken = big_forest::TakeWood(1.0f, 1000.0f, 1.0f, 1000u, 100u);
	EXPECT_FALSE(taken.usedUp);
	EXPECT_FLOAT_EQ(taken.woodLeft, 900.0f);
	EXPECT_FALSE(taken.shrinksTo.has_value());
	EXPECT_EQ(taken.taken, 100u);
}

TEST(BigForestRules, OnceTheDriftPasses250ItShrinksToItsWood)
{
	// After three takes of 100 the wood has drifted 300 from its drawn size: it shrinks to 700 / 1000
	float wood = 1000.0f;
	for (int take = 0; take < 2; ++take)
	{
		const auto taken = big_forest::TakeWood(1.0f, wood, 1.0f, 1000u, 100u);
		EXPECT_FALSE(taken.shrinksTo.has_value());
		wood = taken.woodLeft;
	}
	const auto third = big_forest::TakeWood(1.0f, wood, 1.0f, 1000u, 100u);
	ASSERT_TRUE(third.shrinksTo.has_value());
	EXPECT_FLOAT_EQ(*third.shrinksTo, 0.7f);
	// Exactly 250 is not past it
	EXPECT_FALSE(big_forest::TakeWood(1.0f, 1000.0f, 1.0f, 1000u, 250u).shrinksTo.has_value());
}

TEST(BigForestRules, ItsLifeDividesWhatIsTakenAndScalesWhatItIsWorth)
{
	// At half life, 100 taken comes off as 200
	const auto taken = big_forest::TakeWood(0.5f, 1000.0f, 1.0f, 1000u, 100u);
	EXPECT_FLOAT_EQ(taken.woodLeft, 800.0f);
	// Worth 400 against a drawn 500: a drift of 100, no shrinking
	EXPECT_FALSE(taken.shrinksTo.has_value());
	// A forest of size 2 at half life, worth 1000 as drawn: 300 taken comes off as 600, leaving it worth 700, a drift of
	// 300, and it shrinks to its wood over its kind's at its life
	const auto more = big_forest::TakeWood(0.5f, 2000.0f, 2.0f, 1000u, 300u);
	EXPECT_FLOAT_EQ(more.woodLeft, 1400.0f);
	ASSERT_TRUE(more.shrinksTo.has_value());
	EXPECT_FLOAT_EQ(*more.shrinksTo, 1.4f);
}

TEST(BigForestRules, TakingAllItIsWorthUsesItUp)
{
	const auto exact = big_forest::TakeWood(1.0f, 100.0f, 0.1f, 1000u, 100u);
	EXPECT_TRUE(exact.usedUp);
	EXPECT_FLOAT_EQ(exact.woodLeft, 0.0f);
	EXPECT_FALSE(exact.shrinksTo.has_value());
	EXPECT_EQ(exact.taken, 100u);
	// What it was worth is taken, cut to whole units
	const auto more = big_forest::TakeWood(1.0f, 60.7f, 0.1f, 1000u, 100u);
	EXPECT_TRUE(more.usedUp);
	EXPECT_EQ(more.taken, 60u);
}

TEST(BigForestRules, APineNeedsFourMetresClearOfWhatStandsInItsCell)
{
	EXPECT_TRUE(big_forest::BlocksPine(3.0f, 0.5f));
	EXPECT_FALSE(big_forest::BlocksPine(3.0f, 1.0f));
	EXPECT_FALSE(big_forest::BlocksPine(5.0f, 0.0f));
}
