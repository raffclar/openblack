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

#include <entt/entity/entity.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{
struct LivingAction;
}

/// A villager's belly: it empties at each of the villager's checks, a hungry villager loses life and, when its state
/// lets it, sets off to eat, at home from its home's store, from the town's storage pit, or from what it carries. Eating
/// fills it; poisoned food poisons it and magic food speeds it up for a while. A villager starving to no life dies.
/// The state functions are the living action system's.
namespace openblack::ecs::villager_food
{

/// The check on its belly, made with its other needs: the food burnt since the last check, life lost to hunger or poison,
/// setting off to eat when its state gives way to hunger or starvation, and death by starvation. Whether it now does
/// something else.
bool CheckHungry(components::LivingAction& action);
/// A hungry villager sets off to eat, where it can get enough. Whether it went.
bool ChangeStateToFindFoodToEat(components::LivingAction& action);
/// Hungry, it sets off to eat. Whether it went.
bool CheckSatisfyOwnFoodDesire(components::LivingAction& action);
/// Whether it is hungry
[[nodiscard]] bool IsHungry(entt::entity villager);

/// Takes up to an amount of a resource from a thing into the villager's hands. Taken from sped-up food it is sped up;
/// from poisoned food, poisoned. What it took.
uint16_t GetResourceFrom(entt::entity villager, entt::entity source, ResourceType type, uint16_t amount);
/// How much food the villager must take for a meal, beyond what it carries
[[nodiscard]] uint32_t FoodRequiredForMeal(entt::entity villager);
/// It eats as much of its meal as it carries; its town counts the food used. How full it is after.
float EatFoodHeld(entt::entity villager);
/// The magic food speed-up wears off, a tick every tenth turn
void ProcessFoodSpeedUp(entt::entity villager);

/// How urgently a villager heeds food lying about: the food reaction's own priority when the food has some in it, is
/// within the reaction's running reach and catches its eye; else not at all
[[nodiscard]] uint32_t ReactToFoodPriority(entt::entity villager, entt::entity food, uint32_t priority);
/// It walks up to the food it noticed
uint32_t GotoFoodReaction(components::LivingAction& action);
/// At the food it picks up what it can carry; hungry, it eats it; otherwise it may take it to the store
uint32_t ArrivesAtFoodReaction(components::LivingAction& action);

uint32_t GotoStoragePitForFood(components::LivingAction& action);
uint32_t ArrivesAtStoragePitForFood(components::LivingAction& action);
uint32_t EatFood(components::LivingAction& action);
uint32_t EatFoodAtHome(components::LivingAction& action);
uint32_t ShowPoisoned(components::LivingAction& action);
/// The states the game keeps for dinners and lying weak that do nothing of their own
uint32_t DoesNothing(components::LivingAction& action);

} // namespace openblack::ecs::villager_food
