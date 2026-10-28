/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <filesystem>

#include <gtest/gtest.h>

#include "Simulation/CommandLog.h"

using namespace openblack;

TEST(CommandLog, AGivenCommandOfAScenarioReadsBackFromItsLine)
{
	const auto line = command_log::GiveLineOf(42, 7);
	EXPECT_EQ(line, "{\"turn\":42,\"give\":7}");
	EXPECT_EQ(command_log::ReadGiveLine(line), (command_log::Give {.turn = 42, .command = 7}));
}

TEST(CommandLog, ALineWithNoGivenCommandDoesNotRead)
{
	EXPECT_FALSE(command_log::ReadGiveLine("{\"turn\":3,\"frame\":\"00\"}").has_value());
	EXPECT_FALSE(command_log::ReadGiveLine("{\"turn\":3,\"give\":}").has_value());
	EXPECT_FALSE(command_log::ReadGiveLine("").has_value());
}

TEST(CommandLog, TheGivenCommandsAreBesideTheCommandsFile)
{
	const auto gives = command_log::GivesPathOf(std::filesystem::path("run") / "game" / "commands.jsonl");
	EXPECT_EQ(gives, std::filesystem::path("run") / "game" / "commands.gives.jsonl");
}
