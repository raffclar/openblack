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

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <optional>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/Components/Cloud.h"
#include "ECS/Components/Sky.h"
#include "ECS/DrawnRegistry.h"
#include "ECS/Registry.h"
#include "ECS/Systems/SkyClockSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "Graphics/HeadlessRenderer.h"
#include "Locator.h"
#include "Server/DedicatedServer.h"
#include "TestTempFolder.h"

using openblack::server::ChecksumLine;
using openblack::server::k_RoomSeed;
using openblack::server::ParseChecksumLine;
using openblack::server::RoomStart;
using openblack::server::RoomStartFor;
using openblack::server::RoomStartLine;
using openblack::server::ServerConfig;
using openblack::server::ServerSettings;
using openblack::server::SimulationFor;

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

TEST(DedicatedServer, TheServerHasNoWindowNoDeviceAndNoAudio)
{
	const auto config = ServerConfig(ServerSettings {.gamePath = "data", .seed = 7, .turns = 100});
	EXPECT_EQ(config.graphicsBackend, openblack::GraphicsBackend::Noop);
	EXPECT_TRUE(config.dedicatedServer);
	// The resources keep only their data
	EXPECT_EQ(config.resourceUploads, openblack::graphics::UploadMode::DataOnly);
	EXPECT_TRUE(config.background);
	EXPECT_FALSE(config.vsync);
	EXPECT_EQ(config.resolution.x, 0);
	EXPECT_EQ(config.resolution.y, 0);
}

TEST(DedicatedServer, EachFrameOfTheServerTakesOneTurnAndWaitsForTheWallClock)
{
	const auto simulation = SimulationFor(ServerSettings {.turns = 100});
	ASSERT_TRUE(simulation.frameTime.has_value());
	EXPECT_EQ(*simulation.frameTime, std::chrono::milliseconds(100));
	EXPECT_TRUE(simulation.settings.paceToWallClock);
	EXPECT_FALSE(simulation.settings.fast);
	EXPECT_TRUE(simulation.settings.stillHand);
	ASSERT_TRUE(simulation.settings.untilTurn.has_value());
	EXPECT_EQ(*simulation.settings.untilTurn, 100u);
	EXPECT_FALSE(simulation.recordPath.has_value());
}

TEST(DedicatedServer, AFastRunDoesNotWaitAndARunWithNoTurnsHasNoEnd)
{
	const auto simulation = SimulationFor(ServerSettings {.fast = true});
	EXPECT_TRUE(simulation.settings.fast);
	EXPECT_FALSE(simulation.settings.paceToWallClock);
	EXPECT_FALSE(simulation.settings.untilTurn.has_value());
}

TEST(DedicatedServer, ARoomWithNoSeedOfAToolReadsTheDateFromTheClockOfTheComputer)
{
	const auto start = RoomStartFor(ServerSettings {.gamePath = "data"});
	EXPECT_EQ(start, (RoomStart {.seed = k_RoomSeed, .pinnedDate = std::nullopt}));
	EXPECT_EQ(RoomStartLine(start), "The seed of the world: 1, date from the clock of the computer");
}

TEST(DedicatedServer, TheSeedOfAToolAlsoPinsTheDate)
{
	const auto start = RoomStartFor(ServerSettings {.gamePath = "data", .seed = 7});
	EXPECT_EQ(start.seed, 7u);
	EXPECT_EQ(start.pinnedDate, openblack::ecs::systems::TimeSystemInterface::k_DeterministicDate);
	EXPECT_EQ(RoomStartLine(start), "The seed of the world: 7 (from a tool), date pinned");
}

