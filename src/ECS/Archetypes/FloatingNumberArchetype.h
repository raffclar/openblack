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

namespace openblack::ecs::archetypes
{
/// A number that floats up from a point for a few seconds, written as a whole number
class FloatingNumberArchetype
{
public:
	/// The number, in a colour (0xAARRGGBB)
	static entt::entity Create(const glm::vec3& position, float value, uint32_t colour);
};
} // namespace openblack::ecs::archetypes
