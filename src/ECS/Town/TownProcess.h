/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>

// The town's turn and the players' loop that calls it (each player, then each of its towns). The steps not ported yet
// are TODO, or calls to existing functions of other systems.

namespace openblack::ecs::town_process
{
/// The town's turn, its steps in order (the stats, the plan request flag, the desires, the worship every 10 turns, the
/// build pulse and the empty town countdown; the rest TODO)
void ProcessTown(entt::entity town);
/// The players' town part: for every player and then the neutral one, each of its towns in order
/// (map_cells::ForEachTown). The game logic loop calls it between the sharks and the global game lists, before the
/// villagers
void ProcessPlayers();
} // namespace openblack::ecs::town_process
