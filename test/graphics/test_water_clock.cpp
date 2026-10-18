/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the clock of the sea

#include <chrono>

#include <gtest/gtest.h>

#include "Graphics/WaterClock.h"

using openblack::graphics::underwater::WaterClock;
using std::chrono::milliseconds;

TEST(WaterClock, APausedGameKeepsTheWaterStill)
{
	WaterClock clock;
	clock.Advance(milliseconds(250));
	const float before = clock.GetSeconds();
	for (int frame = 0; frame < 1000; ++frame)
	{
		clock.Advance(milliseconds(0));
	}
	EXPECT_EQ(clock.GetSeconds(), before);
}

TEST(WaterClock, EachStepMovesTheClockOnByExactlyTheStep)
{
	WaterClock clock;
	// The frames of a recorded clip each take 40 milliseconds of game time
	for (int frame = 0; frame < 25; ++frame)
	{
		clock.Advance(milliseconds(40));
	}
	EXPECT_EQ(clock.GetMilliseconds(), 1000u);
	EXPECT_FLOAT_EQ(clock.GetSeconds(), 1.0f);
	// Many small steps do not add errors
	for (int frame = 0; frame < 360000; ++frame)
	{
		clock.Advance(milliseconds(7));
	}
	EXPECT_EQ(clock.GetMilliseconds(), 1000u + (360000u * 7u));
}

TEST(WaterClock, ItNeverGoesBack)
{
	WaterClock clock;
	// A step that goes back does nothing
	clock.Advance(milliseconds(5000));
	clock.Advance(milliseconds(-7000));
	EXPECT_EQ(clock.GetMilliseconds(), 5000u);
	// The clock does not wrap after an hour, and the seconds never get smaller, frame by frame for five hours
	float last = clock.GetSeconds();
	for (int frame = 0; frame < (5 * 3600 * 100); ++frame)
	{
		clock.Advance(milliseconds(frame % 3 == 0 ? 11 : 10));
		const float now = clock.GetSeconds();
		ASSERT_GE(now, last) << "frame " << frame;
		last = now;
	}
	EXPECT_GT(last, 5.0f * 3600.0f);
}

TEST(WaterClock, ItKeepsAMillisecondForFourHours)
{
	WaterClock clock;
	clock.Advance(milliseconds(4 * 3600 * 1000));
	const float before = clock.GetSeconds();
	clock.Advance(milliseconds(1));
	EXPECT_GT(clock.GetSeconds(), before);
	EXPECT_NEAR(clock.GetSeconds() - before, 0.001f, 0.001f);
}
