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
	auto changes = waterfall::Arrive(presence, 3, {});
	EXPECT_TRUE(changes.makeModel);
	EXPECT_TRUE(changes.makeSound);
	EXPECT_FALSE(changes.removeModel);
	EXPECT_FALSE(changes.removeSound);

	changes = waterfall::Arrive(presence, 3, {.model = true});
	EXPECT_FALSE(changes.makeModel);
	EXPECT_FALSE(changes.makeSound);
}

TEST(DesignedWaterfall, OtherLandsHaveNone)
{
	waterfall::Presence presence;
	auto changes = waterfall::Arrive(presence, 1, {});
	EXPECT_FALSE(changes.makeModel);
	EXPECT_FALSE(changes.makeSound);
	EXPECT_FALSE(changes.removeModel);

	static_cast<void>(waterfall::Arrive(presence, 3, {}));
	changes = waterfall::Arrive(presence, 5, {.model = true});
	EXPECT_TRUE(changes.removeModel);
	EXPECT_TRUE(changes.removeSound);
	EXPECT_FALSE(changes.removeDinosaur);
	EXPECT_FALSE(changes.makeModel);
	EXPECT_FALSE(changes.makeDinosaur);
}

TEST(DesignedWaterfall, TheSameLandAfterAClearKeepsTheModelWithNoSound)
{
	waterfall::Presence presence;
	static_cast<void>(waterfall::Arrive(presence, 3, {}));
	waterfall::LandCleared(presence);
	// The frames lost the model with the land: it comes back, but the sound does not
	auto changes = waterfall::Arrive(presence, 3, {});
	EXPECT_TRUE(changes.makeModel);
	EXPECT_FALSE(changes.makeSound);
	EXPECT_FALSE(changes.removeSound);
	EXPECT_FALSE(presence.sound);

	// A change of land and back gives the sound again
	waterfall::LandCleared(presence);
	changes = waterfall::Arrive(presence, 2, {.model = true});
	EXPECT_TRUE(changes.removeModel);
	EXPECT_FALSE(changes.removeSound);
	waterfall::LandCleared(presence);
	changes = waterfall::Arrive(presence, 3, {});
	EXPECT_TRUE(changes.makeModel);
	EXPECT_TRUE(changes.makeSound);
}

TEST(DesignedWaterfall, TheFourthLandNumbersAreTheGames)
{
	EXPECT_EQ(waterfall::k_SceneryLand, 4);
	EXPECT_EQ(waterfall::k_Ark.modelFile, "arche.l3d");
	EXPECT_EQ(std::bit_cast<uint32_t>(waterfall::k_Ark.x), 0x455D2000u);
	EXPECT_EQ(std::bit_cast<uint32_t>(waterfall::k_Ark.z), 0x45051000u);
	EXPECT_EQ(std::bit_cast<uint32_t>(waterfall::k_Ark.turn), 0x410F9097u);
	EXPECT_EQ(std::bit_cast<uint32_t>(waterfall::k_Ark.scale), 0x3F8CCCCDu);
	EXPECT_EQ(waterfall::k_Dinosaur.modelFile, "dinosaur.l3d");
	EXPECT_EQ(std::bit_cast<uint32_t>(waterfall::k_Dinosaur.x), 0x45282000u);
	EXPECT_EQ(std::bit_cast<uint32_t>(waterfall::k_Dinosaur.z), 0x4521E000u);
	EXPECT_EQ(std::bit_cast<uint32_t>(waterfall::k_Dinosaur.turn), 0x3FC8F5C3u);
	EXPECT_EQ(std::bit_cast<uint32_t>(waterfall::k_Dinosaur.scale), 0x3F800000u);
	EXPECT_EQ(std::bit_cast<uint32_t>(waterfall::k_SceneryImportance), 0x41200000u);
}

TEST(DesignedWaterfall, OnlyTheThirdAndFourthLandsHaveAModelInTheWaterfallsPlace)
{
	EXPECT_EQ(waterfall::ModelFileOf(3), waterfall::k_ModelFile);
	EXPECT_EQ(waterfall::ModelFileOf(4), waterfall::k_Ark.modelFile);
	EXPECT_FALSE(waterfall::ModelFileOf(1).has_value());
	EXPECT_FALSE(waterfall::ModelFileOf(2).has_value());
	EXPECT_FALSE(waterfall::ModelFileOf(5).has_value());
}

TEST(DesignedWaterfall, TheFourthLandGetsTheArkWithItsSoundAndTheDinosaurWithItsPrint)
{
	waterfall::Presence presence;
	auto changes = waterfall::Arrive(presence, 4, {});
	EXPECT_TRUE(changes.makeModel);
	EXPECT_TRUE(changes.makeSound);
	EXPECT_TRUE(changes.makeDinosaur);
	EXPECT_TRUE(changes.makePrint);
	EXPECT_FALSE(changes.removeModel);
	EXPECT_FALSE(changes.removeDinosaur);

	// The next frames change nothing
	changes = waterfall::Arrive(presence, 4, {.model = true, .dinosaur = true});
	EXPECT_FALSE(changes.makeModel);
	EXPECT_FALSE(changes.makeSound);
	EXPECT_FALSE(changes.makeDinosaur);
	EXPECT_FALSE(changes.makePrint);

	// The third land does not get the dinosaur
	presence = {};
	changes = waterfall::Arrive(presence, 3, {});
	EXPECT_FALSE(changes.makeDinosaur);
}

TEST(DesignedWaterfall, TheSameFourthLandAfterAClearHasNoSoundAndNoPrint)
{
	waterfall::Presence presence;
	static_cast<void>(waterfall::Arrive(presence, 4, {}));
	waterfall::LandCleared(presence);
	EXPECT_FALSE(presence.sound);
	EXPECT_FALSE(presence.print);
	// The frames lost the models with the land: they come back, but the sound and the print do not
	auto changes = waterfall::Arrive(presence, 4, {});
	EXPECT_TRUE(changes.makeModel);
	EXPECT_FALSE(changes.makeSound);
	EXPECT_TRUE(changes.makeDinosaur);
	EXPECT_FALSE(changes.makePrint);
	EXPECT_FALSE(changes.removeDinosaur);
}

TEST(DesignedWaterfall, AnotherLandRemovesTheArkAndTheDinosaur)
{
	waterfall::Presence presence;
	static_cast<void>(waterfall::Arrive(presence, 4, {}));
	auto changes = waterfall::Arrive(presence, 3, {.model = true, .dinosaur = true});
	EXPECT_TRUE(changes.removeModel);
	EXPECT_TRUE(changes.removeSound);
	EXPECT_TRUE(changes.removeDinosaur);
	// The third land then makes its waterfall in the same place among the models
	EXPECT_TRUE(changes.makeModel);
	EXPECT_TRUE(changes.makeSound);
	EXPECT_FALSE(changes.makeDinosaur);

	// A change back to the fourth land gives the sound and the print again
	changes = waterfall::Arrive(presence, 4, {.model = true});
	EXPECT_TRUE(changes.makeModel);
	EXPECT_TRUE(changes.makeSound);
	EXPECT_TRUE(changes.makeDinosaur);
	EXPECT_TRUE(changes.makePrint);
}
