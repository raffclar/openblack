/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>

#include <entt/entity/registry.hpp>
#include <entt/meta/context.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/PuzzleGame.h"
#include "ECS/Reflection/ComponentReflection.h"
#include "ECS/Reflection/WorldArchive.h"
#include "ECS/TreePuzzleRules.h"

using openblack::TreeInfo;
using openblack::script::PuzzleGameType;
namespace tree_puzzle = openblack::ecs::tree_puzzle;

namespace
{
using Counts = std::array<int8_t, tree_puzzle::k_Spots>;

/// The counts after a pull of a spot, as the puzzle changes them
Counts AfterPull(Counts counts, int32_t across, int32_t along)
{
	for (const auto& [changedAcross, changedAlong] : tree_puzzle::PulledSpots(across, along))
	{
		if (tree_puzzle::OnGrid(changedAcross, changedAlong))
		{
			auto& count = counts.at(tree_puzzle::SpotIndex(changedAcross, changedAlong));
			count = tree_puzzle::Changed(count);
		}
	}
	return counts;
}
} // namespace

// The first tree puzzle has two states, the other has three
TEST(TreePuzzleRules, TheNumberOfStates)
{
	EXPECT_EQ(tree_puzzle::StatesOf(PuzzleGameType::Tree1), 2);
	EXPECT_EQ(tree_puzzle::StatesOf(PuzzleGameType::Tree2), 3);
}

// Each state shows as a kind of tree: a conifer, an oak, then a birch
TEST(TreePuzzleRules, TheKindOfTreeOfEachState)
{
	EXPECT_EQ(tree_puzzle::TreeKindOf(PuzzleGameType::Tree1, 0), TreeInfo::Conifer);
	EXPECT_EQ(tree_puzzle::TreeKindOf(PuzzleGameType::Tree1, 1), TreeInfo::Oak);
	EXPECT_EQ(tree_puzzle::TreeKindOf(PuzzleGameType::Tree1, 2), TreeInfo::Conifer);
	EXPECT_EQ(tree_puzzle::TreeKindOf(PuzzleGameType::Tree2, 2), TreeInfo::Birch);
	EXPECT_EQ(tree_puzzle::TreeKindOf(PuzzleGameType::Tree2, 3), TreeInfo::Conifer);
	EXPECT_EQ(tree_puzzle::FullSizeOf(PuzzleGameType::Tree2, 2), 1.0f);
}

// The count of changes wraps as a signed byte. An odd negative count gives a beech that has no full size.
TEST(TreePuzzleRules, ACountPastTheByteGivesABeech)
{
	EXPECT_EQ(tree_puzzle::Changed(127), -128);
	EXPECT_EQ(tree_puzzle::TreeKindOf(PuzzleGameType::Tree1, -128), TreeInfo::Conifer);
	EXPECT_EQ(tree_puzzle::FullSizeOf(PuzzleGameType::Tree1, -128), 1.0f);
	EXPECT_EQ(tree_puzzle::TreeKindOf(PuzzleGameType::Tree1, -127), TreeInfo::Beech);
	EXPECT_EQ(tree_puzzle::FullSizeOf(PuzzleGameType::Tree1, -127), 0.0f);
	EXPECT_EQ(tree_puzzle::TreeKindOf(PuzzleGameType::Tree2, -2), TreeInfo::Beech);
}

// A pull changes the spot and the spots next to it on the grid, not the corners next to it
TEST(TreePuzzleRules, APullChangesTheSpotAndItsNeighbours)
{
	const auto middle = AfterPull({}, 1, 1);
	EXPECT_EQ(middle, (Counts {0, 1, 0, 1, 1, 1, 0, 1, 0}));
	const auto corner = AfterPull({}, 0, 0);
	EXPECT_EQ(corner, (Counts {1, 1, 0, 1, 0, 0, 0, 0, 0}));
	const auto side = AfterPull({}, 2, 1);
	EXPECT_EQ(side, (Counts {0, 0, 0, 0, 1, 0, 1, 1, 1}));
}

// The pull changes the spots in a fixed order, and the spot itself last
TEST(TreePuzzleRules, TheOrderOfThePulledSpots)
{
	const auto spots = tree_puzzle::PulledSpots(1, 2);
	EXPECT_EQ(spots.at(0), (std::array<int32_t, 2> {2, 2}));
	EXPECT_EQ(spots.at(1), (std::array<int32_t, 2> {0, 2}));
	EXPECT_EQ(spots.at(2), (std::array<int32_t, 2> {1, 3}));
	EXPECT_EQ(spots.at(3), (std::array<int32_t, 2> {1, 1}));
	EXPECT_EQ(spots.at(4), (std::array<int32_t, 2> {1, 2}));
	EXPECT_FALSE(tree_puzzle::OnGrid(1, 3));
	EXPECT_FALSE(tree_puzzle::OnGrid(-1, 0));
}

