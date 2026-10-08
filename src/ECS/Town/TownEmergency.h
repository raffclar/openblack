/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <entt/entity/entity.hpp>

#include "Enums.h"

// The town's emergency: a storage pit or a town centre knocked below its functional threshold (abodes::ReduceLife),
// one of them on fire or the town attacked while in the emergency (ProcessTownEmergency) start it; the housed
// villagers are called at once to congregate in the town (242) and the worship stops for its 1200 turns (the town
// info's emergency duration). town_queries::IsInStateOfEmergency tells whether it is running. Fields:
// components::Town::emergencyStartTurn, aggressorTurn, savedWorshipPercentage; the worship percentage through
// worship::percentage.
// The attack refresh needs UpdateAggressor's record, written by effects::ApplyEffect and the physical shield.

namespace openblack::ecs::town_emergency
{
/// Each abode of the town, each inhabitant: when its final state's row allows it ->
/// SetTopState(242 GOTO_CONGREGATE_IN_TOWN_AFTER_EMERGENCY). The per-villager part is
/// villager::CallToTownEmergency. Only housed villagers are called
void CallAllVillagersToTownEmergency(entt::entity town);
/// No emergency running -> CallAllVillagersToTownEmergency; then the start = the turn (also when it was set: the start
/// is refreshed, the villagers are not called again)
void SetInStateOfEmergency(entt::entity town);
/// TownProcess step 15: while in the emergency the attack of this turn refreshes it and a worship percentage != 0 is
/// saved and set to 0; else the storage pit or the town centre on fire starts it; else the saved percentage comes back
/// (when the current one is 0) and the saved percentage and the start are cleared
void ProcessTownEmergency(entt::entity town);
/// The aggressor record only (what ProcessTownEmergency's refresh while attacked reads): the aggressor = the effect's
/// caused player (`causedPlayer`), the neutral player without one, and the aggressor turn = the turn. Called by
/// effects::ApplyEffect. (not ported) the per-player aggression slot (seeded from the town info when empty, scaled by
/// one factor for the town's own player and another otherwise, a factor then decayed by 0.9), the town attack guidance
/// sound and the creature part
void UpdateAggressor(entt::entity town, std::optional<PlayerNames> causedPlayer);
} // namespace openblack::ecs::town_emergency
