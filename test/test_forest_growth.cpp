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

#include "Nature/ForestGrowth.h"

using namespace openblack;

namespace
{
constexpr forest_growth::Kind k_Kind {.turnsBetween = 100, .amount = 0.01f, .rainAccelerator = 2.0f};
} // namespace

// How much a tree grows

TEST(ForestGrowth, RainAndGoodLandMakeATreeGrowFaster)
{
	EXPECT_FLOAT_EQ(forest_growth::Growth(k_Kind, 0, 0.0f), 0.01f);
	// A hundred points of wet at an accelerator of 2 trebles it
	EXPECT_FLOAT_EQ(forest_growth::Growth(k_Kind, 100, 0.0f), 0.03f);
	EXPECT_FLOAT_EQ(forest_growth::Growth(k_Kind, 0, 1.0f), 0.015f);
	EXPECT_FLOAT_EQ(forest_growth::Growth(k_Kind, 0, -1.0f), 0.005f);
	EXPECT_FLOAT_EQ(forest_growth::Grown(0.5f, 0.1f, 1.0f), 0.6f);
	EXPECT_FLOAT_EQ(forest_growth::Grown(0.95f, 0.1f, 1.0f), 1.0f);
}

TEST(ForestGrowth, WetBelowNothingSlowsGrowth)
{
	// Neither the rain nor the lying snow is held at nothing
	EXPECT_NEAR(forest_growth::Growth(k_Kind, -50, 0.0f), 0.0f, 1e-9f);
	EXPECT_FLOAT_EQ(forest_growth::Growth(k_Kind, -25, 0.0f), 0.005f);
}

TEST(ForestGrowth, WetnessIsTheMoreOfRainAndLyingSnow)
{
	EXPECT_EQ(forest_growth::Wetness(-10, 20), 20);
	EXPECT_EQ(forest_growth::Wetness(30, -5), 30);
	EXPECT_EQ(forest_growth::Wetness(-10, -20), -10);
}

// A growing tree's turn

TEST(ForestGrowth, ATreeGrowsWhenItsClockRunsOut)
{
	uint16_t countdown = 2;
	int asked = 0;
	const auto amount = [&asked] {
		++asked;
		return 0.1f;
	};
	auto turn = forest_growth::TreeStep(countdown, true, 0.5f, 1.0f, 100, amount);
	EXPECT_FALSE(turn.size.has_value());
	EXPECT_TRUE(turn.staysGrowing);
	EXPECT_EQ(countdown, 1);
	// The weather and the land are only looked at when it grows
	EXPECT_EQ(asked, 0);

	turn = forest_growth::TreeStep(countdown, true, 0.5f, 1.0f, 100, amount);
	ASSERT_TRUE(turn.size.has_value());
	EXPECT_FLOAT_EQ(*turn.size, 0.6f);
	EXPECT_TRUE(turn.staysGrowing);
	EXPECT_EQ(countdown, 100);
	EXPECT_EQ(asked, 1);
}

TEST(ForestGrowth, AFirstWaitOfNoneGoesTheWholeClockRound)
{
	uint16_t countdown = forest_growth::FirstWait(0);
	const auto amount = [] { return 0.1f; };
	for (int i = 0; i < 0xFFFF; ++i)
	{
		ASSERT_FALSE(forest_growth::TreeStep(countdown, true, 0.5f, 1.0f, 100, amount).size.has_value());
	}
	EXPECT_TRUE(forest_growth::TreeStep(countdown, true, 0.5f, 1.0f, 100, amount).size.has_value());
}

TEST(ForestGrowth, TheClockIsWoundBackToTheLowSixteenBitsOfTheWait)
{
	uint16_t countdown = 1;
	(void)forest_growth::TreeStep(countdown, true, 0.5f, 1.0f, 0x10005, [] { return 0.1f; });
	EXPECT_EQ(countdown, 5);
}

TEST(ForestGrowth, ATreeReachingItsLargestSizeLeavesTheGrowingTrees)
{
	uint16_t countdown = 1;
	const auto turn = forest_growth::TreeStep(countdown, true, 0.95f, 1.0f, 100, [] { return 0.1f; });
	ASSERT_TRUE(turn.size.has_value());
	EXPECT_FLOAT_EQ(*turn.size, 1.0f);
	EXPECT_FALSE(turn.staysGrowing);
}

