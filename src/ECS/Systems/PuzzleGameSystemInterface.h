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
};

} // namespace openblack::ecs::systems
