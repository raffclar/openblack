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
#include <glm/fwd.hpp>

#include "Enums.h"

namespace openblack::ecs::archetypes
{
class PuzzleTotemArchetype
{
public:
	/// A totem of a puzzle: the building of a kind, at an angle around the vertical axis in radians and a size, with a
	/// greatest height of 100. It belongs to no town, and nothing destroys it. A script makes it, or a puzzle makes it
	/// for itself.
	static entt::entity Create(const glm::vec3& position, AbodeInfo kind, float yAngleRadians, float scale,
	                           entt::entity puzzle = entt::null);
	PuzzleTotemArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
