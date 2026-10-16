/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <utility>

#include <glm/vec2.hpp>

/// EXPERIMENT (not for merge): the tiles of the sea bed past the land's blocks, one for each square of the map with no
/// block. A fine tile, next to a block at a side or a corner, has a corner for each corner of the map; a coarse tile has
/// one every k_CoarseStep. Where a fine tile meets a coarse tile, the fine tile's side lies on the coarse tile's side,
/// so the two meet with no step.
namespace openblack::seabed
{

inline constexpr int k_CoarseStep = 4;
/// The cells along a side of a tile, as along a block
inline constexpr int k_TileCells = 16;

/// True if the tile at a square of the map is next to a block at a side or a corner. isPresent(x, z) is true where the
/// map has a block, false off the map.
template <typename IsPresent>
[[nodiscard]] bool FarTileIsFine(int x, int z, const IsPresent& isPresent)
{
	for (int ox = -1; ox <= 1; ++ox)
	{
		for (int oz = -1; oz <= 1; ++oz)
		{
			if (isPresent(x + ox, z + oz))
			{
				return true;
			}
		}
	}
	return false;
}

/// The two corners that a corner of a fine tile takes its height between, from where it is in the tile (0 to
/// k_TileCells along x and z). coarseSide tells which sides meet a coarse tile: x = 0, x = k_TileCells, z = 0,
/// z = k_TileCells. On such a side the corner lies on the line between the coarse tile's two corners either side of it.
/// Elsewhere both are the corner itself.
[[nodiscard]] constexpr std::pair<glm::ivec2, glm::ivec2> FarCornerSpan(glm::ivec2 corner,
                                                                        const std::array<bool, 4>& coarseSide)
{
	const auto snapped = [](int along) { return along - (along % k_CoarseStep); };
	if ((corner.x == 0 && coarseSide[0]) || (corner.x == k_TileCells && coarseSide[1]))
	{
		const int from = snapped(corner.y);
		return {{corner.x, from}, {corner.x, from == corner.y ? from : from + k_CoarseStep}};
	}
	if ((corner.y == 0 && coarseSide[2]) || (corner.y == k_TileCells && coarseSide[3]))
	{
		const int from = snapped(corner.x);
		return {{from, corner.y}, {from == corner.x ? from : from + k_CoarseStep, corner.y}};
	}
	return {corner, corner};
}

} // namespace openblack::seabed
