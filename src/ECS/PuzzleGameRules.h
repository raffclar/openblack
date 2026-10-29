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

#include <optional>

#include "ScriptHeaders/ScriptEnums.h"

/// What the scripts read from a puzzle game before the player solves it
namespace openblack::ecs::puzzle_game
{

/// What PLAYED gives for a puzzle game that the player has not solved. Some kinds always give the same answer, and the
/// others give no. Nothing comes for the two kinds whose answer before a solution depends on how the game starts.
[[nodiscard]] constexpr std::optional<bool> PlayedBeforeSolved(script::PuzzleGameType type) noexcept
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
	// The answer of these kinds is not known before the game starts
	case PuzzleGameType::Immersion:
	case PuzzleGameType::Lionsheep:
		return std::nullopt;
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

/// What GET_OBJECT_STATE gives for a puzzle game that nobody has started: won once it gives yes to PLAYED, else not
/// yet begun
[[nodiscard]] constexpr script::PuzzleGameStatus StateBeforeStart(bool played) noexcept
{
	return played ? script::PuzzleGameStatus::Won : script::PuzzleGameStatus::GameNotYetBegun;
}

} // namespace openblack::ecs::puzzle_game
