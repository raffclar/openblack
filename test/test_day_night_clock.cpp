/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "3D/DayNightClock.h"

using openblack::DayNightClock;

namespace
{
DayNightClock ResetClock()
{
	DayNightClock clock;
	clock.Reset();
	return clock;
}
} // namespace

TEST(DayNightClock, TheGameCycleHasShortNights)
{
	const auto clock = ResetClock();
	// Night is 8.3% of a half day, about an hour, and the sky turns a quarter of the change's 0.84 hours either side
	const auto& times = clock.GetVisualTimes();
	EXPECT_NEAR(times[0], 0.786f, 1e-4f);
	EXPECT_NEAR(times[1], 1.206f, 1e-4f);
	EXPECT_NEAR(times[2], 1.626f, 1e-4f);
	EXPECT_NEAR(times[3], 2.046f, 1e-4f);
}

TEST(DayNightClock, ScriptTimeFallsOnTheStandardHours)
{
	const auto clock = ResetClock();
	const auto& times = clock.GetVisualTimes();
	for (size_t i = 0; i < times.size(); ++i)
	{
		EXPECT_NEAR(clock.ScriptToVisual(DayNightClock::k_ScriptTimes.at(i)), times.at(i), 1e-5f);
		EXPECT_NEAR(clock.VisualToScript(times.at(i)), DayNightClock::k_ScriptTimes.at(i), 1e-5f);
	}
	// Noon and midnight stay, and the evening mirrors the morning
	EXPECT_FLOAT_EQ(clock.ScriptToVisual(12.0f), 12.0f);
	EXPECT_FLOAT_EQ(clock.ScriptToVisual(0.0f), 0.0f);
	EXPECT_NEAR(clock.ScriptToVisual(20.5f), 24.0f - clock.ScriptToVisual(3.5f), 1e-5f);
	EXPECT_FLOAT_EQ(clock.GetScriptTime(), 12.0f);
}

TEST(DayNightClock, TheSkyTurnsAtTheVisualHours)
{
	const auto clock = ResetClock();
	EXPECT_FLOAT_EQ(clock.SkyType(0.5f), 0.0f);
	EXPECT_FLOAT_EQ(clock.SkyType(1.4f), 1.0f);
	EXPECT_FLOAT_EQ(clock.SkyType(12.0f), 2.0f);
	EXPECT_FLOAT_EQ(clock.SkyType(23.5f), 0.0f);
}

TEST(DayNightClock, TheGameCountsTheSkyFromTwoAtNight)
{
	const auto clock = ResetClock();
	EXPECT_FLOAT_EQ(clock.NightSkyType(0.5f), 2.0f);
	EXPECT_FLOAT_EQ(clock.NightSkyType(1.4f), 1.0f);
	EXPECT_FLOAT_EQ(clock.NightSkyType(12.0f), 0.0f);
	// From dusk to day the game subtracts the ramp from 1. Two minus the sky type of openblack can be one bit off.
	const auto& times = clock.GetVisualTimes();
	int differences = 0;
	constexpr int k_Steps = 4000;
	for (int step = 0; step < k_Steps; ++step)
	{
		const float hour = times[2] + ((times[3] - times[2]) * static_cast<float>(step) / static_cast<float>(k_Steps));
		const float game = 1.0f - ((hour - times[2]) / (times[3] - times[2]));
		EXPECT_EQ(clock.NightSkyType(hour), game);
		differences += (2.0f - clock.SkyType(hour)) != game ? 1 : 0;
	}
	EXPECT_GT(differences, 0);
}

TEST(DayNightClock, NightIsASkyTypeOfMoreThanTheDouble1Point2)
{
	EXPECT_FALSE(DayNightClock::IsNight(1.0f));
	EXPECT_FALSE(DayNightClock::IsNight(1.19999993f));
	EXPECT_TRUE(DayNightClock::IsNight(1.2f));
	EXPECT_TRUE(DayNightClock::IsNight(2.0f));
	// The ramp value 0.8 gives 1.2 as a float, which is night; openblack's old test of less than 0.8 said day
	EXPECT_TRUE(DayNightClock::IsNight(2.0f - 0.8f));
}

TEST(DayNightClock, ADayLastsItsDuration)
{
	auto clock = ResetClock();
	// 1700 seconds is 708 tenths of a second an hour; a turn is a tenth of a second
	constexpr float k_HoursPerTurn = 10.0f / 708.0f * 0.1f;
	for (int turn = 0; turn < 100; ++turn)
	{
		clock.ProcessTurn();
	}
	EXPECT_NEAR(clock.GetVisualTime(), 12.0f + 100.0f * k_HoursPerTurn, 1e-4f);

	// Stopped, the time stays
	const float stopped = clock.GetVisualTime();
	clock.SetRunning(false);
	clock.ProcessTurn();
	EXPECT_FLOAT_EQ(clock.GetVisualTime(), stopped);
}

TEST(DayNightClock, MovesToAnHourInItsSeconds)
{
	auto clock = ResetClock();
	clock.SetRunning(false);
	// Noon back to midnight, twelve hours in ten seconds: 0.12 hours a turn
	clock.MoveScriptTime(0.0f, 10.0f);
	for (int turn = 0; turn < 50; ++turn)
	{
		clock.ProcessTurn();
	}
	EXPECT_NEAR(clock.GetVisualTime(), 6.0f, 1e-3f);
	for (int turn = 0; turn < 51; ++turn)
	{
		clock.ProcessTurn();
	}
	EXPECT_NEAR(clock.GetVisualTime(), 0.0f, 1e-3f);
}

TEST(DayNightClock, ALandsCycleIsClamped)
{
	DayNightClock clock;
	clock.SetCycleFromLand(1700.0f, 2.0f, 0.5f);
	// All night: the change is none
	const auto& times = clock.GetVisualTimes();
	EXPECT_FLOAT_EQ(times[0], 12.0f);
	EXPECT_FLOAT_EQ(times[3], 12.0f);
}

// A land's script gives the numbers of its cycle: the night is at most the whole day, and the change at most the rest
TEST(DayNightClock, ALandsCycleNumbersAreClamped)
{
	const auto all = DayNightClock::LandCycle({.duration = 1000.0f, .night = 2.0f, .change = 0.5f});
	EXPECT_EQ(all.duration, 1000.0f);
	EXPECT_EQ(all.night, 1.0f);
	EXPECT_EQ(all.change, 0.0f);
	const auto some = DayNightClock::LandCycle({.duration = 1000.0f, .night = 0.6f, .change = 0.5f});
	EXPECT_EQ(some.night, 0.6f);
	EXPECT_EQ(some.change, 1.0f - 0.6f);
	const auto within = DayNightClock::LandCycle({.duration = 1700.0f, .night = 0.23f, .change = 0.17f});
	EXPECT_EQ(within.night, 0.23f);
	EXPECT_EQ(within.change, 0.17f);
}

// A saved game sets the hour of visual time at once: a move to another hour stops, and an hour past the day wraps
TEST(DayNightClock, ASavedGameSetsTheVisualHour)
{
	DayNightClock clock;
	clock.Reset();
	clock.MoveScriptTime(20.0f, 30.0f);
	clock.ForceVisualTime(22.25f);
	EXPECT_EQ(clock.GetVisualTime(), 22.25f);
	clock.SetScale(0.0f);
	EXPECT_FALSE(clock.IsRunning());
	clock.ProcessTurn();
	EXPECT_EQ(clock.GetVisualTime(), 22.25f);
	clock.ForceVisualTime(25.0f);
	EXPECT_EQ(clock.GetVisualTime(), 1.0f);
}
