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
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Commands/PlayerCommands.h"
#include "Common/DebugThingActs.h"
#include "Enums.h"

/// The acts of the Physics, Particles, Temple villagers and Path finding debug windows on the world of the game
/// (debug_thing_acts). The turn runs them for a command. The windows use Do: in a network game it sends the command, and in
/// a game of this computer alone it runs the act at once.
namespace openblack::debug_thing_acts
{

/// Runs an act on the world of this computer for the player
ActResult Run(const player_commands::DebugPhysics& act, PlayerNames player);
ActResult Run(const player_commands::DebugParticle& act);
/// The villagers that a Make act makes go into the list of the player, for the Take away button of the window
ActResult Run(const player_commands::DebugTempleVillager& act, PlayerNames player);
ActResult Run(const player_commands::DebugVillagerPath& act);

/// Whether the acts go as commands of the turns: true while a network game is live
[[nodiscard]] bool SendsCommands();

/// In a network game, sends the act as a command of the turns, and returns none. Else runs the act at once for the player
/// of this computer, and returns the result.
std::optional<ActResult> Do(const player_commands::DebugPhysics& act);
std::optional<ActResult> Do(const player_commands::DebugParticle& act);
std::optional<ActResult> Do(const player_commands::DebugTempleVillager& act);
std::optional<ActResult> Do(const player_commands::DebugVillagerPath& act);

/// The villagers that the Temple villagers window of the player of this computer made and that are still in the world
[[nodiscard]] std::vector<entt::entity> MadeVillagers();

/// Gives the effect the nearest creatures and villagers to act on, at most the count. Two creatures or villagers at the same
/// distance go in the order of their numbers, so that each computer gives the same targets.
void GiveNearestTargets(uint32_t effect, const glm::vec3& origin, uint8_t count);

} // namespace openblack::debug_thing_acts
