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

namespace openblack::ecs::archetypes
{
class ArenaArchetype
{
public:
	/// An arena for creature fights with its middle at a place in 16.16 map units (x, z) and a radius. A temporary one is
	/// made for one fight. Its ring of light stands round it from the start, hidden until a fight is on in it.
	static entt::entity Create(const glm::ivec2& place, float radius, bool temporary);
	ArenaArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
