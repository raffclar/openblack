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

#include "ECS/Villager/VillagerDeath.h"

namespace openblack::ecs::events
{
/// A death asks for help sprites and sounds
struct VillagerDeathHelp
{
	entt::entity villager {entt::null};
	entt::entity town {entt::null}; ///< the dead villager's town, entt::null without one
	villager::DeathHelp help {};
};

/// A dead villager's smoke puff and soul: on its first dead turn, and when its body vanishes (smoke only)
struct VillagerDeadEffects
{
	entt::entity villager {entt::null};
	villager::DeadEffects effects {};
};
} // namespace openblack::ecs::events
