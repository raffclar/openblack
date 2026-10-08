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
} // namespace openblack::ecs::components

namespace openblack::ecs::effects::reactions
{
struct Reaction;
} // namespace openblack::ecs::effects::reactions

// The villagers and fire: REACT_TO_FIRE (215, the REACT_TO_FIRE reaction), PUT_OUT_FIRE_BY_BEATING (216), ON_FIRE
// (219, burning or fleeing a fire) and MOVE_AROUND_FIRE (220). With water (217, 218) they give up at once in W120.
// The fire a villager deals with and the object it reacts to are kept by entity. Wiki: docs/bw1-notes/magic.md, "Fire".

namespace openblack::ecs::villager_fire
{
/// The fire (FireEffect id) it flees from or fights; 0 none
[[nodiscard]] uint32_t FireOf(entt::entity villager);
/// Its final state's exit function is ExitPutOutFire (216..218, 220) and nothing else, or it is REACT_TO_FIRE (215).
/// Fighting villagers are not heated by the fire.
[[nodiscard]] bool IsFireMan(entt::entity object);
/// The final state is ON_FIRE (219)
[[nodiscard]] bool IsInOnFireState(entt::entity villager);
/// Unless in the hand or thrown, it keeps its state and destination, then ON_FIRE with the fire that heats it (0: its
/// own)
void SetupOnFire(entt::entity villager, uint32_t fire);
/// A fire group dissolves
void StopFireFighting(entt::entity villager);
/// The reaction's start function for REACT_TO_FIRE
void SetupReactToFire(entt::entity villager, entt::entity object, uint32_t reaction);
/// The reaction's priority function: 0 = don't
[[nodiscard]] uint8_t ReactToFirePriority(entt::entity villager, uint32_t reaction, uint32_t currentReaction);

// the state table entries (LivingActionSystem.cpp k_VillagerStateTable)
uint32_t ReactToFire(components::LivingAction& action);         ///< 215
uint32_t PutOutFireByBeating(components::LivingAction& action); ///< 216
uint32_t PutOutFireWithWater(components::LivingAction& action); ///< 217 / 218: DECIDE_WHAT_TO_DO
uint32_t OnFire(components::LivingAction& action);              ///< 219
uint32_t MoveAroundFire(components::LivingAction& action);      ///< 220
// their rows' entry and exit functions (EnterPutOutFire / ExitPutOutFire on 216, 217, 218 and 220, EnterOnFire /
// ExitOnFire on 219), with the table's codes: entry 1 = accepted, 0 = refused (0x2F); exit 1 = it may leave. The
// villager core's state changes (ECS/Villager/VillagerCore.h) call them, once each
/// (final, next): 1 for a change between fire-fighting states, or with a live fire and reaction when it was not a
/// fireman yet (then it is); else 0
uint32_t EnterPutOutFire(components::LivingAction& action, VillagerStates final, VillagerStates next);
uint32_t ExitPutOutFire(components::LivingAction& action, VillagerStates next); ///< always 1
/// (final, next): 0 only if it is already a fireman of its fire, else 1
uint32_t EnterOnFire(components::LivingAction& action, VillagerStates final, VillagerStates next);
uint32_t ExitOnFire(components::LivingAction& action, VillagerStates next); ///< always 1, the fire is cleared

/// The REACT_TO_FIRE part of applying a reaction to the living of a cell, for a villager of a cell the reaction
/// reaches (ECS/Effects/Reactions spreads it once, when it is made; the original's respreading is off: its flag is
/// never set)
void ApplyReaction(entt::entity villager, const effects::reactions::Reaction& reaction);

/// Whether it follows the fire's reaction (REACT_TO_FIRE)
[[nodiscard]] bool IsReacting(entt::entity villager);
/// For the fire's reaction: the object it reacts to (entt::null when none)
[[nodiscard]] entt::entity ReactionObject(entt::entity villager);
/// Stops the fire's reaction kept here: its record gets the turn, the reaction and the object are cleared
/// (villager_reactions::StopReacting calls it)
void StopReacting(entt::entity villager);

/// The reaction's shut-down for the villagers that follow `reaction`: each one stops reacting and resets its state,
/// before the reaction goes. Called by the fire when it removes its reactions (the fire cooled below its reaction
/// temperature, went or moved)
void ShutDownReaction(uint32_t reaction);

/// A land is loaded (also registers the villagers' reaction handler)
void Clear();
} // namespace openblack::ecs::villager_fire
