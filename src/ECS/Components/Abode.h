/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <vector>

#include <entt/entity/entity.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{

struct Abode
{
	AbodeNumber type;
	uint32_t townId;
	// If a village does not have a ABODE_STORAGE_PIT then other abodes are used
	// by the villagers
	uint32_t foodAmount;
	uint32_t woodAmount;
	/// Its people, the newest first
	std::vector<entt::entity> inhabitants;
	/// Its couple, the man then the woman: the first adult of each sex to move in. A member who moves out alive keeps
	/// the place; the couple ends when one of them dies.
	std::array<entt::entity, 2> couple {entt::null, entt::null};
	/// How many of them are inside now
	uint32_t presentAtHome {0};
	/// Its row in the buildings' table
	AbodeInfo info {AbodeInfo::None};
	/// Its town has put it up for mending, or it went up again from a plan of a finished building, and it hasn't been
	/// made whole since: its town doesn't put it up again
	bool putUpForMending {false};
	/// How many of its town's passes over its buildings it has stood through, up to the age it counts as settled
	uint32_t age {0};
};

} // namespace openblack::ecs::components
