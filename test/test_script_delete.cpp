/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <bit>

#include <gtest/gtest.h>

#include "ECS/ScriptDelete.h"

using namespace openblack::ecs::script_delete;

namespace
{
constexpr Target k_Hut {.object = true};
constexpr Target k_Creature {.object = true, .creature = true};
constexpr Target k_Flock {};
constexpr Target k_Temple {.object = true, .temple = true};
constexpr Target k_DestroyingTemple {.object = true, .temple = true, .templeDestroying = true};
constexpr Target k_Puzzle {.puzzleGame = true, .object = true};
} // namespace

TEST(ScriptDelete, TheSpeedsAreTheGameBytes)
{
	EXPECT_EQ(std::bit_cast<uint32_t>(k_ExplodeSpeed), 0x41700000u);
	EXPECT_EQ(std::bit_cast<uint32_t>(k_TempleExplodeSpeed), 0x42A00000u);
}

TEST(ScriptDelete, AtOnceRemovesAnyThing)
{
	EXPECT_EQ(Choose(0, k_Hut), Step::Remove);
	EXPECT_EQ(Choose(0, k_Flock), Step::Remove);
	EXPECT_EQ(Choose(0, k_Temple), Step::Remove);
}

TEST(ScriptDelete, WithFadeFizzesACreatureAndLeavesAGhostOfAnObject)
{
	EXPECT_EQ(Choose(1, k_Creature), Step::Fizz);
	EXPECT_EQ(Choose(1, k_Hut), Step::GhostThenRemove);
	EXPECT_EQ(Choose(1, k_Flock), Step::LetGo);
}

TEST(ScriptDelete, WithExplodeBreaksUpOnlyAnObject)
{
	EXPECT_EQ(Choose(2, k_Hut), Step::BreakUpThenRemove);
	EXPECT_EQ(Choose(2, k_Creature), Step::BreakUpThenRemove);
	EXPECT_EQ(Choose(2, k_Temple), Step::BreakUpThenRemove);
	EXPECT_EQ(Choose(2, k_Flock), Step::LetGo);
}

TEST(ScriptDelete, WithTempleExplodeStartsTheDestructionThenBreaksUpTheTemple)
{
	EXPECT_EQ(Choose(3, k_Temple), Step::StartTempleDestruction);
	EXPECT_EQ(Choose(3, k_DestroyingTemple), Step::BreakUpTemple);
	EXPECT_EQ(Choose(3, k_Hut), Step::LetGo);
	EXPECT_EQ(Choose(3, k_Flock), Step::LetGo);
}

TEST(ScriptDelete, AnOtherModeOnlyLetsGo)
{
	EXPECT_EQ(Choose(4, k_Hut), Step::LetGo);
	EXPECT_EQ(Choose(-1, k_Temple), Step::LetGo);
}

TEST(ScriptDelete, APuzzleGameStaysInEveryMode)
{
	for (int32_t mode = 0; mode < 5; ++mode)
	{
		EXPECT_EQ(Choose(mode, k_Puzzle), Step::ClearPuzzle) << mode;
	}
}
