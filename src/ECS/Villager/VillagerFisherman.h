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

// The fishermen: CheckSatisfyFoodDesire's fish-farm job, the walk to the farm, the fishing spot within 2.5 m, the
// catch by season (56, clip 262) and the fishermen list (EnterFishing / ExitFishing). The farm's stock is never
// touched by them. The farm side is ecs::fish_farms (ECS/FishFarms.h),
// reached through the Locator's villagerFishFarms service. Puzzle fish farms have no town: no desire
// reaches them.

namespace openblack::ecs::villager
{
// ---- the pure layer ----------------------------------------------------------------------------------------------

/// (float)(u64)Score x GetDistanceModifier(d, 500) (the modifier x the stored float)
[[nodiscard]] float FishFarmScore(int32_t farmScore, float distance);
/// (float)(u64)MaxFoodCarried x 0.25 (37.5 for 150)
[[nodiscard]] float FishCatchBase(uint32_t maxFoodCarried);
/// The season's multiplier: 1.0, 0.9, 0.7, 0.6 for GetSeason 0..3
[[nodiscard]] float FishSeasonMultiplier(uint32_t season);
/// v = catch x mult > (float)cap ? (float)cap : catch x mult; n = tribalPower x v truncated
[[nodiscard]] int32_t FishCatch(float base, uint32_t season, int16_t capacity, float tribalPower);

// ---- the finder and the job --------------------------------------------------------------------------------------

/// Over the town's fish farms (newest first) the strictly best FishFarmScore above 0 (1 only with no fisherman), and
/// its score
[[nodiscard]] entt::entity FindBestFishFarm(entt::entity town, entt::entity villager, float& score);
/// SetTopState(163), the target thing = farm, SetupMoveToOnFootpath(farm, farm's arrive point, 55). 1
uint32_t VillagerBecomesFisherman(entt::entity villager, entt::entity farm);
/// (Also used by the disciples' decision) a town and a best fish farm -> VillagerBecomesFisherman, 1; else 0
uint32_t FishermanLookForWater(entt::entity villager);
/// The map cells of x and z are the farm's
[[nodiscard]] bool IsAtValidFishingPos(entt::entity villager);

// ---- the states (LivingActionSystem.cpp k_VillagerStateTable) ---------------------------------------------------

/// 55 FISHERMAN_ARRIVES_AT_FISHING
uint32_t FishermanArrivesAtFishing(components::LivingAction& action);
/// 56 FISHING (clip 262 P_FISHERMAN)
uint32_t Fishing(components::LivingAction& action);
/// The entry of 55 / 56: the same entry function -> 1; no town -> 1; not in the farm's list -> AddFisherman (which sets
/// the target thing to the farm). 1
uint32_t EnterFishing(components::LivingAction& action, VillagerStates final, VillagerStates next);
/// The exit of 55 / 56: the same exit -> 1; the target thing available -> RemoveFisherman; the target thing cleared. 1
uint32_t ExitFishing(components::LivingAction& action, VillagerStates next);
} // namespace openblack::ecs::villager
