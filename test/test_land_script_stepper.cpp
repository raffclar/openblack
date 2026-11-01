/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Editor/Scripts/LandScriptStepper.h"
#include "LHScriptX/Script.h"

using namespace openblack;
using openblack::editor::scripts::LandScriptStepper;
using State = LandScriptStepper::State;

namespace
{
/// A made-up land script: comment lines, a word that is no command, and four commands
constexpr const char* k_Land = "// A land for the test\n"
                               "VERSION(2.3)\n"
                               "\n"
                               "SET_LAND_NUMBER(2)\n"
                               "NOT_A_COMMAND(1)\n"
                               "CREATE_TOWN(0, \"10.0,20.0\", \"PLAYER_ONE\", 0, \"Norse\")\n"
                               "SET_TOWN_BELIEF(0, \"PLAYER_ONE\", 1.0\n";

/// Keeps the index of each command that it does, and fails at one index
struct FakeRun
{
	std::vector<size_t> done;
	std::optional<size_t> failAt;

	[[nodiscard]] LandScriptStepper::Run Function()
	{
		return [this](size_t index) {
			done.push_back(index);
			return failAt != index;
		};
	}
};
} // namespace

TEST(LandScriptRead, ReadsEachCommandWithItsLineAndArguments)
{
	const auto read = lhscriptx::Script::Read(k_Land);

	ASSERT_FALSE(read.error.has_value());
	ASSERT_EQ(read.commands.size(), 4u);
	EXPECT_EQ(read.commands[0].name, "VERSION");
	EXPECT_EQ(read.commands[0].line, 2);
	EXPECT_EQ(read.commands[1].name, "SET_LAND_NUMBER");
	EXPECT_EQ(read.commands[1].line, 4);
	EXPECT_EQ(read.commands[2].name, "CREATE_TOWN");
	EXPECT_EQ(read.commands[2].line, 6);
	EXPECT_EQ(read.commands[2].arguments.size(), 5u);
	// A command that has no ')' ends at the end of its line
	EXPECT_EQ(read.commands[3].name, "SET_TOWN_BELIEF");
	EXPECT_EQ(read.commands[3].line, 7);
	EXPECT_EQ(read.commands[3].arguments.size(), 3u);
}

TEST(LandScriptRead, StopsAtALineThatItCannotReadAndKeepsTheCommandsBefore)
{
	const auto read = lhscriptx::Script::Read("SET_LAND_NUMBER(1)\nVERSION 2.3)\nSET_LAND_NUMBER(2)\n");

	ASSERT_TRUE(read.error.has_value());
	ASSERT_EQ(read.commands.size(), 1u);
	EXPECT_EQ(read.commands[0].name, "SET_LAND_NUMBER");
}

TEST(LandScriptRead, ACommandWithTheWrongArgumentsDoesNotRun)
{
	lhscriptx::Script script;
	const auto read = lhscriptx::Script::Read("VERSION()\n");
	ASSERT_EQ(read.commands.size(), 1u);

	EXPECT_THROW(script.Run(read.commands[0]), std::runtime_error);
	EXPECT_THROW(script.Run({.name = "NOT_A_COMMAND", .arguments = {}, .line = 1}), std::runtime_error);
}

TEST(LandScriptRead, ShowsTheArgumentsAsTheScriptGivesThem)
{
	const auto read = lhscriptx::Script::Read(k_Land);
	ASSERT_EQ(read.commands.size(), 4u);

	EXPECT_EQ(editor::scripts::ArgumentsText(read.commands[2]), "0, \"10.0,20.0\", \"PLAYER_ONE\", 0, \"Norse\"");
	EXPECT_EQ(editor::scripts::ArgumentsText(read.commands[1]), "2");
}

TEST(LandScriptRead, AFilterLooksAtTheNameAndTheArgumentsInAnyCase)
{
	const auto read = lhscriptx::Script::Read(k_Land);
	ASSERT_EQ(read.commands.size(), 4u);

	EXPECT_TRUE(editor::scripts::CommandMatches(read.commands[2], ""));
	EXPECT_TRUE(editor::scripts::CommandMatches(read.commands[2], "create_town"));
	EXPECT_TRUE(editor::scripts::CommandMatches(read.commands[2], "norse"));
	EXPECT_FALSE(editor::scripts::CommandMatches(read.commands[1], "norse"));
}

TEST(LandScriptStepper, StepsDoTheCommandsInTheirOrder)
{
	LandScriptStepper stepper(3);
	FakeRun run;

	EXPECT_TRUE(stepper.Step(run.Function()));
	EXPECT_TRUE(stepper.Step(run.Function()));
	EXPECT_EQ(stepper.Next(), 2u);
	EXPECT_TRUE(stepper.Step(run.Function()));
	EXPECT_FALSE(stepper.Next().has_value());
	EXPECT_FALSE(stepper.Step(run.Function()));

	EXPECT_EQ(run.done, (std::vector<size_t> {0, 1, 2}));
	EXPECT_EQ(stepper.StateOf(2), State::Done);
}

TEST(LandScriptStepper, ContinueStopsBeforeABreakpointAndThenGoesPastIt)
{
	LandScriptStepper stepper(5);
	stepper.ToggleBreakpoint(3);
	FakeRun run;

	EXPECT_EQ(stepper.Continue(run.Function()), 3u);
	EXPECT_EQ(stepper.Next(), 3u);
	EXPECT_EQ(stepper.Continue(run.Function()), 2u);
	EXPECT_FALSE(stepper.Next().has_value());
	EXPECT_EQ(run.done, (std::vector<size_t> {0, 1, 2, 3, 4}));
}

TEST(LandScriptStepper, ARunStopsAfterACommandThatFailed)
{
	LandScriptStepper stepper(4);
	FakeRun run {.done = {}, .failAt = 1};

	EXPECT_EQ(stepper.Continue(run.Function()), 2u);
	EXPECT_EQ(stepper.StateOf(1), State::Failed);
	EXPECT_EQ(stepper.Next(), 2u);
}

TEST(LandScriptStepper, RunToStopsBeforeTheCommandAndSkipDoesNotDoIt)
{
	LandScriptStepper stepper(4);
	FakeRun run;

	EXPECT_EQ(stepper.RunTo(2, run.Function()), 2u);
	EXPECT_EQ(stepper.Next(), 2u);
	stepper.Skip();
	EXPECT_EQ(stepper.StateOf(2), State::Skipped);
	EXPECT_EQ(stepper.RunTo(99, run.Function()), 1u);
	EXPECT_EQ(run.done, (std::vector<size_t> {0, 1, 3}));
}

TEST(LandScriptStepper, RestartKeepsTheBreakpointsAndResetForgetsThem)
{
	LandScriptStepper stepper(3);
	stepper.ToggleBreakpoint(1);
	FakeRun run;
	stepper.Step(run.Function());

	stepper.Restart();
	EXPECT_EQ(stepper.Next(), 0u);
	EXPECT_EQ(stepper.StateOf(0), State::NotDone);
	EXPECT_TRUE(stepper.HasBreakpoint(1));

	stepper.ToggleBreakpoint(1);
	EXPECT_FALSE(stepper.HasBreakpoint(1));
	stepper.ToggleBreakpoint(1);
	stepper.Reset(2);
	EXPECT_FALSE(stepper.HasBreakpoint(1));
	EXPECT_EQ(stepper.Count(), 2u);
}
