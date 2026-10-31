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

namespace openblack::ecs::components
{

/// On a player entity: the two hands of the player's god. The world finds the hands of a player from this link. A save
/// of an older build has no link, and the load makes it from the hands of each player.
struct PlayerHands
{
	entt::entity left {entt::null};
	entt::entity right {entt::null};
};

} // namespace openblack::ecs::components
