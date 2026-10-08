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

#include <entt/fwd.hpp>
#include <glm/fwd.hpp>

namespace openblack::ecs::archetypes
{
class MistArchetype
{
public:
	/// Map script CREATE_MIST "AFNFF"
	/// @param position x and z of the mist (y is ignored)
	/// @param altitude height above the land (the mist is at the land's height at (x, z) + altitude)
	/// @param colour ARGB, the alpha in the top byte
	/// @param size the mesh scale
	/// @param k the edge-on shrink factor; 1 turns it off (the shrink is only on when k != 1)
	static entt::entity Create(const glm::vec3& position, float altitude, uint32_t colour, float size, float k);
	MistArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
