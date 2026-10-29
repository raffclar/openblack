/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureCarried.h"

#include "Creature/CreatureObjectActions.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Registry.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

std::optional<entt::entity> creature_carried::CarriedBy(const Registry& registry, entt::entity creature)
{
	const auto* held = registry.Valid(creature) ? registry.TryGet<const CreatureHeldObject>(creature) : nullptr;
	if (held == nullptr || !registry.Valid(held->object))
	{
		return std::nullopt;
	}
	const auto* action = registry.TryGet<const CreatureObjectAction>(creature);
	const bool carries = action == nullptr
	                         ? creature_object_actions::CarriesHeld(std::nullopt, {}, false)
	                         : creature_object_actions::CarriesHeld(action->kind, action->status, action->eventDone);
	return carries ? std::optional(held->object) : std::nullopt;
}

std::optional<entt::entity> creature_carried::CarrierOf(const Registry& registry, entt::entity thing)
{
	const auto* holder = registry.Valid(thing) ? registry.TryGet<const HeldByCreature>(thing) : nullptr;
	if (holder == nullptr || CarriedBy(registry, holder->creature) != thing)
	{
		return std::nullopt;
	}
	return holder->creature;
}

bool creature_carried::Carries(const Registry& registry, entt::entity creature, entt::entity thing)
{
	return CarriedBy(registry, creature) == thing;
}
