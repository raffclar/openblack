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

namespace openblack::ecs::components
{

/// A thing that shows only as a print of its model on the land, such as the holder of a singing stone. The thing is
/// not in the map's cells, so the searches of the map, the hand and the collisions do not find it. The land paints the
/// print at the place of the thing, with no turn and no scale, after the footprints of the things in the cells.
struct GroundPrint
{
	/// The order in which the prints were made: the land paints the newest print first
	uint32_t made {0};
};

} // namespace openblack::ecs::components
