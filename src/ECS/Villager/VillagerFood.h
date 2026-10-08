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

#include <entt/entity/fwd.hpp>

#include "ECS/Components/LivingAction.h"
#include "Enums.h"

namespace openblack
{
struct GVillagerInfo;
}

// The villager's food (docs/bw1-notes/villagers.md): the hunger of the periodic check, the amounts, where it goes to
// eat, and the eating states 117 EAT_FOOD, 118 EAT_FOOD_AT_HOME, 212 SHOW_POISONED, 33 / 34 (the storage pit) and 35
// (home with food, the housewife's). The pure layer has the float arithmetic for the tests.

namespace openblack::ecs::villager
{
// ---- the pure layer ----------------------------------------------------------------------------------------------

/// CheckHungry's batch: drop = (float)(u64) turns x 9e-5 (reducesFoodBy); / tribalPower (when the villager has a
/// player); x speed when speed > 1 (double compare) and it is moving; food = max(food - drop, 0)
[[nodiscard]] float HungerBatch(float food, uint32_t turns, float reducesFoodBy, std::optional<float> tribalPower, float speed,
                                bool moving);
/// GetAmountOfFoodToEat: t = (float)(POWER(food) x dinner) (integer multiply, stored); with a town's Food desire
/// (no boosts) clamped to [0, 1]: truncate((1 - 0.3 c) x t); without one truncate(t)
[[nodiscard]] uint32_t FoodToEat(float food, uint32_t dinner, std::optional<float> townFoodDesire);
/// GetAmountOfFoodRequiredForMeal: max(eat - held, 0), signed
[[nodiscard]] uint32_t FoodRequiredForMeal(uint32_t eat, int16_t held);
/// EatFoodHeld's arithmetic: eaten = min((float) eat, (float) held); food = (eaten / eat) x nourish + food,
/// below 0 (or NaN: 0 / 0) -> 0, above 1 -> 1. Returns {eaten, food}
struct EatResult
{
	float eaten {0.0f};
	float food {0.0f};
};
[[nodiscard]] EatResult EatHeld(float food, int16_t held, uint32_t eat, float nourish);

// ---- the hunger --------------------------------------------------------------------------------------------------

/// The hunger check: the last of the periodic checks (CheckEveryTime), also used by 129
bool CheckHungry(entt::entity villager, uint32_t turn);
[[nodiscard]] uint32_t GetAmountOfFoodToEat(entt::entity villager);
[[nodiscard]] uint32_t GetAmountOfFoodRequiredForMeal(entt::entity villager);
/// IsHungry ? ChangeStateToFindFoodToEat : 0
uint32_t CheckSatisfyOwnFoodDesire(entt::entity villager);
/// Returns 0 / 1: need 0 -> eat (117, or 118 inside); its functional abode has
/// enough with what it carries -> 36 (118 inside); the storage pit (the town's, or its abode) functional with enough
/// -> 33; no functional one -> walk to GetResourceDropoffPos with FINAL 34 unless it is there; it carries some -> eat;
/// else 0
uint32_t ChangeStateToFindFoodToEat(entt::entity villager);
/// DropFood, the belly, the town's UseFood. Returns the food
float EatFoodHeld(entt::entity villager);
/// (n): m = min(n, abode food); GetResourceFrom(abode, FOOD, m) and PickupFood(what
/// it took) again: the abode loses m and the villager gains 2m (literal)
void GetFoodFromHome(entt::entity villager, uint32_t amount);

// ---- the state functions (LivingActionSystem.cpp k_VillagerStateTable) ------------------------------------------

/// 117 EAT_FOOD: EatFoodHeld; PlayAnimThenSetState(poisoned ? 212 : 163). 1
uint32_t EatFood(components::LivingAction& action);
/// 118 EAT_FOOD_AT_HOME: eat - held > 0 -> GetFoodFromHome; EatFoodHeld;
/// SetTopState(poisoned ? 212 : 38). 1
uint32_t EatFoodAtHome(components::LivingAction& action);
/// 212 SHOW_POISONED: inside -> SetupMoveToWithHug(FindPosOutsideAbode(0), 212); then
/// PlayAnimThenSetState(163). 1
uint32_t ShowPoisoned(components::LivingAction& action);
/// 33 GOTO_STORAGE_PIT_FOR_FOOD. 1
uint32_t GotoStoragePitForFood(components::LivingAction& action);
/// 34 ARRIVES_AT_STORAGE_PIT_FOR_FOOD =
/// ArrivesAtStoragePitForResource(FOOD, GetAmountOfFoodRequiredForMeal, 163, 163)
uint32_t ArrivesAtStoragePitForFood(components::LivingAction& action);
/// 35 ARRIVES_AT_HOME_WITH_FOOD: with an abode, abode.AddResource(FOOD,
/// DropFood(0)) (all it carries); then ArrivesHome
uint32_t ArrivesAtHomeWithFood(components::LivingAction& action);
} // namespace openblack::ecs::villager
