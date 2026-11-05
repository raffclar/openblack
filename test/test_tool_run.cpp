/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "Debug/GuardedFrameStep.h"
#include "Debug/SimRun.h"
#include "ECS/Systems/Implementations/ToolRunSystem.h"
#include "GameFrameOrder.h"
#include "Simulation/CommandLog.h"
#include "Temple/TempleFrameWrites.h"

using namespace openblack;

namespace
{

/// A guard that records the calls that it gets, in order
struct FakeGuard
{
	struct Call
	{
		std::string what;
		std::string step;
		uint32_t turn {0};
		bool networkGame {false};
		bool writers {false};

		bool operator==(const Call&) const = default;
	};

	void Before() { calls.push_back({.what = "Before"}); }
	void Forget() { calls.push_back({.what = "Forget"}); }
	void After(std::string_view step, uint32_t turn, const temple::FrameWrites& writers, bool networkGame)
	{
		calls.push_back({.what = "After",
		                 .step = std::string(step),
		                 .turn = turn,
		                 .networkGame = networkGame,
		                 .writers = !writers.Empty()});
	}
	void AfterTool(std::string_view step, uint32_t turn)
	{
		calls.push_back({.what = "AfterTool", .step = std::string(step), .turn = turn});
	}

	std::vector<Call> calls;
};

/// A world with a turn, the temple's frame writes and a count of land loads that a test sets
struct FakeWorld
{
	[[nodiscard]] uint32_t Turn() const { return turn; }
	[[nodiscard]] temple::FrameWrites TakeFrameWrites()
	{
		++takes;
		return std::exchange(writes, {});
	}
	[[nodiscard]] uint32_t LandLoads() const { return landLoads; }
	[[nodiscard]] bool IsNetworkGame() const { return networkGame; }

	uint32_t turn {7};
	temple::FrameWrites writes;
	uint32_t landLoads {0};
	bool networkGame {false};
	uint32_t takes {0};
};

std::vector<FakeGuard::Call> Watched(game_frame::Step step, uint32_t turn, bool network = false)
{
	if (step == game_frame::Step::DebugGui)
	{
		return {{.what = "Before"}, {.what = "AfterTool", .step = std::string(game_frame::Name(step)), .turn = turn}};
	}
	return {{.what = "Before"},
	        {.what = "After", .step = std::string(game_frame::Name(step)), .turn = turn, .networkGame = network}};
}

} // namespace

TEST(GuardedFrameStep, TheGuardSeesEachStepButTheTurns)
{
	FakeGuard guard;
	FakeWorld world;
	std::vector<game_frame::Step> played;
	for (const auto& place : game_frame::k_Frame)
	{
		guard.calls.clear();
		const bool result = frame_write_guard::PlayGuardedStep(&guard, place.step, 0, world, [&] {
			played.push_back(place.step);
			return true;
		});
		EXPECT_TRUE(result);
		if (place.step == game_frame::Step::Turns)
		{
			EXPECT_EQ(guard.calls, (std::vector<FakeGuard::Call> {{.what = "Forget"}}));
		}
		else
		{
			EXPECT_EQ(guard.calls, Watched(place.step, world.turn)) << game_frame::Name(place.step);
		}
	}
	ASSERT_EQ(played.size(), game_frame::k_Frame.size());
	for (size_t i = 0; i < played.size(); ++i)
	{
		EXPECT_EQ(played[i], game_frame::k_Frame.at(i).step);
	}
}

TEST(GuardedFrameStep, WithNoGuardTheStepOnlyPlays)
{
	FakeWorld world;
	world.writes.Note(temple::FrameWrite::Entry);
	int plays = 0;
	const bool result = frame_write_guard::PlayGuardedStep<FakeGuard>(nullptr, game_frame::Step::Hand, 0, world, [&] {
		++plays;
		return false;
	});
	EXPECT_FALSE(result);
	EXPECT_EQ(plays, 1);
	// The frame writes stay for the temple's next reader
	EXPECT_EQ(world.takes, 0U);
	EXPECT_FALSE(world.writes.Empty());
}

TEST(GuardedFrameStep, TheGuardDropsTheFrameWritesOfTheStepsThatItDoesNotWatch)
{
	FakeGuard guard;
	FakeWorld world;
	world.writes.Note(temple::FrameWrite::Entry);
	std::ignore = frame_write_guard::PlayGuardedStep(&guard, game_frame::Step::Turns, 0, world, [] { return true; });
	EXPECT_EQ(world.takes, 1U);
	EXPECT_TRUE(world.writes.Empty());
}

