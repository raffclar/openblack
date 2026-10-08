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
/// A script marker (created by CHL CREATE type Marker): only a position, no mesh and no
/// obstacle. Its MapCoords keep y relative to the ground and creating the marker
/// does not reset it, so GET_POSITION gives back exactly the position it was made at (y = 0 for a 2D [x, z]).
class MarkerArchetype
{
public:
	static entt::entity Create(const glm::vec3& position);
	MarkerArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
