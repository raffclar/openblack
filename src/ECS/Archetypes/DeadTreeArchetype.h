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
class DeadTreeArchetype
{
public:
	/// CREATE_DEAD_TREE: a dead tree of that tree type with the given life and x, y, z angles
	static entt::entity Create(const glm::vec3& position, TreeInfo type, float life, float xAngleRadians, float yAngleRadians,
	                           float zAngleRadians);
	DeadTreeArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
