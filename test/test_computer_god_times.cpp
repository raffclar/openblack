/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <string>

#include <Debug/ComputerGodTimes.h>
#include <gtest/gtest.h>

using namespace openblack::computer_god_times;

TEST(ComputerGodTimes, MindIsTheTurnLessTheSeenListAndTheHand)
{
	Times times;
	times.Add(1, 10, 0.5, 1.0, 4.0);
	const auto& god = times.Gods().at(1);
	EXPECT_EQ(god.turns, 1u);
	EXPECT_DOUBLE_EQ(god.mind.totalMs, 2.5);
	EXPECT_DOUBLE_EQ(god.hand.totalMs, 1.0);
	EXPECT_DOUBLE_EQ(god.see.totalMs, 0.5);
	EXPECT_DOUBLE_EQ(god.turn.totalMs, 4.0);
}

TEST(ComputerGodTimes, KeepsTheLongestTurnOfEachPart)
{
	Times times;
	times.Add(2, 10, 0.0, 3.0, 4.0);
	times.Add(2, 11, 0.0, 1.0, 9.0);
	times.Add(2, 12, 0.0, 2.0, 2.0);
	const auto& god = times.Gods().at(2);
	EXPECT_EQ(god.turns, 3u);
	EXPECT_DOUBLE_EQ(god.hand.longestMs, 3.0);
	EXPECT_EQ(god.hand.longestTurn, 10u);
	EXPECT_DOUBLE_EQ(god.mind.longestMs, 8.0);
	EXPECT_EQ(god.mind.longestTurn, 11u);
	EXPECT_DOUBLE_EQ(god.mind.totalMs, 9.0);
}

TEST(ComputerGodTimes, ReportHasOneLineForEachGodInNumberOrder)
{
	Times times;
	times.Add(3, 5, 0.0, 0.0, 1.0);
	times.Add(0, 5, 0.0, 0.0, 2.0);
	const auto report = times.Report(5);
	EXPECT_LT(report.find("god 0 "), report.find("god 3 "));
	EXPECT_NE(report.find("turn 2.0000/2.0000@5"), std::string::npos);
}

TEST(ComputerGodTimes, NoTurnsGiveAnEmptyReport)
{
	EXPECT_TRUE(Times {}.Report(0).empty());
}