TEST(GuardedFrameStep, TheGuardWaitsForItsFirstTurn)
{
	FakeGuard guard;
	FakeWorld world;
	world.turn = 9;
	std::ignore = frame_write_guard::PlayGuardedStep(&guard, game_frame::Step::Hand, 10, world, [] { return true; });
	EXPECT_EQ(guard.calls, (std::vector<FakeGuard::Call> {{.what = "Forget"}}));
	guard.calls.clear();
	world.turn = 10;
	std::ignore = frame_write_guard::PlayGuardedStep(&guard, game_frame::Step::Hand, 10, world, [] { return true; });
	EXPECT_EQ(guard.calls, Watched(game_frame::Step::Hand, 10));
}

TEST(GuardedFrameStep, TheGuardGivesTheTempleWritersAndTheNetworkGame)
{
	FakeGuard guard;
	FakeWorld world;
	world.networkGame = true;
	std::ignore = frame_write_guard::PlayGuardedStep(&guard, game_frame::Step::Temple, 0, world, [&] {
		world.writes.Note(temple::FrameWrite::Exit);
		return true;
	});
	ASSERT_EQ(guard.calls.size(), 2U);
	EXPECT_EQ(guard.calls[1].what, "After");
	EXPECT_TRUE(guard.calls[1].writers);
	EXPECT_TRUE(guard.calls[1].networkGame);
}

TEST(GuardedFrameStep, AStepThatLoadsALandIsNotWatched)
{
	FakeGuard guard;
	FakeWorld world;
	std::ignore = frame_write_guard::PlayGuardedStep(&guard, game_frame::Step::Interface, 0, world, [&] {
		++world.landLoads;
		return true;
	});
	EXPECT_EQ(guard.calls, (std::vector<FakeGuard::Call> {{.what = "Before"}, {.what = "Forget"}}));
}

TEST(ToolRunSystem, WithNoToolRunEveryStepOnlyPlays)
{
	const std::optional<sim_run::Request> noRun;
	ecs::systems::ToolRunSystem toolRun(noRun, std::nullopt, std::nullopt);
	std::vector<game_frame::Step> played;
	for (const auto& place : game_frame::k_Frame)
	{
		EXPECT_TRUE(toolRun.PlayFrameStep(place.step, [&] {
			played.push_back(place.step);
			return true;
		}));
	}
	EXPECT_EQ(played.size(), game_frame::k_Frame.size());
	EXPECT_FALSE(toolRun.PlayFrameStep(game_frame::Step::Hand, [] { return false; }));
	// No log is open, so a give of the testbed scenario goes nowhere
	toolRun.RecordScenarioGive(3, 1);
	const auto state = toolRun.GetState();
	EXPECT_FALSE(state.commandLog);
	EXPECT_FALSE(state.scenarioGives);
	EXPECT_FALSE(state.checksumLog);
	EXPECT_FALSE(state.frameGuard);
	EXPECT_FALSE(state.fightAutopilot);
}

TEST(ToolRunSystem, TheTestLeaveHappensOnce)
{
	const std::optional<sim_run::Request> noRun;
	ecs::systems::ToolRunSystem toolRun(noRun, FightAutopilotRequest {.policy = "still"}, std::nullopt);
	EXPECT_TRUE(toolRun.GetState().fightAutopilot);
	EXPECT_FALSE(toolRun.IsTestLeaveDone());
	toolRun.SetTestLeaveDone();
	EXPECT_TRUE(toolRun.IsTestLeaveDone());
	EXPECT_TRUE(toolRun.GetState().testLeaveDone);
}

TEST(ToolRunSystem, TheGivesWriterGetsEachTurnAndCommand)
{
	const auto folder = std::filesystem::temp_directory_path() / "openblack_test_tool_run";
	std::filesystem::create_directories(folder);
	const auto path = command_log::GivesPathOf(folder / "commands.jsonl");
	{
		command_log::GivesWriter gives(path);
		gives.Write(3, 0);
		gives.Write(3, 2);
		gives.Write(5, 1);
	}
	const auto read = command_log::ReadGivesFile(path);
	EXPECT_EQ(read, (std::map<uint32_t, std::vector<size_t>> {{3, {0, 2}}, {5, {1}}}));
	std::filesystem::remove_all(folder);
}
