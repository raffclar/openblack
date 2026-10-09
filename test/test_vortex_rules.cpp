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

#include "Magic/VortexRules.h"

using namespace openblack;
using namespace openblack::vortex;

namespace
{
constexpr uint32_t k_MillisecondsPerTurn = 100;
}

TEST(VortexRules, ElapsedSecondsCountsTurnsAndThePartOfOne)
{
	EXPECT_NEAR(ElapsedSeconds(10, 0.0f, k_MillisecondsPerTurn), 1.0, 1e-6);
	EXPECT_NEAR(ElapsedSeconds(10, 0.5f, k_MillisecondsPerTurn), 1.05, 1e-6);
	EXPECT_DOUBLE_EQ(ElapsedSeconds(0, 0.0f, k_MillisecondsPerTurn), 0.0);
}

TEST(VortexRules, AFadeIsOverOnTheSeventiethTurn)
{
	// The game's thousandth of a second is a float a hair over a thousandth, so 70 turns are just over 7 seconds
	EXPECT_EQ(Advance(VortexStateType::FadeIn, ElapsedSeconds(69, 0.0f, k_MillisecondsPerTurn)), Step::Stay);
	EXPECT_EQ(Advance(VortexStateType::FadeIn, ElapsedSeconds(70, 0.0f, k_MillisecondsPerTurn)), Step::Open);
	EXPECT_EQ(Advance(VortexStateType::FadeOut, ElapsedSeconds(69, 0.0f, k_MillisecondsPerTurn)), Step::Stay);
	EXPECT_EQ(Advance(VortexStateType::FadeOut, ElapsedSeconds(70, 0.0f, k_MillisecondsPerTurn)), Step::Remove);
	// An open vortex stays open
	EXPECT_EQ(Advance(VortexStateType::Active, 1000.0), Step::Stay);
	EXPECT_EQ(Advance(VortexStateType::Inactive, 1000.0), Step::Stay);
}

TEST(VortexRules, FadingInWaitsTwoSecondsThenEasesOpenOverFive)
{
	EXPECT_FLOAT_EQ(Openness(VortexStateType::FadeIn, 0.0f), 0.0f);
	EXPECT_FLOAT_EQ(Openness(VortexStateType::FadeIn, 1.99f), 0.0f);
	EXPECT_FLOAT_EQ(Openness(VortexStateType::FadeIn, 2.0f), 0.0f);
	EXPECT_FLOAT_EQ(Openness(VortexStateType::FadeIn, 4.5f), 0.5f);
	// A fifth of the way: (3 - 0.4) * 0.04
	EXPECT_NEAR(Openness(VortexStateType::FadeIn, 3.0f), 0.104f, 1e-6f);
	EXPECT_FLOAT_EQ(Openness(VortexStateType::FadeIn, 7.0f), 1.0f);
	EXPECT_FLOAT_EQ(Openness(VortexStateType::FadeIn, 9.0f), 1.0f);
}

TEST(VortexRules, FadingOutEasesShutOverFiveSeconds)
{
	EXPECT_FLOAT_EQ(Openness(VortexStateType::FadeOut, 0.0f), 1.0f);
	EXPECT_FLOAT_EQ(Openness(VortexStateType::FadeOut, 2.5f), 0.5f);
	EXPECT_FLOAT_EQ(Openness(VortexStateType::FadeOut, 5.0f), 0.0f);
	EXPECT_FLOAT_EQ(Openness(VortexStateType::FadeOut, 6.0f), 0.0f);
}

TEST(VortexRules, OpenIsFullyOpenAndInactiveIsShut)
{
	EXPECT_FLOAT_EQ(Openness(VortexStateType::Active, 0.0f), 1.0f);
	EXPECT_FLOAT_EQ(Openness(VortexStateType::Inactive, 3.0f), 0.0f);
}

TEST(VortexRules, TheLevellingEasesOutAndIsCompleteOnceFadingOut)
{
	EXPECT_FLOAT_EQ(LevelAmount(VortexStateType::FadeIn, 1.0f), 0.0f);
	// Half open: one less a quarter
	EXPECT_FLOAT_EQ(LevelAmount(VortexStateType::FadeIn, 4.5f), 0.75f);
	EXPECT_FLOAT_EQ(LevelAmount(VortexStateType::Active, 0.0f), 1.0f);
	EXPECT_FLOAT_EQ(LevelAmount(VortexStateType::FadeOut, 0.0f), 1.0f);
	EXPECT_FLOAT_EQ(LevelAmount(VortexStateType::FadeOut, 6.0f), 1.0f);
	EXPECT_FLOAT_EQ(LevelAmount(VortexStateType::Inactive, 0.0f), 0.0f);
}

TEST(VortexRules, TheGlowRisesSettlesAndDies)
{
	EXPECT_FLOAT_EQ(GlowBrightness(VortexStateType::FadeIn, 0.0f), 0.0f);
	EXPECT_FLOAT_EQ(GlowBrightness(VortexStateType::FadeIn, 1.0f), 0.5f);
	EXPECT_FLOAT_EQ(GlowBrightness(VortexStateType::FadeIn, 2.0f), 1.0f);
	EXPECT_NEAR(GlowBrightness(VortexStateType::FadeIn, 4.5f), 0.8f, 1e-6f);
	EXPECT_NEAR(GlowBrightness(VortexStateType::FadeIn, 8.0f), 0.6f, 1e-6f);
	EXPECT_NEAR(GlowBrightness(VortexStateType::Active, 0.0f), 0.6f, 1e-6f);
	EXPECT_NEAR(GlowBrightness(VortexStateType::FadeOut, 0.0f), 0.6f, 1e-6f);
	EXPECT_FLOAT_EQ(GlowBrightness(VortexStateType::FadeOut, 5.0f), 1.0f);
	EXPECT_FLOAT_EQ(GlowBrightness(VortexStateType::FadeOut, 6.0f), 0.5f);
	EXPECT_FLOAT_EQ(GlowBrightness(VortexStateType::FadeOut, 7.0f), 0.0f);
	EXPECT_FLOAT_EQ(GlowBrightness(VortexStateType::Inactive, 1.0f), 0.0f);
}

