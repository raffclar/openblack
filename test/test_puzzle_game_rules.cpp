/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <vector>

#include <gtest/gtest.h>

#include "ECS/PuzzleGameRules.h"

using openblack::ecs::puzzle_game::LetsGoOfThingsThatWent;
using openblack::ecs::puzzle_game::PlayedBeforeSolved;
using openblack::ecs::puzzle_game::StateBeforeStart;
using openblack::script::PuzzleGameStatus;
using openblack::script::PuzzleGameType;

// The two puzzles of the second land have not played until the player solves them
TEST(PuzzleGameRules, TheSecondLandsPuzzlesStartUnsolved)
{
	EXPECT_EQ(PlayedBeforeSolved(PuzzleGameType::Tree1), false);
	EXPECT_EQ(PlayedBeforeSolved(PuzzleGameType::Tree2), false);
	EXPECT_EQ(PlayedBeforeSolved(PuzzleGameType::Hanoi), false);
}

// Some kinds always give the same answer, whatever their pieces show
TEST(PuzzleGameRules, SomeKindsAlwaysGiveTheSameAnswer)
{
	EXPECT_EQ(PlayedBeforeSolved(PuzzleGameType::None), true);
	EXPECT_EQ(PlayedBeforeSolved(PuzzleGameType::Maze1), true);
	EXPECT_EQ(PlayedBeforeSolved(PuzzleGameType::Maze2), true);
	EXPECT_EQ(PlayedBeforeSolved(PuzzleGameType::Fishes2), true);
	EXPECT_EQ(PlayedBeforeSolved(PuzzleGameType::Last), true);
	EXPECT_EQ(PlayedBeforeSolved(PuzzleGameType::Chess), false);
}

// The mushroom game starts in progress and gives no. The lions and sheep game starts with no side to move and gives yes.
TEST(PuzzleGameRules, TheMushroomAndLionsAndSheepGamesGiveTheAnswerOfTheirStart)
{
	EXPECT_EQ(PlayedBeforeSolved(PuzzleGameType::Immersion), false);
	EXPECT_EQ(PlayedBeforeSolved(PuzzleGameType::Lionsheep), true);
	EXPECT_EQ(StateBeforeStart(PlayedBeforeSolved(PuzzleGameType::Immersion)), PuzzleGameStatus::GameNotYetBegun);
	EXPECT_EQ(StateBeforeStart(PlayedBeforeSolved(PuzzleGameType::Lionsheep)), PuzzleGameStatus::Won);
}

// A puzzle game that nobody started is not yet begun, or won when it always gives yes
TEST(PuzzleGameRules, TheStateBeforeTheStart)
{
	EXPECT_EQ(StateBeforeStart(false), PuzzleGameStatus::GameNotYetBegun);
	EXPECT_EQ(StateBeforeStart(true), PuzzleGameStatus::Won);
}

// Each kind but the beach temple puzzle lets go of its things that went. The beach temple puzzle makes its pieces again.
TEST(PuzzleGameRules, AllKindsButTheBeachTempleLetGoOfThingsThatWent)
{
	EXPECT_TRUE(LetsGoOfThingsThatWent(PuzzleGameType::Tree1));
	EXPECT_TRUE(LetsGoOfThingsThatWent(PuzzleGameType::Tree2));
	EXPECT_TRUE(LetsGoOfThingsThatWent(PuzzleGameType::Immersion));
	EXPECT_FALSE(LetsGoOfThingsThatWent(PuzzleGameType::Hanoi));
}

// A deleted puzzle game lets go of its lanterns that are still there, its trees, its bases and its rings, in this order.
// Its mists and bats stay, and the puzzle counts as played.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(PuzzleGameRules, ADeletedPuzzleLetsGoOfItsPiecesButNotItsMistsAndBats)
{
	using openblack::ecs::components::PuzzleGame;
	const auto thing = [](uint32_t id) { return static_cast<entt::entity>(id); };
	PuzzleGame game;
	game.lanterns = {thing(1), thing(2), entt::null, thing(3)};
	game.trees.at(0) = thing(10);
	game.trees.at(8) = thing(18);
	game.bases = {thing(20), thing(21), entt::null};
	game.rings = {thing(30), entt::null, entt::null, thing(33)};
	game.mists.at(4) = thing(40);
	game.bats.at(4) = thing(50);
	// The second lantern went, so the puzzle keeps no hold of it to let go of
	const auto pieces =
	    openblack::ecs::puzzle_game::TakePiecesForDelete(game, [&thing](entt::entity piece) { return piece != thing(2); });
	const std::vector<entt::entity> expected {thing(1),  thing(3),  thing(10), thing(18),
	                                          thing(20), thing(21), thing(30), thing(33)};
	EXPECT_EQ(pieces, expected);
	EXPECT_TRUE(game.played);
	EXPECT_EQ(game.lanterns.at(0), entt::entity {entt::null});
	EXPECT_EQ(game.lanterns.at(1), thing(2));
	EXPECT_EQ(game.trees.at(0), entt::entity {entt::null});
	EXPECT_EQ(game.rings.at(3), entt::entity {entt::null});
	EXPECT_EQ(game.mists.at(4), thing(40));
	EXPECT_EQ(game.bats.at(4), thing(50));
}
