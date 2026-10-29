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

// The answer of two kinds depends on how the game starts
TEST(PuzzleGameRules, TwoKindsHaveNoKnownAnswer)
{
	EXPECT_FALSE(PlayedBeforeSolved(PuzzleGameType::Immersion).has_value());
	EXPECT_FALSE(PlayedBeforeSolved(PuzzleGameType::Lionsheep).has_value());
}

// A puzzle game that nobody started is not yet begun, or won when it always gives yes
TEST(PuzzleGameRules, TheStateBeforeTheStart)
{
	EXPECT_EQ(StateBeforeStart(false), PuzzleGameStatus::GameNotYetBegun);
	EXPECT_EQ(StateBeforeStart(true), PuzzleGameStatus::Won);
}
