/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptPoison.h"

#include <algorithm>

#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Poisoned.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "Enums.h"

namespace openblack::ecs::script_poison
{

namespace
{
/// Whether a thing is a living thing, which carries the mark of poison
bool IsLiving(const Registry& registry, entt::entity thing)
{
	return registry.AllOf<components::Villager>(thing) || registry.AllOf<components::Animal>(thing) ||
	       registry.AllOf<components::Creature>(thing);
}

/// The pot of a pile that a storage pit has, none when the pit has no such pile
components::Pot* PileOf(Registry& registry, entt::entity pile)
{
	return pile != entt::null && registry.Valid(pile) ? registry.TryGet<components::Pot>(pile) : nullptr;
}

bool PileIsPoisoned(const Registry& registry, entt::entity pile)
{
	if (pile == entt::null || !registry.Valid(pile))
	{
		return false;
	}
	const auto* pot = registry.TryGet<const components::Pot>(pile);
	return pot != nullptr && pot->poisoned;
}
} // namespace

bool IsPoisoned(const Registry& registry, entt::entity thing)
{
	if (thing == entt::null || !registry.Valid(thing))
	{
		return false;
	}
	if (IsLiving(registry, thing))
	{
		return registry.AllOf<components::Poisoned>(thing);
	}
	if (const auto* pit = registry.TryGet<const components::StoragePit>(thing))
	{
		return PileIsPoisoned(registry, pit->foodPile) ||
		       std::ranges::any_of(pit->woodPiles, [&](entt::entity pile) { return PileIsPoisoned(registry, pile); });
	}
	if (const auto* pot = registry.TryGet<const components::Pot>(thing))
	{
		return pot->poisoned;
	}
	if (const auto* mobile = registry.TryGet<const components::MobileObject>(thing))
	{
		return mobile->type == MobileObjectInfo::Toadstool;
	}
	return false;
}

void SetPoisoned(Registry& registry, entt::entity thing, uint32_t value)
{
	if (thing == entt::null || !registry.Valid(thing))
	{
		return;
	}
	if (IsLiving(registry, thing))
	{
		if ((value & 1u) != 0u)
		{
			registry.AssignOrReplace<components::Poisoned>(thing);
		}
		else if (registry.AllOf<components::Poisoned>(thing))
		{
			registry.Remove<components::Poisoned>(thing);
		}
		return;
	}
	if (const auto* pit = registry.TryGet<const components::StoragePit>(thing))
	{
		// Only the food pile changes. A value of 0 cures the pile, so a script can cure the store (deviation 179).
		if (auto* pile = PileOf(registry, pit->foodPile))
		{
			pile->poisoned = value != 0u;
		}
		return;
	}
	if (auto* pot = registry.TryGet<components::Pot>(thing))
	{
		pot->poisoned = (value & 1u) != 0u;
	}
}

} // namespace openblack::ecs::script_poison
