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

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::ecs::systems
{

/// The dances: made by the scripts, the towns and the worship sites, each going on a turn at a time
class DanceSystemInterface
{
public:
	virtual ~DanceSystemInterface() = default;

	/// A dance of a row of the dances' table about a place, for a thing, dancing `duration` turns at a time (none for
	/// no end); none when the row is out of the table or its file can't be read. Worship sites', towns' and scripts'
	/// dances are all made here.
	virtual entt::entity Create(DanceInfo type, const glm::vec3& place, entt::entity owner, uint32_t duration,
	                            bool madeByScript) = 0;
	/// A dance goes, its dancers going back to deciding what to do
	virtual void Destroy(entt::entity dance) = 0;
	/// Each turn, after the players and before the living: each dance's turn
	virtual void ProcessTurn() = 0;
};

} // namespace openblack::ecs::systems
