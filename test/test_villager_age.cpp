/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/VillagerAge.h"

using namespace openblack::ecs::villager_age;

namespace
{
constexpr std::array<float, 20> k_AgeToScale = {0.41f, 0.45f, 0.5f,  0.54f, 0.58f, 0.63f, 0.67f, 0.71f, 0.76f, 0.8f,
                                                0.84f, 0.89f, 0.93f, 0.97f, 0.97f, 0.97f, 0.97f, 1.15f, 1.19f, 1.19f};
constexpr uint32_t k_GrownUp = 13;

/// Hands out the given draws in turn, recording the limits asked for
struct Draws
{
	std::vector<float> values;
	std::vector<float> limits;
	size_t next {0};
	FloatRandom Float()
	{
		return [this](float limit) {
			limits.push_back(limit);
			return values.at(next++);
		};
	}
};
} // namespace

TEST(VillagerAge, AgeCountsWholeYearsOfFifteenHundredTurns)
{
	EXPECT_EQ(AgeOf(1499, 0), 0u);
	EXPECT_EQ(AgeOf(1500, 0), 1u);
	EXPECT_EQ(AgeOf(31500, 1500), 20u);
	// Born before the game began: the turns wrap round
	const auto born = BirthTurnFor(100, 30);
	EXPECT_EQ(AgeOf(100, born), 30u);
	EXPECT_EQ(AgeOf(100 + 1500, born), 31u);
}

TEST(VillagerAge, AdultsAreAtLeastEighteen)
{
	EXPECT_EQ(GivenAge(5, k_GrownUp), 5u);
	EXPECT_EQ(GivenAge(13, k_GrownUp), 18u);
	EXPECT_EQ(GivenAge(40, k_GrownUp), 40u);
	EXPECT_TRUE(IsChildAge(12, k_GrownUp));
	EXPECT_FALSE(IsChildAge(13, k_GrownUp));
}

TEST(VillagerAge, ChildrenStartAtTheirAgesSizeAndAdultsANearlyFullOne)
{
	EXPECT_FLOAT_EQ(StartScale(0, k_GrownUp, k_AgeToScale), 0.41f);
	EXPECT_FLOAT_EQ(StartScale(12, k_GrownUp, k_AgeToScale), 0.93f);
	EXPECT_FLOAT_EQ(StartScale(30, k_GrownUp, k_AgeToScale), 0.9f);
}

TEST(VillagerAge, AChildGrowsUpToThreeQuartersOfTheWayToItsAgesSize)
{
	Draws draws {.values = {0.03f}};
	const float scale = GrownScale(5, k_GrownUp, k_AgeToScale, 0.58f, draws.Float());
	ASSERT_EQ(draws.limits.size(), 1u);
	EXPECT_FLOAT_EQ(draws.limits[0], (0.63f - 0.58f) * 0.75f);
	EXPECT_FLOAT_EQ(scale, 0.61f);
}

TEST(VillagerAge, AnAdultKeepsItsSizeUnlessTheDrawIsBiggerThenDrawsAgain)
{
	// 1.05 - 0.08 = 0.97 is no bigger than 0.98: it stays
	Draws keep {.values = {0.08f}};
	EXPECT_FLOAT_EQ(GrownScale(30, k_GrownUp, k_AgeToScale, 0.98f, keep.Float()), 0.98f);
	EXPECT_EQ(keep.limits.size(), 1u);
	// 1.05 - 0.02 is bigger than 0.9: a second draw decides
	Draws redraw {.values = {0.02f, 0.09f}};
	EXPECT_FLOAT_EQ(GrownScale(30, k_GrownUp, k_AgeToScale, 0.9f, redraw.Float()), 1.05f - 0.09f);
	ASSERT_EQ(redraw.limits.size(), 2u);
	EXPECT_FLOAT_EQ(redraw.limits[1], 0.1f);
}

TEST(VillagerAge, ChildrenGrowFourTimesAYear)
{
	EXPECT_TRUE(IsGrowthTurn(0));
	EXPECT_FALSE(IsGrowthTurn(374));
	EXPECT_TRUE(IsGrowthTurn(375));
	EXPECT_TRUE(IsGrowthTurn(1500));
}

TEST(VillagerAge, TheOldAreCheckedOnTheirOwnTurnOfEightHundred)
{
	// Its own turn, 10 + 790 = 800, fell within the last 8 turns
	EXPECT_TRUE(IsOldAgeCheckDue(790, 10, 8));
	EXPECT_TRUE(IsOldAgeCheckDue(797, 10, 8));
	EXPECT_FALSE(IsOldAgeCheckDue(798, 10, 8));
}

TEST(VillagerAge, OnlyThePastOldAgeDieOfItAndTheOlderTheLikelier)
{
	const Ages ages {.grownUp = 13, .old = 60, .oldest = 100};
	std::vector<uint32_t> intLimits;
	const IntRandom extra = [&intLimits](uint32_t limit) {
		intLimits.push_back(limit);
		return limit;
	};
	const FloatRandom full = [](float limit) { return limit; };
	EXPECT_FALSE(DiesOfOldAge(60, ages, full, extra));
	EXPECT_TRUE(intLimits.empty());
	// The draw at its most adds all forty years: 61 + 40 is past 100
	EXPECT_TRUE(DiesOfOldAge(61, ages, full, extra));
	EXPECT_EQ(intLimits.back(), 40u);
	// A half draw cubed is an eighth: five more years, 90 + 5 is not past 100
	const FloatRandom half = [](float limit) { return limit * 0.5f; };
	EXPECT_FALSE(DiesOfOldAge(90, ages, half, extra));
	EXPECT_EQ(intLimits.back(), 5u);
	EXPECT_TRUE(DiesOfOldAge(96, ages, half, extra));
}
