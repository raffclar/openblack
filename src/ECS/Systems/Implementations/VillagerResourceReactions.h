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

#include "ECS/Components/LivingAction.h"
#include "Enums.h"

// The villagers' reactions 7 REACT_TO_FOOD and 12 REACT_TO_WOOD: a dropped food pile or a fallen log near a villager
// that wants it sends it there (19 / 21), it picks the resource up (20 / 22, clip 340) and takes it to the storage pit
// (31) or eats it (117). They are slot types of the villager type table (villager_reactions::SlotTypeOf): the slot is
// components::VillagerReactionSlot.

namespace openblack::ecs::villager_resource_reactions
{
// ---- the pure layer ----------------------------------------------------------------------------------------------

/// The villager's own interest in a food object: boost x frac x want x s x d + interact (24-bit FPU: every product a
/// float, in this order)
[[nodiscard]] float FoodInterestScore(float boost, float frac, float want, float stateInterest, float distanceModifier,
                                      float interact);
/// The town's interest in a food or wood object: desire x frac x s x d, then interact + it
[[nodiscard]] float TownInterestScore(float desire, float frac, float stateInterest, float distanceModifier, float interact);
/// The food drop-off score on arriving: CalculateDesireForFood x frac x dm
[[nodiscard]] float FoodDropOffScore(float desire, float frac, float distanceModifier);
/// The capacity's share: (float)capacity / (int)max
[[nodiscard]] float CapacityFraction(int16_t capacity, uint32_t maxCarried);
/// How long a falling log is waited for: (1000 / msPerTurn) x 100.0 turns, truncated (1000 at 100 ms)
[[nodiscard]] uint32_t ReactionFallLimit(uint32_t msPerTurn);
/// The final state 47..50 or 52 (the forester's work): no wait before reacting to wood again
[[nodiscard]] bool IsForesterWorkState(VillagerStates final);

// ---- the type table's slots ---------------------------------------------------------------------------------------

/// IsInterestedInFoodObject: the object available, not in the physics nor the hand; GDM(distance,
/// maxReactionDistance[7]) x the final state's food interest x GetDesireForFood x the capacity's share x (sped up ?
/// 2 : 1) + the interact desire > 0.25 -> 1; else with a town the town's food desire x share x interest x GDM +
/// interact > the food desire info's desireTriggersVillagerAction (0.01)
[[nodiscard]] bool IsInterestedInFoodObject(entt::entity villager, entt::entity object);
/// IsInterestedInWoodObject: available, not in the hand, wood capacity != 0, a town, life above
/// DamageThresholdToGoHome; a dead tree with less wood held than the capacity and IsVillagerAvailable -> 1; a valid
/// building site that needs no wood -> 0; a building site's pot or a disciple that fetches wood, and available -> 1;
/// else the town's raw wood desire x the capacity's share x the final state's wood interest x GDM(d, 35) + interact >
/// the wood desire info's desireTriggersVillagerAction (0.01)
[[nodiscard]] bool IsInterestedInWoodObject(entt::entity villager, entt::entity object);
/// The food reaction's priority: the initiator in the hand or the physics -> 0; no food -> 0; farther than
/// ReactionInfo[7] maxDistanceToRunAwayFromObject (100) -> 0; IsInterestedInFoodObject ? priority (60) : 0
[[nodiscard]] uint8_t ReactToFoodPriority(entt::entity villager, uint32_t reaction, uint32_t other);
/// The wood reaction's priority: in the hand -> 0; IsInterestedInWoodObject ? priority (55) : 0
[[nodiscard]] uint8_t ReactToWoodPriority(entt::entity villager, uint32_t reaction, uint32_t other);
/// Adds the reaction with 19 GOTO_FOOD_REACTION and keeps the object in the slot
void SetupReactToFood(entt::entity villager, entt::entity object, uint32_t reaction);
/// Adds the reaction with 21 GOTO_WOOD_REACTION and keeps the object in the slot
void SetupReactToWood(entt::entity villager, entt::entity object, uint32_t reaction);
/// The standard turns to react: effects::reactions::StandardTurnsToReact
uint32_t StandardTurnsToReact(entt::entity villager, entt::entity object, uint8_t type, float distance);
/// The standard turns before reacting again
uint32_t StandardTurnsBeforeAgain(entt::entity villager, entt::entity object, uint8_t type, float distance);
/// IsForesterWorkState(final) ? 0 : the standard (0 for 12 too: info.dat's again is 0)
uint32_t TurnsBeforeReactingToWoodAgain(entt::entity villager, entt::entity object, uint8_t type, float distance);

// ---- the states (LivingActionSystem.cpp k_VillagerStateTable) ---------------------------------------------------

/// 19 GOTO_FOOD_REACTION: no object or not available -> StopReactingAndSetState; else
/// SetupMoveToWithHug(object.GetWorkingPos(me), 20). 1
uint32_t GotoFoodReaction(components::LivingAction& action);
/// 20 ARRIVES_AT_FOOD_REACTION
uint32_t ArrivesAtFoodReaction(components::LivingAction& action);
/// 21 GOTO_WOOD_REACTION: as 19 (also in the hand or on fire -> stop), with FINAL 22
uint32_t GotoWoodReaction(components::LivingAction& action);
/// 22 ARRIVES_AT_WOOD_REACTION
uint32_t ArrivesAtWoodReaction(components::LivingAction& action);
} // namespace openblack::ecs::villager_resource_reactions
