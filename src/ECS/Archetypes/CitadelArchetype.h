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
	/// A temple standing whole, facing some way (in radians, as the land's script gives it)
	static entt::entity Create(const glm::vec3& position, PlayerNames playerOwner, float facing, const glm::mat4& rotation,
	                           const glm::vec3& size);
	/// A temple still to be built
	static entt::entity CreatePlan(int32_t townId, const glm::vec3& position, PlayerNames playerOwner, float facing,
	                               const glm::mat4& rotation, const glm::vec3& size);
	CitadelArchetype() = delete;

private:
	static entt::entity Make(const glm::vec3& position, PlayerNames playerOwner, const glm::mat4& rotation,
	                         const glm::vec3& size);
};
} // namespace openblack::ecs::archetypes
