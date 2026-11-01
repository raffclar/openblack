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
#include <vector>

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

TEST(SimRun, FastModeDrawsTheFrameOfAPicture)
{
	const Settings none {.fast = true};
	EXPECT_TRUE(DrawsFrame(none, 3, true));
	EXPECT_FALSE(DrawsFrame(none, 4, false));
	const Settings some {.fast = true, .drawEvery = 10};
	EXPECT_TRUE(DrawsFrame(some, 5, true));
	EXPECT_FALSE(DrawsFrame(some, 6));
}

TEST(SimRun, TheRunEndsAtItsLastTurn)
{
	EXPECT_FALSE(Finished(Settings {}, 1000000));
	const Settings settings {.untilTurn = 50};
	EXPECT_FALSE(Finished(settings, 49));
	EXPECT_TRUE(Finished(settings, 50));
	EXPECT_TRUE(Finished(settings, 51));
}

TEST(SimRun, AHeldRunThatAToolStepsSkipsItsFrames)
{
	EXPECT_TRUE(SkipsHeldFrame(true, true, false));
	// The game runs, or the tool does not step the run: each frame plays
	EXPECT_FALSE(SkipsHeldFrame(true, false, false));
	EXPECT_FALSE(SkipsHeldFrame(false, true, false));
	// The tool waits for the work of a frame, for example a scenario to lay out
	EXPECT_FALSE(SkipsHeldFrame(true, true, true));
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

TEST(SimRun, TheTurnTimeDoesNotIncludeTheRecordTime)
{
	Timings timings;
	// The record step of a turn ends before the time of the turn comes
	timings.AddRecord(2.0f);
	timings.AddTurn(5.0f);
	timings.AddTurn(4.0f);
	timings.AddRecord(1.0f);
	timings.AddRecord(0.5f);
	timings.AddTurn(1.0f);
	EXPECT_EQ(timings.turns, 3u);
	EXPECT_FLOAT_EQ(timings.turnLongestMs, 4.0f);
	EXPECT_FLOAT_EQ(timings.TurnAverageMs(), 7.0f / 3.0f);
	EXPECT_EQ(timings.records, 3u);
	EXPECT_FLOAT_EQ(timings.recordLongestMs, 2.0f);
	EXPECT_FLOAT_EQ(timings.RecordAverageMs(), 3.5f / 3.0f);
	EXPECT_FLOAT_EQ(timings.recordSinceTurnMs, 0.0f);
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

TEST(SimRun, TheConditionsSwitchReadsNamesNumbersAndALand)
{
	using openblack::game_end::Condition;
	using openblack::game_end::SwitchedCondition;
	const auto named = ParseConditions("WC_HOUSES_BUILT=3,WC_GROW_TREES=5");
	ASSERT_TRUE(named.has_value());
	EXPECT_TRUE(named->limitsLand.empty());
	ASSERT_EQ(named->conditions.size(), 2u);
	EXPECT_EQ(named->conditions[0], (SwitchedCondition {.condition = Condition::HousesBuilt, .value = 3}));
	EXPECT_EQ(named->conditions[1].condition, Condition::TreesGrown);
	const auto numbered = ParseConditions("mpm_2p_1:0=1,14=7");
	ASSERT_TRUE(numbered.has_value());
	EXPECT_EQ(numbered->limitsLand, "mpm_2p_1");
	EXPECT_EQ(numbered->conditions[0].condition, Condition::VillagersKilled);
	EXPECT_EQ(numbered->conditions[1].value, std::optional<int32_t>(7));
	// An empty text, an empty value, an unknown name, a number past the last condition or a condition two times does not
	// read
	EXPECT_FALSE(ParseConditions("").has_value());
	EXPECT_FALSE(ParseConditions("WC_HOUSES_BUILT=").has_value());
	EXPECT_FALSE(ParseConditions("WC_NOTHING=1").has_value());
	EXPECT_FALSE(ParseConditions("WC_NOTHING").has_value());
	EXPECT_FALSE(ParseConditions("15=1").has_value());
	EXPECT_FALSE(ParseConditions("15").has_value());
	EXPECT_FALSE(ParseConditions("3=x").has_value());
	EXPECT_FALSE(ParseConditions("3=1,WC_HOUSES_BUILT=2").has_value());
	EXPECT_FALSE(ParseConditions("3,WC_HOUSES_BUILT").has_value());
}

TEST(SimRun, AConditionWithNoValueKeepsTheFirstValueOfTheLand)
{
	using openblack::game_end::Condition;
	using openblack::game_end::ConditionPick;
	const auto request = ParseConditions("mpm_2p_1:WC_FOOD_IN_PITS,WC_HOUSES_BUILT=3");
	ASSERT_TRUE(request.has_value());
	ASSERT_EQ(request->conditions.size(), 2u);
	EXPECT_EQ(request->conditions[0].condition, Condition::FoodInStores);
	EXPECT_FALSE(request->conditions[0].value.has_value());
	// Only the limits of a land give that value, so a run with no land gets no picks
	EXPECT_FALSE(PicksWithValues(*request).has_value());
	const auto all = ParseConditions("WC_FOOD_IN_PITS=9000,WC_HOUSES_BUILT=3");
	ASSERT_TRUE(all.has_value());
	EXPECT_EQ(PicksWithValues(*all), (std::optional(std::vector<ConditionPick> {
	                                     {.condition = Condition::FoodInStores, .value = 9000},
	                                     {.condition = Condition::HousesBuilt, .value = 3},
	                                 })));
}
