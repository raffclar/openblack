/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <filesystem>
#include <vector>

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

TEST(CommandLog, AGivenCommandOnALaterLandKeepsItsLand)
{
	const auto line = command_log::GiveLineOf(10, 1, 2);
	EXPECT_EQ(line, "{\"turn\":10,\"land\":2,\"give\":1}");
	EXPECT_EQ(command_log::ReadGiveLine(line), (command_log::Give {.turn = 10, .command = 1, .land = 2}));
}

TEST(CommandLog, ALoadStartsTheNextLandOfTheCounter)
{
	command_log::LandCounter lands;
	EXPECT_EQ(lands.Note(1), 0u);
	EXPECT_EQ(lands.Note(9), 0u);
	// The same turn again is not a load: a give and the commands of one turn note it
	EXPECT_EQ(lands.Note(9), 0u);
	EXPECT_EQ(lands.Note(10), 0u);
	EXPECT_EQ(lands.Note(1), 1u);
	EXPECT_EQ(lands.Note(50), 1u);
	EXPECT_EQ(lands.Note(0), 2u);
	EXPECT_EQ(lands.GetLand(), 2u);
}

TEST(CommandLog, TheCommandsOfEachLandReadApart)
{
	const auto folder = std::filesystem::temp_directory_path() / "openblack_test_command_log_lands";
	std::filesystem::create_directories(folder);
	const auto path = folder / "commands.jsonl";
	const std::vector<player_commands::Command> commands {
	    {.player = PlayerNames::PLAYER_ONE, .action = player_commands::HandTake {.object = static_cast<entt::entity>(5)}}};
	{
		command_log::Writer writer(path);
		writer.Write(3, commands);
		writer.Write(9, commands);
		writer.Write(3, commands, 1);
	}
	const auto lands = command_log::ReadLandsFile(path);
	ASSERT_TRUE(lands.has_value());
	ASSERT_EQ(lands->size(), 2u);
	EXPECT_EQ((*lands)[0].size(), 2u);
	ASSERT_EQ((*lands)[1].size(), 1u);
	EXPECT_EQ((*lands)[1].at(3).size(), 1u);
	std::filesystem::remove_all(folder);
}

TEST(CommandLog, TheGivenCommandsAreBesideTheCommandsFile)
{
	const auto gives = command_log::GivesPathOf(std::filesystem::path("run") / "game" / "commands.jsonl");
	EXPECT_EQ(gives, std::filesystem::path("run") / "game" / "commands.gives.jsonl");
}
