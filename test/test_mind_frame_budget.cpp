/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>
#include <cstdint>

#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "PlayerTurnOrder.h"
#include "Simulation/MindFrameBudget.h"

using openblack::simulation::MindFrameBudget;
namespace player_turn = openblack::player_turn;

TEST(MindFrameBudget, AFrameWithNoBudgetNeverStopsATurn)
{
	MindFrameBudget budget;
	budget.StartFrame(std::nullopt);
	budget.AddMindTime(1000.0f);
	EXPECT_FALSE(budget.HasBudget());
	EXPECT_FALSE(budget.Used());
}

TEST(MindFrameBudget, TheBudgetIsUsedWhenTheMindsTookAllOfIt)
{
	MindFrameBudget budget;
	budget.StartFrame(8.0f);
	EXPECT_TRUE(budget.HasBudget());
	budget.AddMindTime(5.0f);
	EXPECT_FALSE(budget.Used());
	budget.AddMindTime(2.5f);
	EXPECT_FALSE(budget.Used());
	budget.AddMindTime(0.5f);
	EXPECT_TRUE(budget.Used());
	EXPECT_FLOAT_EQ(budget.UsedMs(), 8.0f);
}

TEST(MindFrameBudget, EachFrameStartsWithAllOfItsBudget)
{
	MindFrameBudget budget;
	budget.StartFrame(1.0f);
	budget.AddMindTime(3.0f);
	ASSERT_TRUE(budget.Used());
	budget.StartFrame(1.0f);
	EXPECT_FALSE(budget.Used());
	EXPECT_FLOAT_EQ(budget.UsedMs(), 0.0f);
}

TEST(MindFrameBudget, AGameWithNoTimeGivenHasTheDefaultBudget)
{
	EXPECT_EQ(openblack::simulation::FrameBudgetMs(std::nullopt, false),
	          std::optional(openblack::simulation::k_DefaultMindFrameBudgetMs));
}

// The budget follows the machine's clock. Two runs with a fixed frame time must draw the same frames.
TEST(MindFrameBudget, ARunWithAFixedFrameTimeHasNoBudgetWhenNoTimeIsGiven)
{
	EXPECT_EQ(openblack::simulation::FrameBudgetMs(std::nullopt, true), std::nullopt);
}

TEST(MindFrameBudget, TheGivenTimeIsTheBudgetAlsoWithAFixedFrameTime)
{
	EXPECT_EQ(openblack::simulation::FrameBudgetMs(5.0f, false), std::optional(5.0f));
	EXPECT_EQ(openblack::simulation::FrameBudgetMs(5.0f, true), std::optional(5.0f));
}

TEST(MindFrameBudget, AGivenTimeOfZeroGivesNoBudget)
{
	EXPECT_EQ(openblack::simulation::FrameBudgetMs(0.0f, false), std::nullopt);
	EXPECT_EQ(openblack::simulation::FrameBudgetMs(0.0f, true), std::nullopt);
}

