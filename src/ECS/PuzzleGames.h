/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "ScriptHeaders/ScriptEnums.h"

namespace openblack::ecs
{

/// A PuzzleGame of that type at the script's vector (x, altitude + y, z); the angle goes in 2048 steps
/// ((int)(angle x 2048 / 2 pi)). Nothing else happens until it is processed.
entt::entity CreatePuzzleGame(const glm::vec3& position, script::PuzzleGameType type, float yAngleRadians, float scale);

/// One puzzle's turn (every turn): nothing once played; if the played test is true, it is played; else the type's
/// step. Type 14: the first time, the bait, its net and the two shoals (ecs::CreateFishPuzzle); the fish do the rest
/// (ecs::UpdateFishShoals).
void ProcessPuzzleGame(entt::entity puzzle);
/// Every PuzzleGame, and the parts of the ones the scripts deleted go (the bait with its fish plot and the two
/// shoals)
void ProcessPuzzleGamesTurn();

/// The CHL PLAYED of a PuzzleGame, by type: type 14 is 1 once its bait is done (set when the 30 fish stayed 500 ms in
/// the net), and that marks it played too; 0 while the bait is not made yet. The other types are not ported: 0.
[[nodiscard]] bool IsPuzzleGamePlayed(entt::entity puzzle);

/// The bait of a fish puzzle (entt::null before its first turn)
[[nodiscard]] entt::entity PuzzleGameBait(entt::entity puzzle);

} // namespace openblack::ecs
