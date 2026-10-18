/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/vec2.hpp>

#include "3D/MapCoords.h"

namespace openblack::ecs::components
{

/// The map position that the game keeps for a thing that it put at an exact map position. Metres cannot keep every map
/// position far from the corner of the map, so the thing keeps this map position with its metres. The map position
/// stays while the thing stays at the metres that it was put at.
struct MapPlace
{
	/// x and z in 16.16 fixed point; the altitude is not used
	map_coords::MapCoords place;
	/// The x and z of the thing's metres when it got the map position
	glm::vec2 placedAt {-1.0f, -1.0f};
};

} // namespace openblack::ecs::components
