/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>
#include <cstdlib>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "Server/DedicatedServer.h"
#include "TestTempFolder.h"

using openblack::server::ChecksumLine;
using openblack::server::GameArgumentsFor;
using openblack::server::ParseChecksumLine;
using openblack::server::ServerSettings;

namespace
{

/// The settings of a short run on the mock data of the game
ServerSettings MockSettings(const std::filesystem::path& profileFolder)
{
	return ServerSettings {
	    .gamePath = std::filesystem::path(TEST_BINARY_DIR) / "mock",
	    .land = "Land1.txt",
	    // As the server's command line gives it: the question at the start of the game has an answer
	    .opening = openblack::new_game_choice::ParseNewGameStart("cow"),
	    .seed = 1,
	    .turns = 5,
	    .fast = true,
	    .profileFolder = profileFolder,
	    .logLevel = spdlog::level::warn,
	};
}

} // namespace

TEST(DedicatedServer, TheGameOfTheServerHasNoWindowNoDeviceAndNoAudio)
{
	const auto args = GameArgumentsFor(ServerSettings {.gamePath = "data", .seed = 7, .turns = 100});
	EXPECT_EQ(args.graphicsBackend, openblack::GraphicsBackend::Noop);
	EXPECT_TRUE(args.dedicatedServer);
	EXPECT_TRUE(args.background);
	EXPECT_TRUE(args.skipLogos);
	ASSERT_TRUE(args.seed.has_value());
	EXPECT_EQ(*args.seed, 7u);
	EXPECT_EQ(args.gamePath, "data");
}

TEST(DedicatedServer, EachFrameOfTheServerTakesOneTurnAndWaitsForTheWallClock)
{
	const auto args = GameArgumentsFor(ServerSettings {.turns = 100});
	ASSERT_TRUE(args.simulation.has_value());
	ASSERT_TRUE(args.simulation->frameTime.has_value());
	EXPECT_EQ(*args.simulation->frameTime, std::chrono::milliseconds(100));
	EXPECT_TRUE(args.simulation->settings.paceToWallClock);
	EXPECT_FALSE(args.simulation->settings.fast);
	EXPECT_TRUE(args.simulation->settings.stillHand);
	ASSERT_TRUE(args.simulation->settings.untilTurn.has_value());
	EXPECT_EQ(*args.simulation->settings.untilTurn, 100u);
}

TEST(DedicatedServer, AFastRunDoesNotWaitAndARunWithNoTurnsHasNoEnd)
{
	const auto args = GameArgumentsFor(ServerSettings {.fast = true});
	ASSERT_TRUE(args.simulation.has_value());
	EXPECT_TRUE(args.simulation->settings.fast);
	EXPECT_FALSE(args.simulation->settings.paceToWallClock);
	EXPECT_FALSE(args.simulation->settings.untilTurn.has_value());
}

TEST(DedicatedServer, TheToolsReadTheTurnAndTheChecksumFromTheLineOfTheLog)
{
	const auto line = ChecksumLine(42, 0x0123456789abcdefull);
	EXPECT_EQ(line, "turn 42 checksum 0123456789abcdef");
	const auto read = ParseChecksumLine("[12:00:00] [game] [info] " + line);
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(read->first, 42u);
	EXPECT_EQ(read->second, 0x0123456789abcdefull);
	EXPECT_FALSE(ParseChecksumLine("turn 42 hash 0123").has_value());
	EXPECT_FALSE(ParseChecksumLine("the land loaded").has_value());
}

TEST(DedicatedServer, TheServerLogsTheChecksumOfEachTurnUntilItsLastTurn)
{
	const openblack::test::TestTempFolder profileFolder;
	std::vector<std::pair<uint32_t, uint64_t>> turns;
	const std::atomic<bool> stop {false};
	const auto exitCode =
	    openblack::server::Run(MockSettings(profileFolder.Path()), stop,
	                           [&turns](uint32_t turn, uint64_t checksum) { turns.emplace_back(turn, checksum); });
	EXPECT_EQ(exitCode, EXIT_SUCCESS);
	ASSERT_FALSE(turns.empty());
	for (size_t i = 1; i < turns.size(); ++i)
	{
		EXPECT_EQ(turns[i].first, turns[i - 1].first + 1);
	}
	EXPECT_GE(turns.back().first, 5u);
}

TEST(DedicatedServer, AStopBeforeTheFirstFrameEndsTheServerWithNoTurn)
{
	const openblack::test::TestTempFolder profileFolder;
	uint32_t count = 0;
	const std::atomic<bool> stop {true};
	const auto exitCode = openblack::server::Run(MockSettings(profileFolder.Path()), stop,
	                                             [&count](uint32_t /*turn*/, uint64_t /*checksum*/) { ++count; });
	EXPECT_EQ(exitCode, EXIT_SUCCESS);
	EXPECT_EQ(count, 0u);
}
