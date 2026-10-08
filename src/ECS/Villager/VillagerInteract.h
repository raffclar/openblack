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

#include "ECS/Components/LivingAction.h"

// The disciple's inspection states 171-175 CHECK_INTERACT_WITH_* (docs/bw1-notes/villagers.md "Soft drop and
// landing"). Their state function reads the inspected object (components::Villager::targetThing) and sets the villager
// to work at it. They are reached from the landing's disciple branch (the close objects to interact with -> 233
// INSPECT_OBJECT -> the object's state), which is pending: until then nothing enters them. No entry / exit function; they
// always react to a town emergency.
//
// 172 CHECK_INTERACT_WITH_ABODE is pending (disciples): it needs the move-house check (and the move into another town), the
// town of another player's abode and the player of a town-less abode. Its reading: the target as a MultiMapFixed,
// available (else 0); built and not in need of repair, or another player's -> (A); same player, another town and not a
// town centre -> (A); another town's centre -> ForceMoveVillagerToAbode; then villager::SetupBuildingObjectForBuilding
// == 1 -> the creature empathises with the player's town desire (5, 0.5, Pos), 1; else DecideWhatToDo.
// (A): the move-house check == 1 -> 1; another player's -> the villager's town = its town, SetTopState(163), 1; a
// workshop -> SetTopState(163), 1; its own abode -> GO_HOME; else 0.

namespace openblack::ecs::villager
{

/// 173 CHECK_INTERACT_WITH_FIELD: the target field (read without a null test in the original) of the villager's player
/// (a field's player is its town's, the villager's too; both none counts as the same) and SetFarmerGotoField(field, 1)
/// == 1 -> 1 (with a player, the player's creature may come to share the town's need for food, 0.5 at the villager:
/// ECS/CreatureMimic.h); else 0
uint32_t CheckInteractWithField(components::LivingAction& action);

/// 174 CHECK_INTERACT_WITH_FISH_FARM: the same for the target fish farm (its player is its town's) and
/// VillagerBecomesFisherman == 1 -> 1 (the same need for food, 0.5); else 0
uint32_t CheckInteractWithFishFarm(components::LivingAction& action);

} // namespace openblack::ecs::villager
