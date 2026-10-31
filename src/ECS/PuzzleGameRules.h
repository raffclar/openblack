/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include "ScriptHeaders/ScriptEnums.h"

/// What the scripts read from a puzzle game before the player solves it
namespace openblack::ecs::puzzle_game
{

/// What PLAYED gives for a puzzle game that the player has not solved. Some kinds always give the same answer, and the
/// others give no.
[[nodiscard]] constexpr bool PlayedBeforeSolved(script::PuzzleGameType type) noexcept
{
	using script::PuzzleGameType;
	switch (type)
	{
	// These kinds always give yes
	case PuzzleGameType::None:
	case PuzzleGameType::Maze1:
	case PuzzleGameType::Maze2:
	case PuzzleGameType::Fishes2:
		return true;
	// The lions and sheep game starts with no side to move. The test then gives yes at once, so nobody can play it.
	case PuzzleGameType::Lionsheep:
		return true;
	// The mushroom game starts in progress and gives no until the player drops a mushroom in the cauldron
	case PuzzleGameType::Immersion:
	// Chess always gives no. Each other kind gives yes only when its pieces show a solution.
	case PuzzleGameType::Tree1:
	case PuzzleGameType::Tree2:
	case PuzzleGameType::Hanoi:
	case PuzzleGameType::Totem1:
	case PuzzleGameType::Totem2:
	case PuzzleGameType::Totem3:
	case PuzzleGameType::Totem4:
	case PuzzleGameType::Thesius1:
	case PuzzleGameType::Thesius2:
	case PuzzleGameType::Tilt1:
	case PuzzleGameType::Tilt2:
	case PuzzleGameType::Fishes1:
	case PuzzleGameType::Immersion2:
	case PuzzleGameType::Chess:
		return false;
	default:
		// A number past the last kind also always gives yes
		return true;
	}
}

/// Whether a puzzle game lets go of its things that went at the start of each turn, also after the solution. All kinds
/// but the beach temple puzzle do this. The beach temple puzzle makes its pieces again.
[[nodiscard]] constexpr bool LetsGoOfThingsThatWent(script::PuzzleGameType type) noexcept
{
	return type != script::PuzzleGameType::Hanoi;
}

/// What GET_OBJECT_STATE gives for a puzzle game that nobody has started: won once it gives yes to PLAYED, else not
/// yet begun
[[nodiscard]] constexpr script::PuzzleGameStatus StateBeforeStart(bool played) noexcept
{
	return played ? script::PuzzleGameStatus::Won : script::PuzzleGameStatus::GameNotYetBegun;
}

} // namespace openblack::ecs::puzzle_game
