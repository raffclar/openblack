/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

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
