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

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"
#include "ScriptHeaders/ScriptEnums.h"

namespace openblack::ecs::systems
{

/// The puzzle games that the scripts make: their pieces, their turns and what the scripts ask of them
class PuzzleGameSystemInterface
{
public:
	virtual ~PuzzleGameSystemInterface() = default;

	/// A script makes a puzzle game of a kind on the land under a point, at an angle in radians and a size. A kind with
	/// pieces makes its pieces.
	virtual entt::entity Create(script::PuzzleGameType type, glm::vec3 point, float yAngleRadians, float scale) = 0;
	/// Each turn: each puzzle that is not solved puts back its pieces that moved and tests for a solution
	virtual void ProcessTurn() = 0;
	/// The script's "played" test of a puzzle game: whether the player solved it. The test of a solution fixes the
	/// pieces of a solved puzzle. None for a thing that is not a puzzle game.
	[[nodiscard]] virtual std::optional<bool> Played(entt::entity puzzle) = 0;
	/// The state that a script reads of a puzzle game. None for a thing that is not a puzzle game.
	[[nodiscard]] virtual std::optional<script::PuzzleGameStatus> State(entt::entity puzzle) = 0;
	/// A hand took a piece of a puzzle: the piece comes off the piece that it rested on
	virtual void TakenInHand(entt::entity piece) = 0;
	/// A puzzle game goes from the world: its rings keep their links but belong to no puzzle
	virtual void Forget(entt::entity puzzle) = 0;
	/// A script deletes a puzzle game, or the scripts stop: the puzzle game stays, but its pieces go from the world.
	/// False for a thing that is not a puzzle game.
	virtual bool DeletePieces(entt::entity puzzle) = 0;
	/// A script starts a maze puzzle again: its animals go back to their first cells. A puzzle game of another kind does
	/// not change. False for a thing that is not a puzzle game.
	virtual bool Restart(entt::entity puzzle) = 0;
	/// A player tapped the land at a place in this turn. The maze puzzles read the place in their turn, and then the
	/// place goes.
	virtual void TapPlace(const map_coords::MapCoords& place) = 0;
	/// A player tapped a puzzle totem, such as a bell of the lion maze. A totem that the hand can tap takes the tap as a
	/// height of -1, and each tap makes a sound. False for a thing that is not a puzzle totem.
	virtual bool TapTotem(entt::entity totem) = 0;
	/// Something scared the fish at a point, such as the hand going into the sea. The shoals of the fish puzzle read the
	/// point in the next turn, and then the point goes. A later scare before that turn takes the place of an earlier one.
	virtual void ScareFish(glm::vec3 point) = 0;
	/// Whether there is a fish puzzle, whose shoals the scares of the fish go to
	[[nodiscard]] virtual bool HasFishPuzzle() const = 0;
	/// After a load of a saved game: each puzzle game forgets the state that a save does not keep, and no tapped place
	/// waits for the next turn
	virtual void AfterLoad() = 0;
};

} // namespace openblack::ecs::systems