// The commands file of the game has the commands of its testbed scenario. The server drops the same commands of its
// own scenario, so that each command occurs one time only.
TEST(DedicatedServer, AServerThatPlaysTheCommandsOfAFileDropsTheCommandsOfItsOwnPlayer)
{
	using openblack::server::OwnCommands;
	EXPECT_EQ(openblack::server::OwnCommandsOf(ServerSettings {}), OwnCommands::Send);
	EXPECT_EQ(openblack::server::OwnCommandsOf(ServerSettings {.commandsPath = "commands.jsonl"}), OwnCommands::Drop);
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

TEST(DedicatedServer, TheServerWritesNothingInTheProfileFolderItIsGiven)
{
	const openblack::test::TestTempFolder profileFolder;
	const std::atomic<bool> stop {false};
	const auto exitCode =
	    openblack::server::Run(MockSettings(profileFolder.Path()), stop, [](uint32_t /*turn*/, uint64_t /*checksum*/) {});
	EXPECT_EQ(exitCode, EXIT_SUCCESS);
	std::vector<std::filesystem::path> files;
	for (const auto& entry : std::filesystem::recursive_directory_iterator(profileFolder.Path()))
	{
		files.push_back(entry.path());
	}
	EXPECT_TRUE(files.empty());
}

TEST(DedicatedServer, TheServerDoesNotStartTheGraphicsLibrary)
{
	// A call to the library that did not start stops this test program, so the run also shows that no part calls it
	ASSERT_FALSE(openblack::graphics::GraphicsLibraryStarted());
	const openblack::test::TestTempFolder profileFolder;
	std::vector<bool> started;
	const std::atomic<bool> stop {false};
	const auto exitCode =
	    openblack::server::Run(MockSettings(profileFolder.Path()), stop, [&started](uint32_t /*turn*/, uint64_t /*checksum*/) {
		    started.push_back(openblack::graphics::GraphicsLibraryStarted());
	    });
	EXPECT_EQ(exitCode, EXIT_SUCCESS);
	ASSERT_FALSE(started.empty());
	EXPECT_TRUE(std::ranges::none_of(started, [](bool value) { return value; }));
	EXPECT_FALSE(openblack::graphics::GraphicsLibraryStarted());
}

TEST(DedicatedServer, TheServerKeepsTheSkysCloudsOutOfTheWorldAndMakesNoSystemThatOnlyShowsTheWorld)
{
	const openblack::test::TestTempFolder profileFolder;
	bool shown = false;
	bool mists = true;
	size_t worldClouds = 1;
	size_t drawnClouds = 0;
	const std::atomic<bool> stop {false};
	const auto exitCode =
	    openblack::server::Run(MockSettings(profileFolder.Path()), stop,
	                           [&shown, &mists, &worldClouds, &drawnClouds](uint32_t /*turn*/, uint64_t /*checksum*/) {
		                           using openblack::Locator;
		                           using openblack::ecs::components::Cloud;
		                           shown = shown || Locator::oceanSystem::has_value() || Locator::cloudSystem::has_value() ||
		                                   Locator::villageLightSystem::has_value() || Locator::advisorVoiceSystem::has_value() ||
		                                   Locator::turnCheerSystem::has_value();
		                           // The scripts fade the mists, and the turns change their size on every computer
		                           mists = mists && Locator::mistSystem::has_value();
		                           // Every computer draws the numbers of the sky's clouds at the start of the land, but
		                           // the clouds are things of the drawn registry and take no entity numbers of the world
		                           worldClouds = std::as_const(Locator::entitiesRegistry::value()).Size<const Cloud>();
		                           drawnClouds = std::as_const(Locator::drawnRegistry::value()).Size<const Cloud>();
	                           });
	EXPECT_EQ(exitCode, EXIT_SUCCESS);
	EXPECT_FALSE(shown);
	EXPECT_TRUE(mists);
	EXPECT_EQ(worldClouds, 0u);
	EXPECT_GT(drawnClouds, 0u);
}

TEST(DedicatedServer, TheServerStepsTheRulesOfTheParticlesButDoesNotDrawThem)
{
	const openblack::test::TestTempFolder profileFolder;
	bool rules = true;
	bool drawn = false;
	const std::atomic<bool> stop {false};
	const auto exitCode = openblack::server::Run(MockSettings(profileFolder.Path()), stop,
	                                             [&rules, &drawn](uint32_t /*turn*/, uint64_t /*checksum*/) {
		                                             using openblack::Locator;
		                                             rules = rules && Locator::particleRuleSystem::has_value();
		                                             drawn = drawn || Locator::particleSystem::has_value();
	                                             });
	EXPECT_EQ(exitCode, EXIT_SUCCESS);
	EXPECT_TRUE(rules);
	EXPECT_FALSE(drawn);
}

TEST(DedicatedServer, TheServerMovesTheSkysClockButDoesNotShowTheSky)
{
	const openblack::test::TestTempFolder profileFolder;
	bool shown = false;
	bool clock = true;
	size_t turns = 0;
	const std::atomic<bool> stop {false};
	const auto exitCode = openblack::server::Run(
	    MockSettings(profileFolder.Path()), stop, [&shown, &clock, &turns](uint32_t /*turn*/, uint64_t /*checksum*/) {
		    using openblack::Locator;
		    ++turns;
		    shown = shown || Locator::skySystem::has_value();
		    // The clock of day and night, on the dome's entity, is part of the world on every computer
		    clock = clock && Locator::skyClockSystem::has_value() &&
		            std::as_const(Locator::entitiesRegistry::value())
		                .AllOf<openblack::ecs::components::DayNightCycle>(Locator::skyClockSystem::value().GetEntities().dome);
	    });
	EXPECT_EQ(exitCode, EXIT_SUCCESS);
	EXPECT_GT(turns, 0u);
	EXPECT_FALSE(shown);
	EXPECT_TRUE(clock);
}

// The scripts fade the mists and the turns step the fades, thus the server has the mist system too
TEST(DedicatedServer, TheServerHasTheMistSystemThatTheScriptsAndTheTurnsUse)
{
	const openblack::test::TestTempFolder profileFolder;
	bool mists = true;
	const std::atomic<bool> stop {false};
	const auto exitCode =
	    openblack::server::Run(MockSettings(profileFolder.Path()), stop, [&mists](uint32_t /*turn*/, uint64_t /*checksum*/) {
		    mists = mists && openblack::Locator::mistSystem::has_value();
	    });
	EXPECT_EQ(exitCode, EXIT_SUCCESS);
	EXPECT_TRUE(mists);
}
