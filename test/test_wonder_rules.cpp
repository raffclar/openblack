/*******************************************************************************
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

#include <gtest/gtest.h>

#include "ECS/WonderRules.h"

using namespace openblack::ecs::wonder;

TEST(WonderRules, TheReachIsFifteenMetresForEachUnitOfDesireAndAtLeastFifteen)
{
	EXPECT_EQ(Reach(0.0f), 15.0f);
	EXPECT_EQ(Reach(0.5f), 15.0f);
	EXPECT_EQ(Reach(1.0f), 15.0f);
	EXPECT_EQ(Reach(2.0f), 30.0f);
}

TEST(WonderRules, TheStepsAreTheCellsAcrossTheReachSquared)
{
	// 15 m: 3 cells across, 9 steps
	EXPECT_EQ(SearchSteps(15.0f), 9U);
	// The cells across are cut to a whole number
	EXPECT_EQ(SearchSteps(24.9f), 16U);
	EXPECT_EQ(SearchSteps(30.0f), 36U);
	// At least one cell
	EXPECT_EQ(SearchSteps(0.0f), 1U);
	EXPECT_EQ(SearchSteps(4.0f), 1U);
}

TEST(WonderRules, ThePowerCountsTheCellOfThePlaceOnceForEachStep)
{
	const std::array things {
	    Neighbour {.distance = 5.0f, .artefactWorth = 1.5f, .impressiveness = 1.0f},
	    // Out of the reach of 15 m
	    Neighbour {.distance = 15.0f, .artefactWorth = 9.0f, .impressiveness = 9.0f},
	};
	// The desire 0.5 gives a reach of 15 m and 9 steps. Each step adds the worth and a tenth of the impressiveness.
	float expected = 0.5f;
	for (int step = 0; step < 9; ++step)
	{
		expected = 1.5f + expected;
		expected = 1.0f * k_ImpressivenessShare + expected;
	}
	EXPECT_EQ(Power(0.5f, things), expected);
	EXPECT_EQ(std::bit_cast<uint32_t>(k_ImpressivenessShare), 0x3DCCCCCDU);
}

TEST(WonderRules, ThePowerIsAtLeastAQuarter)
{
	EXPECT_EQ(Power(0.0f, {}), 0.25f);
	EXPECT_EQ(Power(0.2f, {}), 0.25f);
	EXPECT_EQ(Power(0.3f, {}), 0.3f);
}

TEST(WonderRules, TheScaleIsThePowerTimesOneLessATenthOfItAndAtMostTwoAndAHalf)
{
	const float power = 2.0f;
	EXPECT_EQ(Scale(power), power * (1.0f - power * k_ScaleLossPerPower));
	EXPECT_EQ(Scale(0.25f), 0.25f * (1.0f - 0.25f * k_ScaleLossPerPower));
	EXPECT_EQ(Scale(5.0f), 2.5f);
	EXPECT_EQ(Scale(12.0f), 2.5f);
}

TEST(WonderRules, AWonderAddsItsPowerToTheMostTribalPowerAndTakesItAwayAgain)
{
	const float most = WithWonder(1.0f, 1.75f);
	EXPECT_EQ(most, 2.75f);
	EXPECT_EQ(WithoutWonder(most, 1.75f), 1.0f);
}

TEST(WonderRules, TheTribalPowerIsTheMostPowerAndAtLeastOne)
{
	EXPECT_EQ(SettledTribalPower(2.5f), 2.5f);
	EXPECT_EQ(SettledTribalPower(1.0f), 1.0f);
	EXPECT_EQ(SettledTribalPower(0.4f), 1.0f);
}
