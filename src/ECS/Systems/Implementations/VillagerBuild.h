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

#include "Enums.h"

namespace openblack::ecs::components
{
struct LivingAction;
}

/// A villager building for its town: it takes up the site its town most wants built, fetches wood from the storage pit
/// when the site lacks it, carries it to one of the places round the building, puts it down there and builds in strokes,
/// each using some of the site's wood to raise the building, until the building stands or the wood runs out. Also the
/// crowd a script gathers villagers into. The state functions are the living action system's.
namespace openblack::ecs::villager_build
{

/// Whether a state is one of the building states, which share their builder's place at its site
[[nodiscard]] bool IsBuildingState(VillagerStates state);

/// The town wants building: the villager takes up its best site. 1 when it did.
uint32_t CheckSatisfyToBuild(components::LivingAction& action);
/// The town wants a home or a civic building: with any site it takes up the best one. 1 when it did.
uint32_t CheckNeededForBuilding(components::LivingAction& action);

uint32_t ArrivesAtStoragePitForBuildingMaterials(components::LivingAction& action);
uint32_t ArrivesAtBuildingSite(components::LivingAction& action);
uint32_t Building(components::LivingAction& action);
uint32_t ReenterBuildingState(components::LivingAction& action);
/// Going into a building state from another kind of state it counts once more among the site's builders, and leaving
/// for another kind it is let go of the site
bool EnterBuilding(components::LivingAction& action, VillagerStates previous, VillagerStates next);
bool ExitBuilding(components::LivingAction& action, VillagerStates next);

/// A villager carrying enough wood or food to show takes it to the storage pit. 1 when it went.
uint32_t CheckTakeResourcesToStoragePit(components::LivingAction& action);
uint32_t GotoStoragePitForDropOff(components::LivingAction& action);
uint32_t ArrivesAtStoragePitForDropOff(components::LivingAction& action);

/// In a script's crowd: a fresh crowd clip each time the last has played
uint32_t ScriptInCrowd(components::LivingAction& action);

} // namespace openblack::ecs::villager_build
