/******************************************************************************
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

#include "ECS/StormFade.h"

using namespace openblack;

TEST(StormFade, AStormFadesInOverItsFadeTime)
{
	const auto faded = storm_fade::Fade(2.5f, 100.0f, 10.0f, 1.0f, 300.0f);
	EXPECT_FLOAT_EQ(faded.strength, 0.25f);
	EXPECT_FLOAT_EQ(faded.innerRadius, 75.0f);
}

TEST(StormFade, AStormIsFullBetweenItsFades)
{
	EXPECT_EQ(storm_fade::Fade(50.0f, 100.0f, 10.0f, 0.7f, 300.0f),
	          (storm_fade::Faded {.strength = 0.7f, .innerRadius = 300.0f}));
	EXPECT_EQ(storm_fade::Fade(90.0f, 100.0f, 10.0f, 0.7f, 300.0f),
	          (storm_fade::Faded {.strength = 0.7f, .innerRadius = 300.0f}));
	EXPECT_EQ(storm_fade::Fade(10.0f, 100.0f, 10.0f, 0.7f, 300.0f),
	          (storm_fade::Faded {.strength = 0.7f, .innerRadius = 300.0f}));
}

TEST(StormFade, AStormFadesOutAtTheEndOfItsLife)
{
	const auto faded = storm_fade::Fade(95.0f, 100.0f, 10.0f, 1.0f, 300.0f);
	EXPECT_FLOAT_EQ(faded.strength, 0.5f);
	EXPECT_FLOAT_EQ(faded.innerRadius, 150.0f);
}

TEST(StormFade, TheFadeMultipliesBeforeItDivides)
{
	// The share of the fade times the strength gives 0x3BE56041 and 0x41460001
	const float age = std::bit_cast<float>(0x3DCCCCCDu);
	EXPECT_EQ(std::bit_cast<uint32_t>(storm_fade::Fade(age, 100.0f, 10.0f, 0.7f, 300.0f).strength), 0x3BE56042u);
	EXPECT_EQ(std::bit_cast<uint32_t>(storm_fade::Fade(0.75f, 100.0f, 10.0f, 1.0f, 165.0f).innerRadius), 0x41460000u);
	EXPECT_EQ(std::bit_cast<uint32_t>(storm_fade::Fade(99.25f, 100.0f, 10.0f, 1.0f, 165.0f).innerRadius), 0x41460000u);
}

TEST(StormFade, ADeadStormGoesAfterItCountsTwoTurns)
{
	// The turn that the storm dies in is the first turn that it counts
	uint8_t deadTurns = 0;
	EXPECT_FALSE(storm_fade::DeadStormGoes(deadTurns));
	EXPECT_EQ(deadTurns, 1);
	EXPECT_TRUE(storm_fade::DeadStormGoes(deadTurns));
	EXPECT_EQ(deadTurns, 2);
}

TEST(StormFade, TheWeatherKeepsHalfOfTheSnowDepth)
{
	EXPECT_EQ(storm_fade::SnowCover(0.0f), 0);
	EXPECT_EQ(storm_fade::SnowCover(53.9f), 26);
	EXPECT_EQ(storm_fade::SnowCover(54.0f), 27);
	EXPECT_EQ(storm_fade::SnowCover(255.0f), 127);
	EXPECT_EQ(storm_fade::SnowCover(-300.0f), -128);
}
