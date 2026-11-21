/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Graphics/ScreenGlow.h"

using namespace openblack::graphics;

TEST(ScreenGlow, ThePointsSpreadOverTheScreen)
{
	// The columns are a sixteenth of the width apart, from the middle of the first sixteenth
	EXPECT_EQ(screen_glow::SamplePoint({640, 480}, {0, 0}), glm::ivec2(20, 15));
	EXPECT_EQ(screen_glow::SamplePoint({640, 480}, {15, 15}), glm::ivec2(620, 465));
	EXPECT_EQ(screen_glow::SamplePoint({1920, 1080}, {1, 1}), glm::ivec2(180, 100));
	// A width that does not divide by 16 drops the rest of the sixteenth
	EXPECT_EQ(screen_glow::SamplePoint({1366, 768}, {0, 0}), glm::ivec2(42, 24));
	EXPECT_EQ(screen_glow::SamplePoint({1366, 768}, {15, 0}).x, 42 + (15 * 85));
	// A height that does not divide by 16 rounds each row down
	EXPECT_EQ(screen_glow::SamplePoint({1366, 1000}, {3, 3}).y, 187 + 31);
}

TEST(ScreenGlow, APointMovesAnEighthOfTheWayCutTowardsZero)
{
	EXPECT_EQ(screen_glow::Step(0, 255), 0xFF000 / 8);
	EXPECT_EQ(screen_glow::Step(100, 0), 100 - 12);
	// Under 8 from the screen's value, a point stays where it is
	EXPECT_EQ(screen_glow::Step(7, 0), 7);
	EXPECT_EQ(screen_glow::Step((10 << 12) - 7, 10), (10 << 12) - 7);
}

TEST(ScreenGlow, APointStaysUnderTheTopLevel)
{
	EXPECT_EQ(screen_glow::Step(screen_glow::k_ChannelMost, 255), screen_glow::k_ChannelMost);
	int value = 0;
	for (int i = 0; i < 200; ++i)
	{
		value = screen_glow::Step(value, 255);
	}
	EXPECT_EQ(value, screen_glow::k_ChannelMost);
	EXPECT_EQ(screen_glow::Levels(glm::ivec3(value)), glm::ivec3(15));
}

TEST(ScreenGlow, BlueGoesToTheNearestLevelRedAndGreenStayLower)
{
	// Half way to level 1: blue goes up, red and green stay at 0
	EXPECT_EQ(screen_glow::Levels(glm::ivec3(0x8000)), glm::ivec3(0, 0, 1));
	// Just under level 2: blue goes up, red and green stay at 1
	EXPECT_EQ(screen_glow::Levels(glm::ivec3(0x1F000)), glm::ivec3(1, 1, 2));
	// Only the last 0x80 and 0x800 under a level take red and green up
	EXPECT_EQ(screen_glow::Levels(glm::ivec3(0x20000 - 0x80)), glm::ivec3(2, 2, 2));
	EXPECT_EQ(screen_glow::Levels(glm::ivec3(0x20000 - 0x81)), glm::ivec3(1, 2, 2));
}

TEST(ScreenGlow, TheGlowSpreadsFromTheFirstPixelToTheFarEdge)
{
	EXPECT_EQ(screen_glow::PointsAt({0.0f, 0.0f}, {640.0f, 480.0f}), glm::vec2(0.0f));
	EXPECT_EQ(screen_glow::PointsAt({640.0f, 480.0f}, {640.0f, 480.0f}), glm::vec2(15.0f));
	EXPECT_FLOAT_EQ(screen_glow::PointsAt({320.0f, 0.0f}, {640.0f, 480.0f}).x, 7.5f);
}

TEST(ScreenGlow, TheGlowAddsItsLevelTimesTheStrength)
{
	EXPECT_EQ(screen_glow::Shown(100, 0.0f, 90), 100);
	EXPECT_EQ(screen_glow::Shown(100, 15.0f, 0), 100);
	// The whole top level at full strength 90 adds 90
	EXPECT_EQ(screen_glow::Shown(100, 15.0f, 90), 190);
	EXPECT_EQ(screen_glow::Shown(200, 15.0f, 90), 255);
}

TEST(ScreenGlow, OnlyTheWeatherOptionShowsTheGlow)
{
	EXPECT_EQ(screen_glow::Strength(90, true), 90);
	EXPECT_EQ(screen_glow::Strength(90, false), 0);
	EXPECT_EQ(screen_glow::Strength(0, true), 0);
}

TEST(ScreenGlow, TheGlowAddsToItselfOverAStillScreen)
{
	// A still grey screen of 100 under a fully good sky: the points ease towards the screen with the glow added
	constexpr uint8_t k_Frame = 100;
	constexpr uint8_t k_Strength = 90;
	glm::ivec3 value(0);
	uint8_t shown = k_Frame;
	for (int i = 0; i < 400; ++i)
	{
		const auto level = static_cast<float>(screen_glow::Levels(value).b);
		shown = screen_glow::Shown(k_Frame, level, k_Strength);
		value =
		    glm::ivec3(screen_glow::Step(value.r, shown), screen_glow::Step(value.g, shown), screen_glow::Step(value.b, shown));
	}
	// The glow settles higher than the screen alone would give: 100 / (1 - 90 / 255) is about 155
	EXPECT_GT(shown, 140);
	EXPECT_LT(shown, 165);
	// Red and green settle lower than blue
	EXPECT_LE(screen_glow::Levels(value).r, screen_glow::Levels(value).b);
}
