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
