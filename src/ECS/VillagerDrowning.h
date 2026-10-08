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

namespace openblack::ecs
{
namespace components
{
struct LivingAction;
}

/// Sinking and drowning, per class, like the original:
///
/// On every physics substep, when the body is awake, its centre is under half its radius and its density is over 1,
/// the object is asked whether it has sunk (HasSunk); if it has, the body stops (v = 0, L = 0)
/// and EndPhysics runs as if it had come to rest. The villager then stands in the sea in DROWNING (16) for
/// GVillagerInfo::drowningTime turns (600 = 60 s) and dies (DEATH_REASON_PLAYER_INTERACTION_DROWN). The drowning
/// villager's clip is the state's (info.dat: 252 P_DROWNING, whose events play the swim splash 157 and the drowning
/// scream 134 through ECS/AnimationSounds).

/// Whether a sinking body has sunk, by kind:
/// - a plain object: no (rocks, trees, pots... sink on until the -4R deletion, ToBeDeleted);
/// - an animal: SetDying, LIVING_DEAD and ToBeDeleted(0) -> the entity is gone;
/// - a villager: stateCounter = drowningTime and DROWNING (16).
/// A villager that can still be reached first tells the creature of the player whose hand last dropped it that it was
/// thrown in the sea (ECS/CreatureMimic.h). (pending) The animal's own report: who last dropped an animal is not kept.
bool HasSunk(entt::entity entity);

/// The water case of a villager's end of physics: the villager came to rest (or sank) on a cell
/// with the water bit (MapCoords::IsWater, the shallow shore too): alive -> stateCounter = drowningTime and DROWNING;
/// otherwise VillagerDead(DEATH_REASON_PLAYER_INTERACTION_DROWN). No LANDED.
void VillagerEndPhysicsInWater(entt::entity villager);
/// lastPlayerToInteract from the body that ends in the water: the player of the hand that dropped or threw it (byPlayer:
/// the local hand player PLAYER_ONE, inferred; else none). Read by the DROWNING death's VillagerDead.
void RememberLastPlayerToInteract(entt::entity villager, bool byPlayer);

/// The DROWNING state's function, once per game turn: --stateCounter, and at 0
/// VillagerDead(DEATH_REASON_PLAYER_INTERACTION_DROWN, the player who dropped it or lastPlayerToInteract, 0.01, true).
uint32_t VillagerDrowningState(components::LivingAction& action);

/// Whether the entity is drowning, the script's GET_PROPERTY(DROWNING):
/// - a villager: its state is DROWNING (16);
/// - another object: it has a physics body and the body's centre is under y 0;
/// - a thing that is not an object: no.
[[nodiscard]] bool IsDrowning(entt::entity entity);

} // namespace openblack::ecs