// The puzzle shows a solution when all spots show the state of the first spot
TEST(TreePuzzleRules, AllSpotsInTheSameState)
{
	EXPECT_TRUE(tree_puzzle::SameState(PuzzleGameType::Tree1, Counts {}));
	EXPECT_TRUE(tree_puzzle::SameState(PuzzleGameType::Tree1, Counts {1, 3, 5, 1, 1, 1, 1, 1, 7}));
	EXPECT_FALSE(tree_puzzle::SameState(PuzzleGameType::Tree1, Counts {1, 2, 1, 1, 1, 1, 1, 1, 1}));
	EXPECT_TRUE(tree_puzzle::SameState(PuzzleGameType::Tree2, Counts {0, 3, 6, 0, 0, 0, 0, 0, 3}));
	EXPECT_FALSE(tree_puzzle::SameState(PuzzleGameType::Tree2, Counts {0, 2, 0, 0, 0, 0, 0, 0, 0}));
	// A negative count gives a negative remainder, which is not the state of a positive count
	EXPECT_FALSE(tree_puzzle::SameState(PuzzleGameType::Tree1, Counts {1, -127, 1, 1, 1, 1, 1, 1, 1}));
}

// A pull of all nine spots, each once, puts a two state puzzle in its first state again
TEST(TreePuzzleRules, NinePullsChangeEachSpotAnOddOrEvenNumberOfTimes)
{
	Counts counts {};
	for (int32_t across = 0; across < tree_puzzle::k_Side; ++across)
	{
		for (int32_t along = 0; along < tree_puzzle::k_Side; ++along)
		{
			counts = AfterPull(counts, across, along);
		}
	}
	// A corner changes three times, a side four times and the middle five times
	EXPECT_EQ(counts, (Counts {3, 4, 3, 4, 5, 4, 3, 4, 3}));
	EXPECT_FALSE(tree_puzzle::SameState(PuzzleGameType::Tree1, counts));
}

// A tree grows by a twentieth each turn, up to its full size
TEST(TreePuzzleRules, ATreeGrowsByAStepUpToItsFullSize)
{
	EXPECT_EQ(tree_puzzle::Grown(tree_puzzle::k_StartSize, 1.0f), 0.1f + 0.05f);
	EXPECT_EQ(tree_puzzle::Grown(0.98f, 1.0f), 1.0f);
	float size = tree_puzzle::k_StartSize;
	int turns = 0;
	while (size < 1.0f)
	{
		size = tree_puzzle::Grown(size, 1.0f);
		++turns;
	}
	EXPECT_EQ(size, 1.0f);
	// About eighteen turns, as each float sum rounds
	EXPECT_GE(turns, 18);
	EXPECT_LE(turns, 19);
}

// A save keeps the tree puzzle's counts and its things
TEST(TreePuzzleRules, ASaveKeepsTheTreePuzzle)
{
	using openblack::ecs::components::PuzzleGame;
	namespace reflection = openblack::ecs::reflection;
	entt::meta_ctx context;
	reflection::RegisterComponents(context);
	entt::registry registry;
	const auto puzzle = registry.create();
	const auto tree = registry.create();
	const auto mist = registry.create();
	const auto bat = registry.create();
	const auto lantern = registry.create();
	PuzzleGame game {.type = PuzzleGameType::Tree1, .started = true};
	game.treeCounts = {1, 0, -127, 0, 2, 0, 0, 0, 5};
	game.trees.at(4) = tree;
	game.treesPlacedAt.at(4) = {1.0f, 2.0f, 3.0f};
	game.mists.at(2) = mist;
	game.bats.at(8) = bat;
	game.lanterns.at(3) = lantern;
	registry.emplace<PuzzleGame>(puzzle, game);

	reflection::WorldSaveReport report;
	const auto bytes = reflection::SaveWorld(registry, context, &report);
	EXPECT_TRUE(report.unsavedValues.empty());
	entt::registry loaded;
	ASSERT_TRUE(reflection::LoadWorld(loaded, context, bytes, reflection::WorldLoadMode::Exact).has_value());
	const auto& loadedGame = loaded.get<PuzzleGame>(puzzle);
	EXPECT_EQ(loadedGame.treeCounts, game.treeCounts);
	EXPECT_EQ(loadedGame.trees.at(4), tree);
	EXPECT_EQ(loadedGame.trees.at(0), entt::entity {entt::null});
	EXPECT_EQ(loadedGame.treesPlacedAt.at(4).y, 2.0f);
	EXPECT_EQ(loadedGame.mists.at(2), mist);
	EXPECT_EQ(loadedGame.bats.at(8), bat);
	EXPECT_EQ(loadedGame.lanterns.at(3), lantern);
}
