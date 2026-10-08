/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>

#include <entt/entity/entity.hpp>

#include "Enums.h"

// The miracles a town holds and whether it may have a worship site.
// A player owns a magic type while one of its towns holds it (Magic/Core/Players.h).

namespace openblack::worship::town
{
/// Not held yet -> held, the owner's magic type enabled, and the town centre's icon (town_centre::AddSpell of the seed
/// for a base magic, AddPowerUp of its level for a power-up). True if newly added.
bool AddMagicTypesHeld(entt::entity town, MagicType type);
/// The mirror (town_centre::RemoveSpell removes the spell's icon, ClearPowerUp clears the level)
void RemoveMagicTypesHeld(entt::entity town, MagicType type);
/// The town holds that magic type
[[nodiscard]] bool IsMagicTypeHeld(entt::entity town, MagicType type);

/// Creature theft: if the town holds the seed's base magic, it and its held power-ups go; `taken` records
/// which ([0] the base, [1..3] the levels). True when something was taken.
bool TakeSeedMagic(entt::entity town, SpellSeedType seed, std::array<bool, 4>& taken);
/// Gives them back / to another town
void GiveSeedMagic(entt::entity town, SpellSeedType seed, const std::array<bool, 4>& taken);

/// Not on land 1, not forbidden by the script, and a population (the town's adults and children)
[[nodiscard]] bool IsAllowedToCreateWorshipSite(entt::entity town);
/// Allowed, a player that is not type 3 with a citadel -> citadel::FindOrCreateWorshipSite and site::AddTown when the
/// town is not in it yet
void CheckAddWorshipSite(entt::entity town);

/// The town's centre: its Abode of type TOWN_CENTRE, or entt::null
[[nodiscard]] entt::entity TownCentreOf(entt::entity town);
/// The town's population for worship: its villagers
[[nodiscard]] int Population(entt::entity town);
/// The town's owner, or the neutral player
[[nodiscard]] PlayerNames OwnerOf(entt::entity town);
/// The town entity of an id (components::Town::id), or entt::null
[[nodiscard]] entt::entity FromId(uint32_t id);
} // namespace openblack::worship::town
