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

#include <vector>

#include <entt/entity/entity.hpp>

#include "3D/MapCoords.h"
#include "ECS/ScriptFlockRules.h"

namespace openblack::ecs::components
{

/// A flock a script gathers villagers into. It has no turn of its own: its members keep up with it in their own state.
struct ScriptFlock
{
	/// Its own place, which its leader wanders about and the scripts move it by
	map_coords::MapCoords place;
	/// Its members in line, the first to the last; the last is its leader
	std::vector<entt::entity> members;
	/// How far from its place its leader may wander, and its followers from its leader, in whole metres
	uint16_t domainRadius {script_flock_rules::k_DefaultDomainRadius};
	uint16_t flockDistance {script_flock_rules::k_DefaultFlockDistance};
	/// How calm it is, as a script set it; nothing reads it
	int32_t calm {0};
};

/// A living's part in the scripts' flocks: the flock it is in, if any, and its place in a flock's line, which it keeps
/// after leaving
struct ScriptFlockMember
{
	entt::entity flock {entt::null};
	uint8_t order {0};
};

} // namespace openblack::ecs::components
