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

#include "ECS/Components/LivingReaction.h"
#include "ECS/Registry.h"

namespace openblack::ecs::creature_reaction_notice
{

/// Whether a creature takes notice of reactions: true until something tells it to take no notice
[[nodiscard]] inline bool TakesNotice(const Registry& registry, entt::entity creature)
{
	return !registry.AllOf<components::CreatureReactionsOff>(creature);
}

/// A creature starts or stops to take notice of reactions, as a script, a swap of minds or the debug tools tell it
inline void SetTakesNotice(Registry& registry, entt::entity creature, bool notice)
{
	if (notice)
	{
		registry.Remove<components::CreatureReactionsOff>(creature);
	}
	else
	{
		registry.AssignOrReplace<components::CreatureReactionsOff>(creature);
	}
}

} // namespace openblack::ecs::creature_reaction_notice
