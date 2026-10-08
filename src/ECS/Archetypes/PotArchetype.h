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
class PotArchetype
{
public:
	static entt::entity Create(const glm::vec3& position, float yAngleRadians, PotInfo type, int32_t amount);
	/// A pot or pile holding nothing yet, as a storage pit makes one for what it is given
	static entt::entity CreateEmpty(const glm::vec3& position, float yAngleRadians, PotInfo type);
	PotArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
