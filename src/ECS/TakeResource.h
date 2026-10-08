/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/fwd.hpp>

#include "ECS/PotResource.h"

// Taking a resource: a structure takes the object given to it (by the hand) and the object goes. Its callers ask
// whether the receiver is a resource store first: a tree put down on a store (within a building site) and a tree
// that hit one (the interface is the physics object's). A plain object takes nothing; the storage pit and the
// worship site (here and worship::site::DeleteObjectAndTakeResource) wrap the shared
// object_delivery::DoDeleteObjectAndTakeResource (the resource moved and the object deleted: ECS/ObjectDelivery.h).
// The other receivers (MultiMapFixed, Pot, Scaffold, Workshop) are (pending).

namespace openblack::ecs::take_resource
{
/// The same code in the storage pit and the worship site: the object is in the physics with a physics object whose
/// interface is the local one -> the help profile's SUPPLY (6) trigger. The receiver's own `is` is not what is
/// tested. openblack: PhysicsObjects::Find and its byPlayer (the local hand threw it, directly or through what it
/// hit; (approximate) one local interface, so "the hand's" is the local interface).
/// TODO(Intro HEAD): the trigger is only logged until help_profile::Trigger lands (TakeResource.cpp).
void TriggerSupplyHelpIfThrownByMe(entt::entity object);

/// A storage pit takes the object: the player of `is` (none without an interface, read before anything else),
/// TriggerSupplyHelpIfThrownByMe(object), DoDeleteObjectAndTakeResource(object, is), then a reaction 22
/// REACT_TO_HAND_PUTTING_STUFF_IN_STORAGE_PIT from the store with that player (its result is not used). Returns true.
bool StoragePit(entt::entity store, entt::entity object, const pot_resource::Dropper& is);
} // namespace openblack::ecs::take_resource
