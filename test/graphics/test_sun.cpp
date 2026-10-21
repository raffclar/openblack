/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Graphics/Sun.h"

namespace sun = openblack::graphics::sun;

TEST(Sun, RisesAndSetsWithTheScriptTime)
{
	EXPECT_FALSE(sun::Place(2.0f).has_value());
	EXPECT_FALSE(sun::Place(22.0f).has_value());

	// Coming up a third each hour from 3, on the horizon until 6
	const auto dawn = sun::Place(4.5f);
	ASSERT_TRUE(dawn.has_value());
	EXPECT_FLOAT_EQ(dawn->alpha, 127.5f);
	EXPECT_FLOAT_EQ(dawn->position.y, 0.0f);

	const auto noon = sun::Place(12.0f);
	ASSERT_TRUE(noon.has_value());
	EXPECT_FLOAT_EQ(noon->alpha, 255.0f);
	EXPECT_FLOAT_EQ(noon->position.y, 7500.0f);
	EXPECT_FLOAT_EQ(noon->position.x, -30000.0f);
	EXPECT_FLOAT_EQ(noon->position.z, -30000.0f);

	// Setting as it rose
	EXPECT_FLOAT_EQ(sun::Place(15.0f)->position.y, 3750.0f);
	EXPECT_FLOAT_EQ(sun::Place(19.5f)->alpha, 127.5f);
}

TEST(Sun, GlareEasesTowardsWhatShows)
{
	// A hundredth of the way each millisecond
	EXPECT_FLOAT_EQ(sun::EaseGlare(0.0f, 0, 10), 25.5f);
	// Two hidden samples aim at three fifths
	EXPECT_FLOAT_EQ(sun::EaseGlare(255.0f, 2, 50), 255.0f - (102.0f * 0.5f));
	// Paused, it stays put; a long frame doesn't overshoot past the bounds
	EXPECT_FLOAT_EQ(sun::EaseGlare(100.0f, 0, 0), 100.0f);
	EXPECT_FLOAT_EQ(sun::EaseGlare(100.0f, 0, 199), 255.0f);
	EXPECT_FLOAT_EQ(sun::EaseGlare(100.0f, 5, 199), 0.0f);
}

TEST(Sun, TheGlaresOpacityIsAWholeNumberThatTheOvercastDimsWithTheFog)
{
	// 200 x 255 / 255 = 200; half of it for a half strength sun, cut to a whole number
	EXPECT_EQ(sun::GlareAlpha(200.0f, 255.0f, 0.0f, true), 200.0f);
	EXPECT_EQ(sun::GlareAlpha(201.0f, 127.0f, 0.0f, true), 100.0f);
	// With the fog option an overcast of 0.5 divides it by 5; without the option it does nothing
	EXPECT_EQ(sun::GlareAlpha(200.0f, 255.0f, 0.5f, true), 40.0f);
	EXPECT_EQ(sun::GlareAlpha(200.0f, 255.0f, 0.5f, false), 200.0f);
	// An overcast above 1 counts as 1
	EXPECT_EQ(sun::GlareAlpha(255.0f, 255.0f, 3.0f, true), 28.0f);
}

TEST(Sun, WhatIsDrawnHidesTheGlareOnlyAtDetailLevelsThreeFourAndSix)
{
	for (uint8_t level = 0; level < 7; ++level)
	{
		EXPECT_EQ(sun::GlareTestsDepth(level), level == 3 || level == 4 || level == 6) << static_cast<int>(level);
	}
}
