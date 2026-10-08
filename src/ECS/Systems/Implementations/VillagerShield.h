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

// The villagers and the shield spells: a villager the REACT_TO_MAGIC_SHIELD reaction of a SpellShield reaches walks
// under the dome (SetupMoveToWithHug to a point at 0.8 R of its own bearing) and stands there amazed, looking outwards
// (AMAZED_BY_MAGIC_SHIELD_REACTION 168), until the shield or its town's need for it goes. Magic/Spells/SpellShield
// makes the reaction (radius R + 30) when the spell is cast; it is spread once, so only the villagers near it at that
// moment get the chance to react.
//
// The town gates read the town's desire for protection (GetDesireSignificanceToVillager, through ecs::town_desire) and
// the turn of the town's last aggressor. Two things that feed them are still missing, so in a normal game only the
// homeless react (see ReactToMagicShieldPriority):
//  - the protection desire's input comes from the player's interaction with the town, which is not ported
//    (town_desire's DesireInputs::protection is 0): only a script boost makes it above its trigger;
//  - of the town's aggressor update only its two record fields are ported (components::Town::aggressor*), written by
//    the physical shield's impacts; nothing else makes a town's aggressor yet.
// OPENBLACK_TEST_SHIELD_REACTION=1 takes both gates as "the town wants protection and was just attacked" so the
// reaction can be seen in game; without it the gates are the original's reads. Wiki: docs/bw1-notes/miracles.md, "Shields".

namespace openblack::ecs::villager_shield
{
/// The priority of REACTION_REACT_TO_MAGIC_SHIELD (from its ReactionInfo row) when the reaction's initiator is an
/// available SpellShield and either the villager has no town or its town wants protection and was attacked less than
/// numGameTurnsAfterAggressionInterestedInShield turns ago; else 0
[[nodiscard]] uint8_t ReactToMagicShieldPriority(entt::entity villager, uint32_t reaction);

/// AddReaction(reaction, 168), remembers the spell, and, when the villager is not already under the shield minus 0.2 R
/// (IsUnder), a walk to shield + GetPosFromAngle(the bearing shield -> villager +- pi / 8, 0.8 R (1 - rand^3)) with
/// final state 168. Then, either way, the look-at point 1 m beyond the villager on that same bearing: it faces away
/// from the dome
void SetupReactToMagicShield(entt::entity villager, entt::entity spell, uint32_t reaction);

/// The state function of AMAZED_BY_MAGIC_SHIELD_REACTION (168): turns
/// towards the look-at point, re-aims it once facing (1 in 4), and keeps the state's clip for 20..30 s before rolling
/// another one (IntoPointing goes to TalkingAndPointing after one cycle). With no town, no shield or no need for
/// protection it waits GameRand(60) + 20 turns (WAIT_FOR_COUNTER) and then decides what to do
uint32_t AmazedByMagicShieldReaction(components::LivingAction& action);

/// The REACT_TO_MAGIC_SHIELD part of applying a reaction to the living objects of a cell, for one villager
void ApplyReaction(entt::entity villager, const effects::reactions::Reaction& reaction);

/// It follows a shield reaction
[[nodiscard]] bool IsReacting(entt::entity villager);
/// The shield spell of the reaction it follows (entt::null: none)
[[nodiscard]] entt::entity ReactionObject(entt::entity villager);
/// ReactionValidate's availability test for the reaction's shield spell: still a live SpellShield
[[nodiscard]] bool IsReactionObjectAvailable(entt::entity villager);
/// StopReacting for the shield reaction: its record gets the turn, and the reaction and its spell are cleared
void StopReacting(entt::entity villager);
/// A land is loaded
void Clear();
} // namespace openblack::ecs::villager_shield
