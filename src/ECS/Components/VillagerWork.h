/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{

/// What a villager is working at: the fish farm a fisherman fishes, the field a farmer farms, or the tree a forester has
/// just felled and watches come down
struct WorkTarget
{
	entt::entity thing {entt::null};
};

/// The spot in its field a farmer walks to and sows or digs at next, in metres
struct FieldWorkSpot
{
	glm::vec2 position {0.0f, 0.0f};
};

/// The wood a villager reacted to and is going for, kept while it waits beside wood that still moves
struct WoodReactionTarget
{
	entt::entity wood {entt::null};
	/// The turn it took up the wood, after which it waits only so long for the wood to come to rest
	uint32_t startTurn {0};
};

/// A shepherd's timing: the turns left of what it is doing with its flock, counted only while it stands with the flock
/// and not while it walks, and the state it goes on to when a wait ends, none to decide again
struct ShepherdWork
{
	int16_t countdown {0};
	VillagerStates pending {VillagerStates::InvalidState};
};

/// The thing a villager walks to while it moves, as it walks up to the leader of a flock
struct MoveTarget
{
	entt::entity thing {entt::null};
};

} // namespace openblack::ecs::components
