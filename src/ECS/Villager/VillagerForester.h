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

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

#include "ECS/Components/LivingAction.h"
#include "Enums.h"

// The foresters: the Wood desire (DecideHowToGetWood: VillagerBuild.h), the walk to the forest (47 / 48), the
// chopping (49 / 50, clip 217), the BigForest (53), and what a villager does once it holds wood (52,
// GotWoodDecideWhatToDo, also after reaction 12's log). The trees and forests are ecs::Trees (ECS/Trees.h).
// Villager::targetThing is the felled tree while it falls.

namespace openblack::ecs::villager
{
// ---- the pure layer ----------------------------------------------------------------------------------------------

/// FindTreeNearVillager's result: 0 no tree, 1 a tree (not touching its working point), 10 touching it
[[nodiscard]] uint32_t TreeSearchResult(bool found, bool touching);
/// IsTouching: the speed in metres > the distance from the object (equal is not touching)
[[nodiscard]] bool TouchingRule(float distanceFromObject, float speedInMetres);

// ---- the functions -----------------------------------------------------------------------------------------------

/// ecs::FindTreeNearVillager (the 9 cells' nearest tree by its working point) and IsTouching of that point; `tree` is
/// written when one is found
uint32_t FindTreeNearVillager(entt::entity villager, entt::entity& tree);
/// IsTouching with pos a world point (ecs::object::GetDistanceFromObject: distance - radius)
[[nodiscard]] bool IsTouching(entt::entity villager, glm::vec3 pos);
/// The forest's centre tree's working point, else the forest's centre; SetupMoveToWithHug(p, state) == 1 -> the raw
/// LivingAction top state set to 47 (no exit, entry or clips; turns since the change = 0), 1; else 0
uint32_t VillagerGotoForest(entt::entity villager, uint32_t forestId, VillagerStates state);
/// (Used by 52, 53 and 22) no wood held -> SetTopState(163); a disciple held at its job -> 163
/// (villager::DiscipleHeldAtJob); its building site valid with builders needed or a BUILDER disciple, and
/// GotoBuildingSite == 1 -> 1; CheckNeededForBuilding -> 1; else SetTopState(31
/// GOTO_STORAGE_PIT_FOR_DROP_OFF). Always 1
uint32_t GotWoodDecideWhatToDo(entt::entity villager);

// ---- the states (LivingActionSystem.cpp k_VillagerStateTable; exit ExitForesting for 47-50, 52) -----------------

/// 47 FORESTER_MOVE_TO_FOREST: `moveResult` is the MOVE_TO_POS step's, which the table runs first (LivingActionSystem's
/// VillagerMoveToPos). Its look-ahead needs 7 (a new map cell) and a wall-hug move byte of 2: never in openblack. 1
uint32_t ForesterMoveToForest(components::LivingAction& action, uint32_t moveResult);
/// 48 FORESTER_GOTO_FOREST: just CheckSatisfyWoodDesire
uint32_t ForesterGotoForest(components::LivingAction& action);
/// 49 FORESTER_ARRIVES_AT_FOREST (clip 217)
uint32_t ForesterArrivesAtForest(components::LivingAction& action);
uint32_t ForesterArrivesAtForest(entt::entity villager);
/// 50 FORESTER_CHOPS_TREE (clip 217)
uint32_t ForesterChopsTree(components::LivingAction& action);
/// Row 51: SetTopState(163), 1. The row stays k_TodoEntry: never entered in the original
uint32_t ForesterChopsTreeForBuilding(components::LivingAction& action);
/// 52 FORESTER_FINISHED_FORESTERING: wood held > 0 -> GotWoodDecideWhatToDo; else SetTopState(163), 1
uint32_t ForesterFinishedForestering(components::LivingAction& action);
uint32_t ForesterFinishedForestering(entt::entity villager);
/// 53 ARRIVES_AT_BIG_FOREST
uint32_t ArrivesAtBigForest(components::LivingAction& action);
/// Row 54: `return 1`. The row stays k_TodoEntry (as 51)
uint32_t ArrivesAtBigForestForBuilding(components::LivingAction& action);
/// 186 TAKE_WOOD_FROM_TREE
uint32_t TakeWoodFromTree(components::LivingAction& action);
/// 187 TAKE_WOOD_FROM_POT: `return 1`
uint32_t TakeWoodFromPot(components::LivingAction& action);
/// The exit of 47-50, 52: the target thing cleared; 1
uint32_t ExitForesting(components::LivingAction& action, VillagerStates next);
} // namespace openblack::ecs::villager
