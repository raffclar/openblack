/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>

#include "ECS/Registry.h"
#include "Locator.h"

// Two bits of an object's flags that the scripts set: SET_ID_MOVEABLE 168 and SET_ID_PICKUPABLE 169 set the bit when
// their bool is 0 and clear it otherwise. They are also set by the saved game's load and the puzzles (the Hanoi
// blocks), not ported. Kept as tag components.
// Wiki: docs/bw1-notes/hand-and-interface.md, "Grabbing".
namespace openblack::ecs::components
{
/// The object cannot be moved (the immovable flag)
struct Immovable
{
};
/// The object cannot be picked up
struct CannotBePickedUp
{
};
} // namespace openblack::ecs::components

namespace openblack::ecs::object_flags
{

/// Whether the object cannot be picked up (in the original only the Hanoi blocks, not ported, answer otherwise)
[[nodiscard]] inline bool IsCannotBePickedUp(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(object) && registry.AllOf<components::CannotBePickedUp>(object);
}

/// SET_ID_PICKUPABLE 169: pickupable 0 sets the flag, anything else clears it
inline void SetPickupable(entt::entity object, bool pickupable)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		return;
	}
	if (pickupable)
	{
		registry.Remove<components::CannotBePickedUp>(object);
	}
	else
	{
		registry.AssignOrReplace<components::CannotBePickedUp>(object);
	}
}

/// The immovable flag (an object in physics or immovable gets no physics)
[[nodiscard]] inline bool IsImmovable(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(object) && registry.AllOf<components::Immovable>(object);
}

/// SET_ID_MOVEABLE 168: moveable 0 sets the flag, anything else clears it
inline void SetMoveable(entt::entity object, bool moveable)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		return;
	}
	if (moveable)
	{
		registry.Remove<components::Immovable>(object);
	}
	else
	{
		registry.AssignOrReplace<components::Immovable>(object);
	}
}

} // namespace openblack::ecs::object_flags