TEST(VortexRules, TheGroundRingIsNeverQuiteOpaque)
{
	EXPECT_EQ(GroundRingAlpha(0.0f), 0);
	EXPECT_EQ(GroundRingAlpha(0.5f), 127);
	EXPECT_EQ(GroundRingAlpha(0.9f), 229);
	EXPECT_EQ(GroundRingAlpha(1.0f), k_GroundRingMaxAlpha);
}

TEST(VortexRules, TheSwirlRisesTowardsTheGroundAsTheLandIsLevelled)
{
	EXPECT_FLOAT_EQ(SwirlDepth(0.0f), 2.5f);
	EXPECT_FLOAT_EQ(SwirlDepth(1.0f), 0.3f);
}

TEST(VortexRules, OnlyTheLandVorticesLevelTheGround)
{
	EXPECT_TRUE(LevelsGround(VortexType::In));
	EXPECT_TRUE(LevelsGround(VortexType::Out));
	EXPECT_FALSE(LevelsGround(VortexType::Volcano));
}

TEST(VortexRules, TheSquareIsElevenCellsRoundTheVortexsCell)
{
	EXPECT_EQ(k_LevelCellCount, 121u);
	const auto centre = CentreCell({123.9f, 45.2f});
	EXPECT_EQ(centre, glm::ivec2(12, 4));
	EXPECT_EQ(SquareCell(centre, 0), glm::ivec2(7, -1));
	// Along z first
	EXPECT_EQ(SquareCell(centre, 1), glm::ivec2(7, 0));
	EXPECT_EQ(SquareCell(centre, 11), glm::ivec2(8, -1));
	EXPECT_EQ(SquareCell(centre, 60), glm::ivec2(12, 4));
	EXPECT_EQ(SquareCell(centre, 120), glm::ivec2(17, 9));
}

TEST(VortexRules, TheAverageCountsEveryCell)
{
	const std::array<uint8_t, 4> altitudes {10, 20, 0, 30};
	EXPECT_FLOAT_EQ(AverageAltitude(altitudes), 15.0f);
	EXPECT_FLOAT_EQ(AverageAltitude({}), 0.0f);
}

TEST(VortexRules, TheLandIsPulledWhollyWithinFiftyAndLessOutToFiftySix)
{
	EXPECT_FLOAT_EQ(LevelWeight(0.0f), 1.0f);
	EXPECT_FLOAT_EQ(LevelWeight(49.9f), 1.0f);
	EXPECT_FLOAT_EQ(LevelWeight(50.0f), 1.0f);
	EXPECT_FLOAT_EQ(LevelWeight(53.0f), 0.5f);
	EXPECT_FLOAT_EQ(LevelWeight(56.0f), 0.0f);
	EXPECT_FLOAT_EQ(LevelWeight(57.0f), 0.0f);
}

TEST(VortexRules, ACellIsPulledTowardsTheAverageByTheAmount)
{
	EXPECT_EQ(LevelledAltitude(100, 50.0f, 10.0f, 0.0f), 100);
	EXPECT_EQ(LevelledAltitude(100, 50.0f, 10.0f, 0.5f), 75);
	EXPECT_EQ(LevelledAltitude(100, 50.0f, 10.0f, 1.0f), 50);
	// Halfway between 50 and 56 it goes half as far
	EXPECT_EQ(LevelledAltitude(100, 50.0f, 53.0f, 1.0f), 75);
	EXPECT_EQ(LevelledAltitude(100, 50.0f, 60.0f, 1.0f), 100);
	// Rounded to the nearest, halves to the even height
	EXPECT_EQ(LevelledAltitude(10, 11.0f, 0.0f, 0.5f), 10);
	EXPECT_EQ(LevelledAltitude(11, 12.0f, 0.0f, 0.5f), 12);
	EXPECT_EQ(LevelledAltitude(0, 255.0f, 0.0f, 1.0f), 255);
}

TEST(VortexRules, TheSquareFlattensRoundItsMiddleAndLeavesItsCorners)
{
	// A peak in the middle of a square of height 10, the vortex on the peak's cell
	std::vector<uint8_t> originals(k_LevelCellCount, 10);
	originals[60] = 131;
	const auto average = AverageAltitude(originals);
	EXPECT_FLOAT_EQ(average, 11.0f);
	const glm::vec2 centre {120.0f, 40.0f};
	const auto levelled = LevelSquare(centre, originals, average, 1.0f);
	// The peak and the land within 50 come to the average
	EXPECT_EQ(levelled[60], 11);
	EXPECT_EQ(levelled[61], 11);
	// A corner, 70 away, keeps its height
	EXPECT_EQ(levelled[0], 10);
	EXPECT_EQ(levelled[120], 10);
	// Not levelled at all: the heights are those of the start
	const auto untouched = LevelSquare(centre, originals, average, 0.0f);
	EXPECT_EQ(untouched[60], 131);
	EXPECT_EQ(untouched[0], 10);
}
