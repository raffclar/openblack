/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <chrono>
#include <optional>

#include <gtest/gtest.h>

#include "Debug/SimRun.h"

using namespace openblack::sim_run;
using namespace std::chrono_literals;

TEST(SimRun, TheUsualGameDrawsEveryFrame)
{
	const Settings settings {};
	EXPECT_TRUE(DrawsFrame(settings, 0));
	EXPECT_TRUE(DrawsFrame(settings, 7));
}

TEST(SimRun, FastModeDrawsNoFrameByDefault)
{
	const Settings settings {.fast = true};
	EXPECT_FALSE(DrawsFrame(settings, 0));
	EXPECT_FALSE(DrawsFrame(settings, 100));
}

TEST(SimRun, FastModeDrawsOneFrameInSome)
{
	const Settings settings {.fast = true, .drawEvery = 10};
	EXPECT_TRUE(DrawsFrame(settings, 0));
	EXPECT_FALSE(DrawsFrame(settings, 5));
	EXPECT_TRUE(DrawsFrame(settings, 20));
}

TEST(SimRun, TheRunEndsAtItsLastTurn)
{
	EXPECT_FALSE(Finished(Settings {}, 1000000));
	const Settings settings {.untilTurn = 50};
	EXPECT_FALSE(Finished(settings, 49));
	EXPECT_TRUE(Finished(settings, 50));
	EXPECT_TRUE(Finished(settings, 51));
}

TEST(SimRun, ThePacedRunWaitsForTheRestOfTheFrame)
{
	const Settings paced {.paceToWallClock = true};
	EXPECT_EQ(PaceWait(paced, 20ms, 5ms), 15ms);
	EXPECT_EQ(PaceWait(paced, 20ms, 25ms), 0ms);
	EXPECT_EQ(PaceWait(paced, std::nullopt, 5ms), 0ms);
}

TEST(SimRun, FastAndUnpacedRunsDoNotWait)
{
	EXPECT_EQ(PaceWait(Settings {.fast = true, .paceToWallClock = true}, 20ms, 1ms), 0ms);
	EXPECT_EQ(PaceWait(Settings {}, 20ms, 1ms), 0ms);
}

TEST(SimRun, TimingsGiveAveragesAndTheLongest)
{
	Timings timings;
	EXPECT_EQ(timings.TurnAverageMs(), 0.0f);
	timings.AddTurn(2.0f);
	timings.AddTurn(4.0f);
	timings.AddFrame(1.0f);
	EXPECT_EQ(timings.turns, 2u);
	EXPECT_FLOAT_EQ(timings.TurnAverageMs(), 3.0f);
	EXPECT_FLOAT_EQ(timings.turnLongestMs, 4.0f);
	EXPECT_FLOAT_EQ(timings.FrameAverageMs(), 1.0f);
}

TEST(SimRun, TimingsAddTheStagesOfEachFrame)
{
	Timings timings;
	const std::array<float, 3> first {1.0f, 0.0f, 2.0f};
	const std::array<float, 2> second {0.5f, 1.5f};
	timings.AddStages(first);
	timings.AddStages(second);
	ASSERT_EQ(timings.stageTotalMs.size(), 3u);
	EXPECT_FLOAT_EQ(timings.stageTotalMs[0], 1.5f);
	EXPECT_FLOAT_EQ(timings.stageTotalMs[1], 1.5f);
	EXPECT_FLOAT_EQ(timings.stageTotalMs[2], 2.0f);
}

TEST(SimRun, AFrameTimeCanHaveAPartOfAMillisecond)
{
	EXPECT_EQ(FrameTimeFromMs(16.7), std::optional(std::chrono::microseconds(16700)));
	EXPECT_EQ(FrameTimeFromMs(20.0), std::optional(std::chrono::microseconds(20000)));
	EXPECT_FALSE(FrameTimeFromMs(0.5).has_value());
	EXPECT_FALSE(FrameTimeFromMs(1000.5).has_value());
}
