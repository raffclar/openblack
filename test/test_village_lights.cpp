/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <bit>

#include <gtest/gtest.h>

#include "3D/VillageLights.h"

using namespace openblack;

TEST(VillageLights, ComeOnWhenTheLandIsDark)
{
	EXPECT_FALSE(village_lights::IsDark(0xFFFFFF));
	EXPECT_FALSE(village_lights::IsDark(0x787878));
	// The mean is rounded down: (120 + 120 + 119) / 3 is 119
	EXPECT_TRUE(village_lights::IsDark(0x787877));
	EXPECT_TRUE(village_lights::IsDark(0x000000));
}

TEST(VillageLights, FollowTheHour)
{
	EXPECT_FLOAT_EQ(village_lights::Intensity(12.0f), 0.0f);
	EXPECT_FLOAT_EQ(village_lights::Intensity(16.5f), 0.0f);
	EXPECT_FLOAT_EQ(village_lights::Intensity(17.0f), 127.5f);
	EXPECT_FLOAT_EQ(village_lights::Intensity(17.5f), 255.0f);
	EXPECT_FLOAT_EQ(village_lights::Intensity(23.0f), 255.0f);
	EXPECT_FLOAT_EQ(village_lights::Intensity(3.0f), 255.0f);
	EXPECT_FLOAT_EQ(village_lights::Intensity(6.0f), 255.0f);
	EXPECT_FLOAT_EQ(village_lights::Intensity(6.5f), 127.5f);
	EXPECT_FLOAT_EQ(village_lights::Intensity(7.0f), 0.0f);
}

TEST(VillageLights, StrengthIsRoundedDown)
{
	EXPECT_EQ(village_lights::Strength(1.0f), 255);
	EXPECT_EQ(village_lights::Strength(0.5f), 127);
	EXPECT_EQ(village_lights::Strength(0.0f), 0);
	EXPECT_EQ(village_lights::VillageStrength(255.0f), 255);
	EXPECT_EQ(village_lights::VillageStrength(127.5f), 127);
}

TEST(VillageLights, PlacedOnTheCellsWithWeights)
{
	// Halfway into cell 12 along x; along z a little before cell 0, in cell -1 most of the way to 0
	const auto placement = village_lights::Place({125.0f, -3.0f});
	EXPECT_EQ(placement.cell, glm::ivec2(12, -1));
	EXPECT_EQ(placement.weight, glm::ivec2(127, 76));
	// On a cell's corner the next texel takes it all: 100 times the float tenth rounds to 10 exactly
	const auto corner = village_lights::Place({100.0f, 0.0f});
	EXPECT_EQ(corner.cell, glm::ivec2(10, 0));
	EXPECT_EQ(corner.weight, glm::ivec2(255, 255));
}

TEST(VillageLights, FlickerEveryThirtyMilliseconds)
{
	auto step = village_lights::AdvanceFlicker(20.0f, 5.0f);
	EXPECT_FLOAT_EQ(step.timer, 25.0f);
	EXPECT_FALSE(step.flickers);
	step = village_lights::AdvanceFlicker(25.0f, 5.0f);
	EXPECT_FLOAT_EQ(step.timer, 30.0f);
	EXPECT_FALSE(step.flickers);
	step = village_lights::AdvanceFlicker(20.0f, 15.0f);
	EXPECT_FLOAT_EQ(step.timer, 5.0f);
	EXPECT_TRUE(step.flickers);
	// Once however long the frame
	step = village_lights::AdvanceFlicker(0.0f, 70.0f);
	EXPECT_FLOAT_EQ(step.timer, 10.0f);
	EXPECT_TRUE(step.flickers);
}

TEST(VillageLights, FlamesLoopEverySevenHundredMilliseconds)
{
	auto step = village_lights::AdvanceFlames(0, 350);
	EXPECT_EQ(step.clock, 350);
	EXPECT_EQ(step.step, 15);
	// The clock only wraps once past the loop
	step = village_lights::AdvanceFlames(600, 100);
	EXPECT_EQ(step.clock, 700);
	EXPECT_EQ(step.step, 31);
	step = village_lights::AdvanceFlames(600, 150);
	EXPECT_EQ(step.clock, 50);
	EXPECT_EQ(step.step, 2);
}

TEST(VillageLights, FlamesPlayBackwards)
{
	EXPECT_EQ(village_lights::FlameCell(0, 0, 0), 31);
	EXPECT_EQ(village_lights::FlameCell(1, 0, 0), 30);
	// The second flame is ten cells on, from where it starts
	EXPECT_EQ(village_lights::FlameCell(1, 1, 13), (10 + 31 - 14) & 31);
	EXPECT_EQ(village_lights::FlameCell(20, 0, 13), (31 - 1) & 31);
}

TEST(VillageLights, SpriteCellsAreEightAcross)
{
	EXPECT_EQ(village_lights::SpriteCellUv(0), glm::vec2(0.0f));
	EXPECT_EQ(village_lights::SpriteCellUv(village_lights::k_GlowCell), glm::vec2(0.0f, 7.0f / 8.0f));
	EXPECT_EQ(village_lights::SpriteCellUv(11), glm::vec2(3.0f / 8.0f, 1.0f / 8.0f));
}

TEST(VillageLights, ThresholdFollowsTheBrightestLight)
{
	EXPECT_EQ(village_lights::Threshold(255), 47);
	EXPECT_EQ(village_lights::Threshold(128), 24);
	EXPECT_EQ(village_lights::Threshold(0), 0);
}

TEST(VillageLights, ThePlaceAndTheFlickerRoundToFloats)
{
	// The position times 0.1 rounds to a float before the cell and its weight. In double precision the weights are 50
	// and 105
	const auto placement = village_lights::Place({4178.0390625f, 1375.88232421875f});
	EXPECT_EQ(placement.cell.x, 417);
	EXPECT_EQ(placement.cell.y, 137);
	EXPECT_EQ(placement.weight.x, 49);
	EXPECT_EQ(placement.weight.y, 104);
	// The timer times 1/30 rounds up to 94 periods, so the timer goes a little below nought
	const auto step = village_lights::AdvanceFlicker(2800.0f, 19.999755859375f);
	EXPECT_TRUE(step.flickers);
	EXPECT_EQ(std::bit_cast<uint32_t>(step.timer), 0xB9800000u);
}
