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

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{
struct LivingAction;
} // namespace openblack::ecs::components

namespace openblack::ecs::effects::reactions
{
struct Reaction;
} // namespace openblack::ecs::effects::reactions

// The villagers and the teleport stones: a walking villager that the stone's REACT_TO_TELEPORT reaction reaches, and
// for which another stone of its player is worth the detour, walks to the stone (GO_TOWARDS_TELEPORT_REACTION 201 /
// _QUICKLY 251), jumps (TELEPORT_REACTION 202) and resumes what it was doing. Magic/Objects/MagicTeleport does the
// jump. Wiki: docs/bw1-notes/miracles.md, "Teleport".

namespace openblack::ecs::villager_teleport
{
/// Whether the living thing moved during the last turn (its position differs from the turn before's). (approximate)
/// openblack keeps no previous-turn position: a move state that is not ARRIVED plus a speed stands for it, whatever
/// the state is.
[[nodiscard]] bool IsMoving(entt::entity living);
/// The final destination: the wall hug's goal, moving or not; (pending) the footpath branch has no openblack
/// equivalent. MapCoords as metres (y 0).
[[nodiscard]] glm::vec3 FinalDestination(entt::entity living);
/// The reaction it follows, as far as the stones care: the REACT_TO_TELEPORT one, 0 none
[[nodiscard]] uint32_t CurrentReaction(entt::entity living);
/// The villager's player: its town's owner
[[nodiscard]] std::optional<PlayerNames> PlayerOf(entt::entity villager);

/// (MagicTeleport's ShouldLivingThingReact ? 0xFF : 0) & the priority of REACT_TO_TELEPORT (the ReactionInfo table)
[[nodiscard]] uint8_t ReactToTeleportPriority(entt::entity villager, uint32_t reaction);
/// The stone registers its final destination, the stone is kept as the reaction's object, and it reacts with
/// GO_TOWARDS_TELEPORT_REACTION
void SetupReactToTeleport(entt::entity villager, entt::entity stone, uint32_t reaction);

// the state table entries (LivingActionSystem.cpp k_VillagerStateTable)
uint32_t GoToTeleportReaction(components::LivingAction& action); ///< 201 (251 runs it too)
uint32_t TeleportReaction(components::LivingAction& action);     ///< 202
/// The exit of 201, 202 and 251: unless IsStateExitFunctionSameAs(next), off its town's way-to-worship list and the
/// on-way flag cleared; then ExitReaction
uint32_t ExitReactToTeleport(components::LivingAction& action, VillagerStates next);
/// Reacting to the teleport's reaction (REACT_TO_TELEPORT)
[[nodiscard]] bool IsReacting(entt::entity villager);
/// The teleport reaction's object: the stone it goes to (entt::null when none)
[[nodiscard]] entt::entity ReactionObject(entt::entity villager);
/// StopReacting for the teleport's reaction kept here: its record gets the turn, the reaction and the stone are cleared
/// (villager_reactions::StopReacting calls it)
void StopReacting(entt::entity villager);

/// The REACT_TO_TELEPORT part of applying a reaction to the living things of a cell, for a villager of a cell the
/// reaction reaches (spread once by ECS/Effects/Reactions, when the stone is made: the reactions are never
/// respread)
void ApplyReaction(entt::entity villager, const effects::reactions::Reaction& reaction);

/// The villager side before the jump: FLYING, put down at the stone, LANDED, DecideWhatToDo
void LandAt(entt::entity villager, const glm::vec3& mapPosition);
/// Sets DECIDE_WHAT_TO_DO as the top state
void DecideWhatToDo(entt::entity villager);
/// After the teleport moves it in the map: a walk in progress goes on from the new position
void OnMoved(entt::entity living);

/// A land is loaded (also registers the villagers' reaction handler)
void Clear();
} // namespace openblack::ecs::villager_teleport
