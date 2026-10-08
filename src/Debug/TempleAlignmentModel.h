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

#include <functional>
#include <string>
#include <vector>

#include <entt/entity/fwd.hpp>

#include "3D/TempleExteriorMorph.h"
#include "Enums.h"

namespace openblack::ecs::systems
{
class AlignmentSystemInterface;
class PlayerSystemInterface;
class TempleExteriorSystemInterface;
} // namespace openblack::ecs::systems

// What the Temple debug window works out before it draws or acts on a player's alignment: the players to choose from,
// the alignment a slider gives, where the temple's outside will head, and how its look reads. Pure functions over the
// services' interfaces, tested with fakes.

namespace openblack::debug::temple_alignment
{

/// The players in the game, in player order: those the player system has an entity for
/// The players that have an entity on this land, asked through a lookup that gives none (entt::null) for a player
/// that is not there, as magic::players::EntityOf does
using PlayerEntityOf = std::function<entt::entity(PlayerNames)>;
[[nodiscard]] std::vector<PlayerNames> PlayersInGame(const PlayerEntityOf& entityOf);

/// The slider's value, from -1, evil, to 1, good, given to the player's alignment through the alignment service
void SetAlignment(ecs::systems::AlignmentSystemInterface& alignment, PlayerNames player, float value);

/// Where the player's temple outside heads from the next turn, from 0, evil, to just short of 1, good
[[nodiscard]] float AlignmentTargetNow(const ecs::systems::AlignmentSystemInterface& alignment, PlayerNames player);

/// The heart's outside taken at once to where the player's alignment has it head
void SnapOutside(const ecs::systems::AlignmentSystemInterface& alignment, ecs::systems::TempleExteriorSystemInterface& exterior,
                 entt::entity heart, PlayerNames player);

/// How many turns the outside's alignment takes to reach the target at the game's step a turn; 0 when it is there
[[nodiscard]] uint32_t TurnsToReach(float current, float target);

/// The outside's look as the window shows it: where its alignment is, where it heads (the next turn's target when the
/// player's alignment changed since the last turn), what its mesh was last blended for, and how far it has to go
[[nodiscard]] std::string LookStatus(const TempleExteriorMorph::State& look, float targetNow);

} // namespace openblack::debug::temple_alignment
