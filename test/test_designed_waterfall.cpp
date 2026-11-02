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

#include "3D/DesignedWaterfall.h"
#include "3D/WaterRings.h"

using namespace openblack;
namespace waterfall = openblack::designed_waterfall;

TEST(DesignedWaterfall, ItsNumbersAreTheGames)
{
	EXPECT_EQ(std::bit_cast<uint32_t>(waterfall::k_Position.x), 0x453F33AEu);
	EXPECT_EQ(std::bit_cast<uint32_t>(waterfall::k_Position.y), 0x00000000u);
	EXPECT_EQ(std::bit_cast<uint32_t>(waterfall::k_Position.z), 0x45449548u);
	EXPECT_EQ(std::bit_cast<uint32_t>(waterfall::k_Turn), 0x40966666u);
	EXPECT_EQ(std::bit_cast<uint32_t>(waterfall::k_SprayInterval), 0x3F333333u);
	EXPECT_EQ(waterfall::k_Land, 3);
}

TEST(DesignedWaterfall, TheWaterSlidesHalfItsHeightEachSecond)
{
	EXPECT_EQ(waterfall::Slide(0.0f, 1000), -0.5f);
	// At -1 the whole part goes, and the slide starts again from 0
	EXPECT_EQ(waterfall::Slide(-0.5f, 1000), 0.0f);
	EXPECT_FLOAT_EQ(waterfall::Slide(0.0f, 16), -0.008f);
	EXPECT_NEAR(waterfall::Slide(-0.99f, 100), -0.04f, 1e-6f);
	// A paused game does not move the water
	EXPECT_EQ(waterfall::Slide(-0.25f, 0), -0.25f);
}

TEST(DesignedWaterfall, TheSlideStaysBetweenMinusOneAndZero)
{
	float slide = 0.0f;
	for (int step = 0; step < 1000; ++step)
	{
		slide = waterfall::Slide(slide, 17);
		ASSERT_GT(slide, -1.0f);
		ASSERT_LE(slide, 0.0f);
	}
}

TEST(DesignedWaterfall, SprayComesUpOnTheFirstStepPastTheInterval)
{
	// At 60 steps a second of 16 milliseconds, the clock passes 0.7 seconds on the 44th step
	float clock = 0.0f;
	int steps = 0;
	do
	{
		++steps;
	} while (!waterfall::StepSpray(clock, 16));
	EXPECT_EQ(steps, 44);
	EXPECT_EQ(clock, 0.0f);

	// The clock must go past the interval. The factor of one millisecond is a little more than 0.001, so 700
	// milliseconds are a little more than the interval.
	clock = 0.0f;
	EXPECT_FALSE(waterfall::StepSpray(clock, 699));
	EXPECT_TRUE(waterfall::StepSpray(clock, 1));
	clock = 0.0f;
	EXPECT_TRUE(waterfall::StepSpray(clock, 700));
}

TEST(DesignedWaterfall, TheSprayKeepsNoTimeOverTheInterval)
{
	float clock = 0.0f;
	EXPECT_TRUE(waterfall::StepSpray(clock, 2000));
	EXPECT_EQ(clock, 0.0f);
	EXPECT_FALSE(waterfall::StepSpray(clock, 600));
}

TEST(DesignedWaterfall, TheSprayIsAWideSlowRingAtTheFoot)
{
	const auto ring = waterfall::Spray(0xAB123456u);
	EXPECT_EQ(std::bit_cast<uint32_t>(ring.position.x), 0x453CACCDu);
	EXPECT_EQ(std::bit_cast<uint32_t>(ring.position.y), 0x3E4CCCCDu);
	EXPECT_EQ(std::bit_cast<uint32_t>(ring.position.z), 0x4543A266u);
	EXPECT_EQ(ring.age, 0u);
	EXPECT_EQ(ring.growth, 30.0f);
	EXPECT_EQ(ring.angle, 0.0f);
	EXPECT_EQ(ring.aspect, 1.0f);
	EXPECT_EQ(std::bit_cast<uint32_t>(ring.rate), 0x3E99999Au);
	EXPECT_EQ(ring.cell, 0x30);
	// Half opacity, in the land's light
	EXPECT_EQ(ring.argb, 0x80123456u);

	// It lives about 2.33 seconds of game time
	auto aged = ring;
	int milliseconds = 0;
	while (water_rings::Advance(aged, 10.0f))
	{
		milliseconds += 10;
	}
	EXPECT_EQ(milliseconds, 2330);
}

TEST(DesignedWaterfall, TheThirdLandGetsItsModelAndSoundOnItsFirstFrame)
{
	waterfall::Presence presence;
	auto changes = waterfall::Arrive(presence, 3, false);
	EXPECT_TRUE(changes.makeModel);
	EXPECT_TRUE(changes.makeSound);
	EXPECT_FALSE(changes.removeModel);
	EXPECT_FALSE(changes.removeSound);

	changes = waterfall::Arrive(presence, 3, true);
	EXPECT_FALSE(changes.makeModel);
	EXPECT_FALSE(changes.makeSound);
}

TEST(DesignedWaterfall, OtherLandsHaveNone)
{
	waterfall::Presence presence;
	auto changes = waterfall::Arrive(presence, 1, false);
	EXPECT_FALSE(changes.makeModel);
	EXPECT_FALSE(changes.makeSound);
	EXPECT_FALSE(changes.removeModel);

	static_cast<void>(waterfall::Arrive(presence, 3, false));
	changes = waterfall::Arrive(presence, 4, true);
	EXPECT_TRUE(changes.removeModel);
	EXPECT_TRUE(changes.removeSound);
	EXPECT_FALSE(changes.makeModel);
}

TEST(DesignedWaterfall, TheSameLandAfterAClearKeepsTheModelWithNoSound)
{
	waterfall::Presence presence;
	static_cast<void>(waterfall::Arrive(presence, 3, false));
	waterfall::LandCleared(presence);
	// The frames lost the model with the land: it comes back, but the sound does not
	auto changes = waterfall::Arrive(presence, 3, false);
	EXPECT_TRUE(changes.makeModel);
	EXPECT_FALSE(changes.makeSound);
	EXPECT_FALSE(changes.removeSound);
	EXPECT_FALSE(presence.sound);

	// A change of land and back gives the sound again
	waterfall::LandCleared(presence);
	changes = waterfall::Arrive(presence, 2, true);
	EXPECT_TRUE(changes.removeModel);
	EXPECT_FALSE(changes.removeSound);
	waterfall::LandCleared(presence);
	changes = waterfall::Arrive(presence, 3, false);
	EXPECT_TRUE(changes.makeModel);
	EXPECT_TRUE(changes.makeSound);
}