// The players' turns with minds of a fixed time, frame by frame: each frame does the players until their minds used the
// budget, and the next frame continues with the next player. All the players do their turns once, in their order.
TEST(MindFrameBudget, TheFramesShareThePlayersOfALongTurn)
{
	const std::vector<uint32_t> players {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
	constexpr float k_MindMs = 3.0f;
	MindFrameBudget budget;
	std::vector<uint32_t> minds;
	std::vector<std::vector<uint32_t>> frames;
	std::optional<size_t> next = 0;
	while (next.has_value())
	{
		budget.StartFrame(8.0f);
		frames.emplace_back();
		next = player_turn::PlayFrom(
		    players, *next,
		    [&](uint32_t player, player_turn::Part part) {
			    if (part == player_turn::Part::Mind)
			    {
				    minds.push_back(player);
				    frames.back().push_back(player);
				    budget.AddMindTime(k_MindMs);
			    }
		    },
		    [&budget] { return budget.Used(); });
	}
	EXPECT_EQ(minds, players);
	const std::vector<std::vector<uint32_t>> expected {{0, 1, 2}, {3, 4, 5}, {6, 7, 8}, {9}};
	EXPECT_EQ(frames, expected);
}

namespace
{

/// The players of a turn frame by frame, each mind of a fixed time, with the budget's room for the steps after the
/// players. Each list is the players of one frame.
std::vector<std::vector<uint32_t>> FramesOfTurn(size_t playerCount, float mindMs, float budgetMs, float afterPlayersMs)
{
	std::vector<uint32_t> players(playerCount);
	for (size_t i = 0; i < playerCount; ++i)
	{
		players[i] = static_cast<uint32_t>(i);
	}
	MindFrameBudget budget;
	budget.NoteStepsAfterPlayers(afterPlayersMs);
	std::vector<std::vector<uint32_t>> frames;
	std::optional<size_t> next = 0;
	uint32_t frame = 0;
	while (next.has_value())
	{
		budget.StartFrame(budgetMs);
		budget.SetFrameOfTurn(++frame);
		frames.emplace_back();
		auto played = *next;
		next = player_turn::PlayFrom(
		    players, *next,
		    [&](uint32_t player, player_turn::Part part) {
			    if (part == player_turn::Part::Mind)
			    {
				    frames.back().push_back(player);
			    }
		    },
		    [&] {
			    budget.AddMindTime(mindMs);
			    ++played;
			    return budget.StopsBefore(playerCount - played);
		    });
	}
	return frames;
}

} // namespace

// The minds of all players fit in the budget, and the steps after the players fit too: the turn does not stop
TEST(MindFrameBudget, ATurnWithRoomForTheStepsAfterThePlayersDoesNotStop)
{
	const std::vector<std::vector<uint32_t>> expected {{0, 1, 2, 3}};
	EXPECT_EQ(FramesOfTurn(4, 0.5f, 8.0f, 4.0f), expected);
}

// The minds of all players fit in the budget, but the steps after them do not. The turn stops when the steps after the
// players have no more room, and the next frame ends it.
TEST(MindFrameBudget, TheBudgetKeepsRoomForTheStepsAfterThePlayers)
{
	const std::vector<std::vector<uint32_t>> expected {{0, 1, 2, 3}, {4, 5}};
	EXPECT_EQ(FramesOfTurn(6, 1.0f, 8.0f, 4.0f), expected);
}

// A long turn uses all of the budget in its first frames. The frame that ends the turn keeps room for the steps after the
// players.
TEST(MindFrameBudget, TheFrameThatEndsALongTurnKeepsRoomForTheStepsAfterThePlayers)
{
	// Without the room, the second frame does players 8 to 14: 7 ms of minds and 4.5 ms of steps after the players
	const std::vector<std::vector<uint32_t>> expected {{0, 1, 2, 3, 4, 5, 6, 7}, {8, 9, 10, 11}, {12, 13, 14}};
	EXPECT_EQ(FramesOfTurn(15, 1.0f, 8.0f, 4.5f), expected);
}

// After the frames with room, a frame uses all of the budget. Thus the room adds no more than one frame to a turn.
TEST(MindFrameBudget, TheBudgetKeepsRoomOnlyInTheFirstFramesOfATurn)
{
	const std::vector<std::vector<uint32_t>> expected {{0}, {1}, {2, 3}};
	EXPECT_EQ(FramesOfTurn(4, 1.0f, 8.0f, 7.5f), expected);
}

// Steps after the players that need all of the budget get no room: the budget then stops the turn as before
TEST(MindFrameBudget, StepsAfterThePlayersThatNeedAllOfTheBudgetGetNoRoom)
{
	const std::vector<std::vector<uint32_t>> expected {{0, 1, 2}, {3, 4, 5}, {6, 7, 8}, {9}};
	EXPECT_EQ(FramesOfTurn(10, 3.0f, 8.0f, 9.0f), expected);
}

TEST(MindFrameBudget, AFrameWithNoBudgetNeverStopsBeforeAPlayer)
{
	MindFrameBudget budget;
	budget.NoteStepsAfterPlayers(5.0f);
	budget.StartFrame(std::nullopt);
	budget.AddMindTime(1000.0f);
	EXPECT_FALSE(budget.StopsBefore(10));
}
