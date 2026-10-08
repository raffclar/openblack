/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <entt/fwd.hpp>

#include "ECS/PotResource.h"
#include "Enums.h"

// Which objects store a resource, and what an object given to one of them is worth. The hand asks a store before it
// gives it a held object (on the press, or when a thrown body hits it); the store then takes the object's resource and
// the object goes (ecs::take_resource, ecs::object_delivery). Wiki: docs/bw1-notes/objects-and-resources.md, "Giving
// an object to a store".

namespace openblack::ecs::resource_stores
{
/// The classes that answer whether they store a resource
enum class StoreKind : uint8_t
{
	None,          ///< any other object: never a store
	StoragePit,    ///< a storage pit: any type
	StructurePile, ///< a pile that is part of a storage pit or a workshop: asks its structure, for its own type
	LoosePot,      ///< a pot or a pile of no structure: never a store
	Abode,         ///< any other building: wood while it has a building site
	Workshop,      ///< wood (and any)
	WorshipSite,   ///< food (and any), wood while it has a building site
};

/// Whether a store of `kind` takes `type`. `hasBuildingSite` is the building's own site (abodes, workshops and worship
/// sites); a structure's pile is answered by PileIsStoreForType
[[nodiscard]] bool IsStoreForType(StoreKind kind, ResourceType type, bool hasBuildingSite);
/// A pile that is part of a structure: the structure takes the type, and the type is the pile's own or any
[[nodiscard]] bool PileIsStoreForType(bool structureTakesType, ResourceType type, ResourceType pileType);

/// The food an animal is worth: its info's food value, truncated, when its food type is meat or vegetable; none else
[[nodiscard]] uint32_t AnimalFood(uint32_t foodTypeBits, float foodValue);
/// The wood a fence is worth: its info's wood value x its life x its scale cubed, in float, truncated
[[nodiscard]] uint32_t FenceWood(uint32_t woodValue, float life, float scale);

/// The class of `object` as a store
[[nodiscard]] StoreKind KindOf(entt::entity object);
/// Whether `object` stores `type` (the table of IsStoreForType, a pile asking its structure)
[[nodiscard]] bool IsResourceStore(entt::entity object, ResourceType type);
/// The store takes the object given to it and the object goes: a storage pit through its own handler (the supply help,
/// the delivery, its reaction), a worship site through its own, a pot or a pile, a workshop or a building with a
/// building site through the plain delivery. Returns false when `store` takes nothing given (the object stays)
bool DeleteObjectAndTakeResource(entt::entity store, entt::entity object, const pot_resource::Dropper& dropper);
} // namespace openblack::ecs::resource_stores
