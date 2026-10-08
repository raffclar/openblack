/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// The Villager handler of ECS/Effects/Reactions: applies a reaction to a villager of a cell, by reaction type (the
// ported ones: REACT_TO_FIRE in VillagerFire.cpp, REACT_TO_TELEPORT in VillagerTeleport.cpp; the rest reach no
// villager yet).

// Also the villager's side of a reaction's end that the reaction states share (the reaction it follows and its
// object: kept by VillagerFire.cpp and VillagerTeleport.cpp for their reaction).
//
// The types of the villager type table go through the original's per-villager logic with one shared slot
// (components::VillagerReactionSlot: the reaction and its object): the body that applies a reaction to a villager
// (availability, the score, the records, the switch rule), AddReaction, ProcessReaction and the reaction's
// shut-down. Only 7 REACT_TO_FOOD and 12 REACT_TO_WOOD are registered (VillagerResourceReactions.h); the miracle types
// (10, 20, 13 and 23) keep their maps and code: they are only scored as "the current reaction" when a slot type meets
// them, and the slot and those maps are never held at once.

#include <cstdint>

#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>

#include "ECS/Components/LivingAction.h"
#include "Enums.h"

namespace openblack::ecs::effects::reactions
{
struct Reaction;
} // namespace openblack::ecs::effects::reactions

namespace openblack::ecs::villager_reactions
{
/// SetLivingReactionHandler(Villager, ...) and SetLivingShutDownHandler(Villager, ...) (the slot holders' side of a
/// reaction's shut-down), once the game's reactions exist (before any map load)
void RegisterHandlers();
/// RegisterHandlers and the join order back to 0: at every map load (villager_fire::Clear, villager_teleport::Clear)
void Register();

/// SetTopState for the reactions' states: the villager core's. (approximate) MOVE_TO_POS' exit function is not
/// ported, so the walk a change leaves is ended here (openblack's move tags), unless the exit refused the change
/// (0x2E: nothing changed)
uint32_t SetTopState(entt::entity villager, VillagerStates state);
/// SetTopState of the stored state's resume state (field0x20); 0x2E -> a raw store of 163 in index 0; then a raw store
/// of 0 in index 2
void PopFromPrevious(entt::entity villager);
/// PopFromPrevious, then SetTopState(163) if the final state is a reactive state (field0xb8)
void ResetStateAfterReacting(entt::entity villager);
/// ResetStateAfterReacting, then StopReacting if still reacting
void StopReactingAndSetState(entt::entity villager);
/// It follows a reaction (the fire's, the teleport's, the shield's, the mourning's or a slot type's)
[[nodiscard]] bool IsReacting(entt::entity villager);
/// The reaction's record gets the turn, the reaction and its object are cleared (the miracle maps and the slot)
void StopReacting(entt::entity villager);
/// The validate slot of the state table for the reaction rows (201, 202, 251, 215-218, 220, 6-30, 140-146; run each
/// turn for the TOP and the stored state, before the state itself). PopFromPrevious when the reaction's object is
/// none or not available, or when the reaction's info row says whetherReactionFinishesIfInitiatorInHand and the
/// object is in the hand. The original returns nothing; this returns false when it popped. The signature is the
/// table's `validate` column (ECS/Villager/VillagerStateTable.h); LivingActionSystem::VillagerCallValidate calls it
/// for every row with no validate of its own whose original validate is this one (VillagerOriginalFns.h).
bool ReactionValidate(components::LivingAction& action);
// ---- the slot types ---------------------------------------------------------------------------------------------

/// One row of the villager type table for a slot type (the villager's functions for that type)
struct SlotType
{
	bool sameTypeSwitch {false}; ///< effects::reactions::SameTypeSwitch
	/// What starting to react calls (initiator, reaction): the type's Setup*
	void (*setup)(entt::entity villager, entt::entity object, uint32_t reaction) {nullptr};
	/// The score's priority (reaction, other): 0 = don't
	uint8_t (*priority)(entt::entity villager, uint32_t reaction, uint32_t other) {nullptr};
	/// ProcessReaction's limit (object, type, distance)
	uint32_t (*turnsToReact)(entt::entity villager, entt::entity object, uint8_t type, float distance) {nullptr};
	/// The turns before reacting again (initiator, type, distance)
	uint32_t (*turnsBeforeAgain)(entt::entity villager, entt::entity object, uint8_t type, float distance) {nullptr};
};
/// The Villager handler of effects::reactions (SetLivingReactionHandler): applies a reaction to one villager of a cell
/// at the spread's distance d. A slot type -> its per-villager body; else, with no slot held, the miracle dispatch as
/// before (fire / teleport / shield / death); a miracle type meeting a slot holder -> the switch rule first. Public for
/// the tests
void HandleReaction(entt::entity villager, const effects::reactions::Reaction& reaction, float distance);
/// The row of a type, nullptr when it is not a slot type
[[nodiscard]] const SlotType* SlotTypeOf(openblack::Reaction type);
/// The villager follows a slot reaction
[[nodiscard]] bool SlotHeld(entt::entity villager);
/// The slot's reaction (0 none)
[[nodiscard]] uint32_t SlotReaction(entt::entity villager);
/// The slot's object (entt::null none)
[[nodiscard]] entt::entity SlotObject(entt::entity villager);
/// The setups store the object, after AddReaction
void SetSlotObject(entt::entity villager, entt::entity object);
/// (reaction, state): how impressed it is (TODO(reactions): waits for the town belief), then: no reaction yet ->
/// StorePreviousState; SetTopState(state); dancing -> out of the dance (TODO(dance)); the reaction is stored (a miracle
/// map the villager held is dropped here without refreshing its record, as the original only unlinks on a switch);
/// the follower link
void AddReaction(entt::entity villager, uint32_t reaction, VillagerStates state);
/// For a slot holder (villager::ProcessReaction calls it before ProcessState): the reaction gone -> StopReacting; the
/// object none or not available -> StopReactingAndSetState; turns since its record (signed) > the type's
/// turnsToReact(object, type, the distance to the object) -> StopReactingAndSetState. Nothing for the miracle maps
/// (unchanged)
void ProcessSlotReaction(entt::entity villager);
/// The reaction score for a villager: 0 unless the villager info's isReacting[type] and d <= maxReactionDistance;
/// only then the type's priority (reaction, other) and effects::reactions::Score. The miracle types score with their
/// own priority functions (VillagerFire / VillagerTeleport / VillagerShield / VillagerMourning)
[[nodiscard]] uint32_t ScoreOf(entt::entity villager, uint32_t reaction, openblack::Reaction type, float distance,
                               uint32_t other);

/// The reaction rows' exit function: the circle hug reset, and StopReacting unless `next` is a reactive state. Always 1
/// (it may leave)
uint32_t ExitReaction(components::LivingAction& action, VillagerStates next);
} // namespace openblack::ecs::villager_reactions
