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

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

#include "ECS/Components/LivingAction.h"
#include "Enums.h"

// The pregnancy and the births: how a woman gets pregnant at home (CheckGetPregnantAtHome, VillagerHome.h), the count
// down (UpdatePregnancy, VillagerAge.h), the states 110 HOUSEWIFE_STARTS_GIVING_BIRTH, 111 HOUSEWIFE_GIVING_BIRTH and 112
// HOUSEWIFE_GIVEN_BIRTH, and ChildBorn. The housewife's day (100..109) and the meal call are dead code in the game and
// not here.

namespace openblack::ecs::villager
{
// ---- the pure layer ----------------------------------------------------------------------------------------------

/// The range of HousewifeStartsGivingBirth's GameRand: truncated turns per day (36000 turns per year / 365.25 days,
/// 98.5626f) = 98
[[nodiscard]] uint32_t BirthCounterRange();
/// The birth counter = truncated r + turns per day x 0.25 + 1 (u16, float steps): r + 25
[[nodiscard]] uint16_t BirthCounter(uint32_t r);

// ---- the pregnancy -----------------------------------------------------------------------------------------------

/// An abode, not pregnant, a town and IsSexuallyActive; s = the town's GetDesireSignificanceToVillager(8
/// FOR_CHILDREN); n = the abode's child count + its pregnant inhabitants. s > 0 and MaxChildrenInAbode > n (unsigned)
/// -> true
[[nodiscard]] bool WillHousewifeGetPregnant(entt::entity villager);
/// pregnancy = TimePregnantFor (u16); not at home -> GoHome (its result). (approximate) at home 1: the original returns
/// a pointer
uint32_t HousewifeGetsPregnant(entt::entity villager);

// ---- the birth (states 110..112) ---------------------------------------------------------------------------------

/// Called from UpdatePregnancy, and the state 110's function: pregnancy = 0; the counter = BirthCounter(GameRand(98));
/// SetTopState(111); HousewifeGivingBirth (its result)
uint32_t HousewifeStartsGivingBirth(entt::entity villager);
/// State 110 HOUSEWIFE_STARTS_GIVING_BIRTH ((inferred) no code sets 110)
uint32_t HousewifeStartsGivingBirthState(components::LivingAction& action);
/// --counter (u16) != 0 -> 1. At 0: ChildBorn; a child -> a random birth sound at its position; SetTopState(112); 1
uint32_t HousewifeGivingBirth(entt::entity villager);
/// State 111 HOUSEWIFE_GIVING_BIRTH
uint32_t HousewifeGivingBirthState(components::LivingAction& action);
/// State 112 HOUSEWIFE_GIVEN_BIRTH: pregnancy (u16) = 0, then GoHome
uint32_t HousewifeGivenBirth(components::LivingAction& action);
/// r = GameRand(100); r <= BoyGirlChance -> FindVillagerInfo(the tribe, GameRand(6) + 1), null -> the mother's info;
/// else the mother's. A child of that info, age 1, at the mother's position (a skeleton if she is one); null -> null.
/// The mother's abode -> AddVillagerToAbode; else her town -> AddVillagerToTown; else the vagrants' head. The child's
/// mother is set; the player's births stat ++ (TODO(Intro)); a poisoned mother -> a poisoned child;
/// ChildDecideWhatToDo(child). The child
entt::entity ChildBorn(entt::entity mother);
} // namespace openblack::ecs::villager
