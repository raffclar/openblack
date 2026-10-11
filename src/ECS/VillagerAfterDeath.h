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

#include <entt/entity/entity.hpp>

#include "Enums.h"

/// What a villager's death does to the world about it, in the order the death goes: the players here hear of it, its
/// player's alignment moves, its town counts it and its worship site lets it go; then, as it falls, the people about it
/// are told; and as its flesh goes, a puff of smoke and its soul rising, and another puff as the body goes. The souls are
/// moved on by the game's clock as they are drawn.
namespace openblack::ecs::villager_after_death
{

/// The player at this machine hears of a death: the guidance's line of a death in the village when the villager was the
/// player's own and the player here didn't cause it, by a miracle, a predator, chanting, starving, exhaustion or a
/// sacrifice. `killer` is the player put down for it, none for nobody's.
void Hear(entt::entity villager, DeathReason reason, std::optional<PlayerNames> killer);
/// The death moves the alignment of the player whose villager it was, by that player's change for the cause, twice as
/// much for a child. A villager of nobody's town moves nobody's.
void MoveOwnersAlignment(entt::entity villager, DeathReason reason);
/// The villager's town counts the death: when, put down to whom and of what
void CountInTown(entt::entity town, std::optional<PlayerNames> killer, DeathReason reason);
/// How many of a town's people have died of chanting at worship
[[nodiscard]] uint32_t DeathsOf(entt::entity town, DeathReason reason);
/// The villager leaves its town's worship site's worshippers
void LeaveWorship(entt::entity villager, entt::entity town);

/// The people about a villager are told of its death as it begins to fall (never of a death at home)
void TellThoseAbout(entt::entity villager);

/// A white puff of smoke a little out from the body along its facing, as the flesh goes and again as the body goes
void SmokePuff(entt::entity villager);
/// The villager's soul rises from its body, unless it lies on the sea
void SoulRises(entt::entity villager);
/// The souls move on by so many milliseconds of the game's clock, posed by their clips, and go as their clips end
void MoveSouls(uint32_t elapsedMs);

} // namespace openblack::ecs::villager_after_death
