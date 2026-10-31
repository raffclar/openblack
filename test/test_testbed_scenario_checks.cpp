/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Debug/TestbedScenarioChecks.h"
#include "Debug/TestbedScenarioRegistry.h"

using namespace openblack::testbed_scenarios;
using namespace openblack::testbed_scenarios::checks;

namespace
{
/// A world that gives one value for every check
Probe Gives(std::optional<double> value)
{
	return [value](const Check& /*check*/) { return value; };
}
} // namespace

TEST(TestbedScenarioChecks, ALimitIsHeldInclusively)
{
	const Check check {.atLeast = 2.0, .atMost = 4.0};
	EXPECT_FALSE(Holds(check, 1.99));
	EXPECT_TRUE(Holds(check, 2.0));
	EXPECT_TRUE(Holds(check, 4.0));
	EXPECT_FALSE(Holds(check, 4.01));
	EXPECT_TRUE(Holds({}, -1000.0)) << "A check with no limit holds for any value";
}

TEST(TestbedScenarioChecks, NoChecksGiveNoVerdictAndTheCodeForNone)
{
	Evaluator evaluator;
	evaluator.Start({});
	evaluator.Update(100, Gives(1.0));
	EXPECT_EQ(evaluator.GetVerdict(), Verdict::NoChecks);
	EXPECT_EQ(ExitCodeOf(Verdict::NoChecks), 2);
	EXPECT_EQ(ExitCodeOf(Verdict::Passed), 0);
	EXPECT_EQ(ExitCodeOf(Verdict::Failed), 1);
	EXPECT_EQ(ExitCodeOf(Verdict::Waiting), 2);
}

TEST(TestbedScenarioChecks, ACheckWaitsForItsTurnThenIsReadOnce)
{
	const std::vector<Check> list {{.afterTurns = 10, .atLeast = 1.0}};
	Evaluator evaluator;
	evaluator.Start(list);
	evaluator.Update(9, Gives(5.0));
	EXPECT_EQ(evaluator.GetVerdict(), Verdict::Waiting);
	EXPECT_FALSE(evaluator.Done());
	evaluator.Update(10, Gives(5.0));
	EXPECT_EQ(evaluator.GetVerdict(), Verdict::Passed);
	// A later read does not change the answer
	evaluator.Update(50, Gives(0.0));
	EXPECT_EQ(evaluator.GetVerdict(), Verdict::Passed);
	EXPECT_EQ(evaluator.GetResults()[0].turn, 10u);
}

TEST(TestbedScenarioChecks, AFailureDecidesTheVerdictAtOnce)
{
	const std::vector<Check> list {{.afterTurns = 0, .atLeast = 1.0}, {.afterTurns = 500, .atLeast = 1.0}};
	Evaluator evaluator;
	evaluator.Start(list);
	evaluator.Update(0, Gives(0.0));
	EXPECT_EQ(evaluator.GetVerdict(), Verdict::Failed) << "The second check still waits";
	EXPECT_FALSE(evaluator.Done());
}

TEST(TestbedScenarioChecks, ACheckWithALastTurnPassesWhenItHoldsAtAnyRead)
{
	const std::vector<Check> list {{.afterTurns = 0, .byTurn = 100, .atMost = 1.0}};
	Evaluator evaluator;
	evaluator.Start(list);
	evaluator.Update(0, Gives(9.0));
	evaluator.Update(50, Gives(9.0));
	EXPECT_EQ(evaluator.GetVerdict(), Verdict::Waiting);
	evaluator.Update(60, Gives(0.5));
	EXPECT_EQ(evaluator.GetVerdict(), Verdict::Passed);
}

TEST(TestbedScenarioChecks, ACheckWithALastTurnFailsWhenThatTurnGoesBy)
{
	const std::vector<Check> list {{.afterTurns = 0, .byTurn = 100, .atMost = 1.0}};
	Evaluator evaluator;
	evaluator.Start(list);
	evaluator.Update(99, Gives(9.0));
	EXPECT_EQ(evaluator.GetVerdict(), Verdict::Waiting);
	evaluator.Update(100, Gives(9.0));
	EXPECT_EQ(evaluator.GetVerdict(), Verdict::Failed);
}

TEST(TestbedScenarioChecks, AWorldWithNoValueFailsTheCheck)
{
	const std::vector<Check> list {{.afterTurns = 0}};
	Evaluator evaluator;
	evaluator.Start(list);
	evaluator.Update(0, Gives(std::nullopt));
	EXPECT_EQ(evaluator.GetVerdict(), Verdict::Failed);
	EXPECT_FALSE(evaluator.GetResults()[0].observed.has_value());
	EXPECT_NE(Describe(list[0], evaluator.GetResults()[0]).find("no value"), std::string::npos);
}

TEST(TestbedScenarioChecks, TheReportHasALineForEachCheckAndTheVerdict)
{
	const std::vector<Check> list {{.label = "it lives", .afterTurns = 0, .atLeast = 1.0},
	                               {.label = "later", .afterTurns = 99, .atLeast = 1.0}};
	Evaluator evaluator;
	evaluator.Start(list);
	evaluator.Update(0, Gives(1.0));
	const auto report = evaluator.Report("checks.example");
	ASSERT_EQ(report.size(), 3u);
	EXPECT_EQ(report[0].rfind("pass it lives", 0), 0u);
	EXPECT_EQ(report[1], "waiting later");
	EXPECT_EQ(report[2], "scenario checks.example: waiting");
}

TEST(TestbedScenarioChecks, StartingAgainForgetsTheLastScenario)
{
	const std::vector<Check> first {{.afterTurns = 0, .atLeast = 1.0}};
	const std::vector<Check> second {{.afterTurns = 0, .atLeast = 1.0}};
	Evaluator evaluator;
	evaluator.Start(first);
	evaluator.Update(0, Gives(0.0));
	ASSERT_EQ(evaluator.GetVerdict(), Verdict::Failed);
	evaluator.Start(second);
	EXPECT_EQ(evaluator.GetVerdict(), Verdict::Waiting);
}

TEST(TestbedScenarioChecks, TheScenariosThatStateChecksAreSane)
{
	size_t withChecks = 0;
	for (const auto& scenario : All())
	{
		if (scenario.checks.empty())
		{
			continue;
		}
		++withChecks;
		for (const auto& check : scenario.checks)
		{
			EXPECT_TRUE(check.atLeast.has_value() || check.atMost.has_value())
			    << scenario.id << ": a check with no limit passes whatever the world is";
			if (check.byTurn.has_value())
			{
				EXPECT_GE(*check.byTurn, check.afterTurns) << scenario.id;
			}
			if (check.quantity == Quantity::Entities)
			{
				EXPECT_FALSE(check.kind.empty()) << scenario.id << ": a count of things needs the kind of thing";
			}
		}
	}
	EXPECT_GE(withChecks, 2u);
}

TEST(TestbedScenarioChecks, AStoryLandIsNamedExactlyForAStoryLand)
{
	for (const auto& scenario : All())
	{
		EXPECT_EQ(scenario.environment.land == Land::Story, !scenario.environment.storyLand.empty()) << scenario.id;
	}
}
