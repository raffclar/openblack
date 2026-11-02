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

#include <vector>

#include <entt/entity/entity.hpp>

#include "ECS/Components/PuzzleGame.h"
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

/// A script deletes a puzzle game. The puzzle game stays, but it lets go of these things, in this order:
/// - each palm tree and each lantern that is still there, corner by corner,
/// - its trees,
/// - the lion and the villager or the sheep of a maze,
/// - the fences of a maze, slot by slot, the fence towards the next cell across first,
/// - the bells of the lion maze,
/// - the bases and the rings of the beach temple puzzle.
/// Its mists and its bats stay. The puzzle counts as played from then on, so its turn does nothing more.
/// The result is the list of the things to remove from the world.
template <typename IsThere>
[[nodiscard]] std::vector<entt::entity> TakePiecesForDelete(components::PuzzleGame& game, IsThere&& isThere)
{
	std::vector<entt::entity> pieces;
	const auto takeIfThere = [&pieces, &isThere](entt::entity& slot) {
		if (slot != entt::null && isThere(slot))
		{
			pieces.push_back(slot);
			slot = entt::null;
		}
	};
	const auto take = [&pieces](entt::entity& slot) {
		if (slot != entt::null)
		{
			pieces.push_back(slot);
			slot = entt::null;
		}
	};
	const auto takeAll = [&take](auto& slots) {
		for (auto& slot : slots)
		{
			take(slot);
		}
	};
	for (size_t corner = 0; corner < game.lanterns.size(); ++corner)
	{
		takeIfThere(game.maze.palms.at(corner));
		takeIfThere(game.lanterns.at(corner));
	}
	takeAll(game.trees);
	take(game.maze.lion);
	take(game.maze.hero);
	for (size_t slot = 0; slot < game.maze.acrossFences.size(); ++slot)
	{
		take(game.maze.acrossFences.at(slot));
		take(game.maze.alongFences.at(slot));
	}
	takeAll(game.maze.bells);
	takeAll(game.bases);
	takeAll(game.rings);
	game.played = true;
	return pieces;
}

/// The calm count of each puzzle game after a load of a saved game
constexpr int32_t k_CalmTurnsAfterLoad = 5;

/// A load of a saved game gives back a puzzle game without some of its state, as the game does:
/// - the calm count of the beach temple puzzle is 5,
/// - a maze has no marker on the last tapped cell, and its two animals turn to angle 0 before their next move,
/// - a maze has no fences towards the next cell across. These fences stay in the world, but the maze does not put them
///   back and does not delete them,
/// - the fish puzzle has no ring of floats and no shoals. It makes them again, as at its start, in its next turn.
inline void ForgetUnsavedState(components::PuzzleGame& game) noexcept
{
	game.calmTurns = k_CalmTurnsAfterLoad;
	game.maze.marker = components::MazeState {}.marker;
	game.maze.heroAngle = 0;
	game.maze.lionAngle = 0;
	game.maze.acrossFences.fill(entt::null);
	game.fish.reset();
}
} // namespace openblack::ecs::puzzle_game