TEST(ForestGrowth, AFullGrownTreeLeavesTheGrowingTreesOnlyWhenItsClockRunsOut)
{
	int asked = 0;
	const auto amount = [&asked] {
		++asked;
		return 0.1f;
	};
	uint16_t countdown = 2;
	EXPECT_TRUE(forest_growth::TreeStep(countdown, true, 1.0f, 1.0f, 100, amount).staysGrowing);
	const auto turn = forest_growth::TreeStep(countdown, true, 1.0f, 1.0f, 100, amount);
	EXPECT_FALSE(turn.size.has_value());
	EXPECT_FALSE(turn.staysGrowing);
	EXPECT_EQ(countdown, 100);
	EXPECT_EQ(asked, 0);
}

TEST(ForestGrowth, ATreeNotMadeToGrowNeverGrows)
{
	uint16_t countdown = 1;
	const auto turn = forest_growth::TreeStep(countdown, false, 0.5f, 1.0f, 100, [] { return 0.1f; });
	EXPECT_FALSE(turn.size.has_value());
	EXPECT_FALSE(turn.staysGrowing);
}

// A forest spreading

TEST(ForestGrowth, AForestWithNoGrownTreesNeverSpreads)
{
	uint16_t counter = 60000;
	EXPECT_FALSE(forest_growth::Spreads(0.0f, counter, 100000, 0));
	EXPECT_EQ(counter, 60001);
}

TEST(ForestGrowth, AForestSpreadsWhenTheRollFallsBelowItsChance)
{
	// Twenty grown trees count in full, and three hundred turns since the last gain count a little over once, so the
	// chance is a little over the counter
	uint16_t counter = 1998;
	EXPECT_FALSE(forest_growth::Spreads(0.0f, counter, 300, 20));
	EXPECT_EQ(counter, 1999);
	EXPECT_TRUE(forest_growth::Spreads(0.0f, counter, 300, 20));
	EXPECT_EQ(counter, 0);

	// More than twenty grown trees count no more
	counter = 1998;
	EXPECT_FALSE(forest_growth::Spreads(0.0f, counter, 300, 400));
	// Ten count half
	counter = 3998;
	EXPECT_FALSE(forest_growth::Spreads(0.0f, counter, 300, 10));
	EXPECT_TRUE(forest_growth::Spreads(0.0f, counter, 300, 10));
	// A higher roll needs more
	counter = 2500;
	EXPECT_FALSE(forest_growth::Spreads(999.0f, counter, 300, 20));
}

TEST(ForestGrowth, TheSpreadingCounterWrapsRound)
{
	uint16_t counter = 0xFFFF;
	EXPECT_FALSE(forest_growth::Spreads(0.0f, counter, 300, 20));
	EXPECT_EQ(counter, 0);
}

TEST(ForestGrowth, TheParentIsDrawnFromTheNearerHalfOfTheGrownTrees)
{
	EXPECT_EQ(forest_growth::ParentDraws(1), 1u);
	EXPECT_EQ(forest_growth::ParentDraws(2), 2u);
	EXPECT_EQ(forest_growth::ParentDraws(5), 3u);
	EXPECT_EQ(forest_growth::ParentDraws(20), 11u);
}

// An empty forest

TEST(ForestGrowth, AnEmptyForestGoes1999TurnsAfterItIsFirstSeenEmpty)
{
	uint16_t countdown = 0;
	EXPECT_FALSE(forest_growth::EmptyForestGoes(countdown));
	EXPECT_EQ(countdown, 2000);
	for (int turn = 1; turn < 1999; ++turn)
	{
		ASSERT_FALSE(forest_growth::EmptyForestGoes(countdown)) << turn;
	}
	EXPECT_TRUE(forest_growth::EmptyForestGoes(countdown));
}

TEST(ForestGrowth, AForestEmptiedAgainCarriesOnItsCount)
{
	uint16_t countdown = 0;
	(void)forest_growth::EmptyForestGoes(countdown);
	for (int turn = 0; turn < 1000; ++turn)
	{
		(void)forest_growth::EmptyForestGoes(countdown);
	}
	// It gained trees, then lost them again: the count goes on from where it was
	EXPECT_EQ(countdown, 1000);
	for (int turn = 1; turn < 999; ++turn)
	{
		ASSERT_FALSE(forest_growth::EmptyForestGoes(countdown));
	}
	EXPECT_TRUE(forest_growth::EmptyForestGoes(countdown));
}
