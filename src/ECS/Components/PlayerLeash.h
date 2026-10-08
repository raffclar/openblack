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

#include "Creature/LeashOwnership.h"

namespace openblack::ecs::components
{

/// On a player's entity: the last leash the player was refused, on which creature (none when they had no creature to
/// lead) and why. The leash service gives it at the player's first refusal and keeps it for the debug windows.
struct PlayerLeashRefusal
{
	entt::entity creature {entt::null};
	creature_leash::Refusal why {creature_leash::Refusal::None};
};

} // namespace openblack::ecs::components
