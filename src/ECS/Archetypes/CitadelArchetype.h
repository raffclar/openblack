/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/fwd.hpp>
#include <glm/fwd.hpp>

#include "Enums.h"

namespace openblack::ecs::archetypes
{
class CitadelArchetype
{
public:
	/// A temple turned by yAngle radians about the vertical, as the game measures its turn
	static entt::entity Create(const glm::vec3& position, PlayerNames playerOwner, float yAngle, const glm::vec3& size);
	static entt::entity CreatePlan(int32_t townId, const glm::vec3& position, PlayerNames playerOwner, float yAngle,
	                               const glm::vec3& size);
	CitadelArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
