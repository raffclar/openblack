/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/fwd.hpp>

namespace openblack::ecs
{

/// The common "this object goes" per class (the physics' deletion, a sunk animal, a drowned villager...): the class's
/// own clean-up, then out of the physics and out of the registry.
///
/// - a villager: villager::ToBeDeletedOverride (its dependants, then it stops reacting; ECS/Villager/VillagerDeath.h);
/// - an animal: out of its town's list, its town cleared, then out of its flock; prey and hunter links are not cleared
///   (their readers ask IsAvailable). (pending) the town list and the town reset; animal_ai::Forget only leaves the
///   flock yet;
/// - a tree: out of its forest and of the game's tree list; in openblack a tree only carries its forest id, so nothing
///   is left to unlink;
/// - every object: the rest; its fire is deleted after the mark, trees and dead trees too.
///
/// Then: already unavailable, nothing; `now`, deleted at once; else marked Unavailable and put at the head of the dead
/// list, freed by ProcessDeadList; its physics body stays until the physics' turn update drops the unavailable ones.
/// (pending) Until every owner's readers ask IsAvailable the deferral is off (SetDeferredDeletion) and the entity goes
/// at once (out of the physics, then destroyed), as before. Trees go through DeleteTree at once either way (pending:
/// its unlinking and its destruction are one function, Trees.cpp).
void ToBeDeleted(entt::entity entity, bool now = false);

/// A valid entity not marked Unavailable (entt::null: false)
[[nodiscard]] bool IsAvailable(entt::entity entity);

/// From the head (the last marked first), each thing marked before this pass; the first pass only notes it, the next
/// one frees it. `drain` (closing the game, clearing the map, loading a game) frees them all, again until the list is
/// empty. The turn calls it with false. Nothing to do while the deferral is off.
void ProcessDeadList(bool drain);

/// (openblack) on once every owner's readers are zombie-safe
void SetDeferredDeletion(bool deferred);
[[nodiscard]] bool DeferredDeletion();

} // namespace openblack::ecs
