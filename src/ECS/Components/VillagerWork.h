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

/// What a villager is working at: the fish farm a fisherman fishes, or the tree a forester has just felled and watches
/// come down
struct WorkTarget
{
	entt::entity thing {entt::null};
};

/// The wood a villager reacted to and is going for, kept while it waits beside wood that still moves
struct WoodReactionTarget
{
	entt::entity wood {entt::null};
	/// The turn it took up the wood, after which it waits only so long for the wood to come to rest
	uint32_t startTurn {0};
};

} // namespace openblack::ecs::components
