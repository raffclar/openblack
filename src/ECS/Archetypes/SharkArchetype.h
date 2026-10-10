/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs::archetypes
{
class SharkArchetype
{
public:
	/// A shark where a script makes one, on the land's grid at the height of the land there, facing +x, at twice its
	/// size
	static entt::entity Create(const glm::vec3& position, float scale);
	SharkArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
