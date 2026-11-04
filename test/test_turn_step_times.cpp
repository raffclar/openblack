/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <string>

#include <gtest/gtest.h>

#include "Simulation/TurnStepTimes.h"

using openblack::game_turn::Step;
using openblack::simulation::TurnStepTimes;

TEST(TurnStepTimes, AStepThatTheTurnDoesTwiceAddsItsTimes)
{
	TurnStepTimes times;
	times.Add(Step::Living, 1.5f);
	times.Add(Step::Living, 2.0f);
	EXPECT_FLOAT_EQ(times.Total(), 3.5f);
	EXPECT_EQ(times.Report(0.1f), std::to_string(static_cast<int>(Step::Living)) + "=3.50");
}

TEST(TurnStepTimes, TheReportGivesTheLongestStepFirstAndLeavesOutTheShortSteps)
{
	TurnStepTimes times;
	times.Add(Step::PlayerCommands, 1.0f);
	times.Add(Step::Living, 4.0f);
	times.Add(Step::TempleLooks, 0.05f);
	const auto living = std::to_string(static_cast<int>(Step::Living));
	const auto commands = std::to_string(static_cast<int>(Step::PlayerCommands));
	EXPECT_EQ(times.Report(0.1f), living + "=4.00 " + commands + "=1.00");
	EXPECT_FLOAT_EQ(times.Total(), 5.05f);
}

TEST(TurnStepTimes, StepsWithTheSameTimeKeepTheOrderOfTheirNumbers)
{
	TurnStepTimes times;
	times.Add(Step::Living, 2.0f);
	times.Add(Step::PlayerCommands, 2.0f);
	const auto living = std::to_string(static_cast<int>(Step::Living));
	const auto commands = std::to_string(static_cast<int>(Step::PlayerCommands));
	EXPECT_EQ(times.Report(0.0f), commands + "=2.00 " + living + "=2.00");
}

TEST(TurnStepTimes, ATurnWithNoStepsGivesAnEmptyReport)
{
	const TurnStepTimes times;
	EXPECT_FLOAT_EQ(times.Total(), 0.0f);
	EXPECT_TRUE(times.Report(0.0f).empty());
}
