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

#include "ECS/Components/LivingAction.h"
#include "Enums.h"

namespace openblack::ecs::effects::reactions
{
struct Reaction;
}

// The neighbours' mourning of a dead villager and its orphans: REACT_TO_DEATH (23), created when a villager dies,
// spreads once over 60 m; ReactToDeathPriority and SetupReactToDeath start the villagers' states 205
// POINT_AT_DEAD_PERSON, 206 GO_TOWARDS_DEAD_PERSON, 207 LOOK_AT_DEAD_PERSON and 208 MOURN_DEAD_PERSON. A dead mother's
// children go to 131 MORN_DEATH (FindChildrenAndOrphanThem). The reaction followed and the dead villager are kept here,
// as VillagerFire / VillagerShield keep theirs.

namespace openblack::ecs::villager_mourning
{
// ---- the pure layer ----------------------------------------------------------------------------------------------

/// The facing turns: truncate((GameFloatRand(20) + 2) x (1000 / msPerTurn = 100, an unsigned division))
[[nodiscard]] int32_t PointTurns(float roll20);
/// The mourning turns: truncate((GameFloatRand(3) + 4) x (1000 / 100))
[[nodiscard]] int32_t MournTurns(float roll3);

// ---- the reaction ------------------------------------------------------------------------------------------------

/// A creature initiator -> REACT_TO_DEATH's priority; else my town has a functional graveyard, I am the dead one, or 10
/// Livings already took it -> 0; else the priority
[[nodiscard]] uint8_t ReactToDeathPriority(entt::entity villager, uint32_t reaction);
/// A creature initiator -> AddReaction(r, 7 LOOKING_AT_OBJECT_REACTION); else GameRand(2): 0 -> the state counter = 0,
/// AddReaction(r, 205); 1 -> AddReaction(r, 206); then the dead villager is kept
void SetupReactToDeath(entt::entity villager, entt::entity dead, uint32_t reaction);
/// The REACT_TO_DEATH part of applying a reaction to the living objects of a cell, for one villager of the cell (the
/// fire's, teleport's and shield's rules: available, not reacting, within maxReactionDistance, the reaction score, not
/// reacted lately), then SetupReactToDeath
void ApplyReaction(entt::entity villager, const effects::reactions::Reaction& reaction);
/// It follows a REACT_TO_DEATH
[[nodiscard]] bool IsReacting(entt::entity villager);
/// The dead villager it mourns
[[nodiscard]] entt::entity ReactionObject(entt::entity villager);
/// Stops the REACT_TO_DEATH kept here: its record gets the turn, and the reaction and the dead villager go
void StopReacting(entt::entity villager);

// ---- the states --------------------------------------------------------------------------------------------------

/// 205 POINT_AT_DEAD_PERSON
uint32_t PointAtDeadPerson(components::LivingAction& action);
/// 206 GO_TOWARDS_DEAD_PERSON
uint32_t GoTowardsDeadPerson(components::LivingAction& action);
/// 207 LOOK_AT_DEAD_PERSON
uint32_t LookAtDeadPerson(components::LivingAction& action);
/// 208 MOURN_DEAD_PERSON
uint32_t MournDeadPerson(components::LivingAction& action);
/// The exit of 205-208: villager_reactions::ExitReaction, which stops this reaction too unless `next` is a reactive
/// state. Always 1
uint32_t ExitReaction(components::LivingAction& action, VillagerStates next);
/// The validate of 205-208 for REACT_TO_DEATH: the dead villager gone or not available, or in the hand (row 23
/// finishesIfInitiatorInHand 1) -> PopFromPrevious
bool ReactionValidate(components::LivingAction& action);

// ---- orphans -----------------------------------------------------------------------------------------------------

/// Every villager of the town's structures (their inhabitants) and of its homeless list whose mother is `mother` ->
/// MakeChildOrphaned
void FindChildrenAndOrphanThem(entt::entity mother);
/// The child's mother is not her -> 0; IsVillagerAvailable -> SetTopState(131 MORN_DEATH); mother = 0; 1
uint32_t MakeChildOrphaned(entt::entity child, entt::entity mother);

/// A new map: no mourners
void Clear();
} // namespace openblack::ecs::villager_mourning
