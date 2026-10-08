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
#include <optional>
#include <vector>

#include <entt/entity/fwd.hpp>

#include "3D/MapCoords.h"
#include "ECS/Components/LivingAction.h"
#include "Enums.h"

// The farmers, and the town's FindBestField: CheckSatisfyFoodDesire's field job, the walk to the field, sowing
// (67 / 68, clip 259) and harvesting (67 / 69, clip 257) at random points within 5 m of its centre, the farmer list
// (EnterFarming / ExitFarming). The field side (crops, growth, food, the list) is ecs::fields (ECS/Fields.h); the
// villager reaches it through the Locator's villagerFields service. Villager::targetThing is the field,
// Villager::workPos the work point.

namespace openblack::ecs::villager
{
// ---- the pure layer ----------------------------------------------------------------------------------------------

/// FindBestField's score: GetDistanceModifier(d, 300) x GetDesireToBeFarmed (kept as a float)
[[nodiscard]] float FieldScore(float distance, float desireToBeFarmed);
/// FarmerDigsUpCrop's test: (float)(int16)GetFoodCapacity < 0.0 (a limit the original never changes)
[[nodiscard]] bool OverFull(int16_t capacity);

// ---- the finder and the job --------------------------------------------------------------------------------------

/// score = 0 first; over the town's fields (newest first) the strictly best FieldScore above 0, and its score
[[nodiscard]] entt::entity FindBestField(entt::entity town, entt::entity villager, float& score);
/// No field -> the town's FindBestField (no town or none -> 0); then SetFarmerGotoField(field)
uint32_t VillagerBecomesFarmer(entt::entity villager, entt::entity field);
/// arrive = GetArrivePos; activity 1 or 2 -> SetTopState(163), the field, SetupMoveToOnFootpath(field, arrive, 67), the
/// work point = RandomFarmPoint (2 GameFloatRand(10), after the footpath setup), 1; else 0
uint32_t SetFarmerGotoField(entt::entity villager, entt::entity field);
/// The field's activity through the villagerFields service: 1 to sow, 2 to
/// harvest, else 0. A disciple deciding what to do asks it of the best field
[[nodiscard]] int FieldActivityOf(entt::entity field);

// ---- the states (LivingActionSystem.cpp k_VillagerStateTable) ---------------------------------------------------

/// 67 FARMER_ARRIVES_AT_FARM (clip 259 P_FARMER_SOWING_SEEDS)
uint32_t FarmerArrivesAtFarm(components::LivingAction& action);
/// 68 FARMER_PLANTS_CROP: PlantCrop false -> 163; else IsStillSowing ? 67 : 163. 1
uint32_t FarmerPlantsCrop(components::LivingAction& action);
/// 69 FARMER_DIGS_UP_CROP (clip 257 P_FARMER_HARVESTING)
uint32_t FarmerDigsUpCrop(components::LivingAction& action);
/// The entry of 67-69: no field -> 0 (refused); another entry function before -> AddFarmer. 1
uint32_t EnterFarming(components::LivingAction& action, VillagerStates final, VillagerStates next);
/// The exit of 67-69: the same exit -> 1; else with a field still in the global field list -> RemoveFarmer (which
/// clears the villager's field). 1
uint32_t ExitFarming(components::LivingAction& action, VillagerStates next);
} // namespace openblack::ecs::villager
