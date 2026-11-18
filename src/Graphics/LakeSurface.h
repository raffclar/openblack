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

#include <functional>
#include <optional>
#include <vector>

#include "3D/LandLakes.h"

/// The water surface of the lakes above the sea of a generated land. The surface of a lake is not a rectangle: it is the
/// part of each triangle of the land that is under the level, flat at the level and cut where the land meets the level.
/// The water fades to nothing at that line, as the sea fades into its coast. Thus a lake on a hill never shows through
/// the slopes of the hill.
namespace openblack::graphics::lake_surface
{
struct Vertex
{
	float x;
	float y;
	float z;
	/// The height of the land under the vertex, and the level of the vertex's lake
	float landHeight;
	float level;
	/// The height that the water fades by. It is the land's height in the lake. Towards a corner outside the lake, it
	/// goes up to the level at the edge of the surface, so the water fades to nothing there.
	float fade;
};

struct Surface
{
	std::vector<Vertex> vertices;
	/// Three vertices for each triangle
	std::vector<uint32_t> indices;
};

/// The height of the land at a corner of the map, as the land is drawn. None where no land is.
using HeightAt = std::function<std::optional<float>(int x, int z)>;
/// Whether the cell at (x, z) has its diagonal from corner (x + 1, z) to corner (x, z + 1), or else from (x, z) to
/// (x + 1, z + 1)
using SplitAt = std::function<bool(int x, int z)>;

/// The surface of all the lakes, one mesh. The cells that a lake's corners touch get the lake's water. A corner that is
/// not in the lake counts as land at least shoreReach above the level, so the water stops before it.
[[nodiscard]] Surface Build(const land_lakes::LakeMap& lakes, const HeightAt& heightAt, const SplitAt& splitAt,
                            float shoreReach);
} // namespace openblack::graphics::lake_surface
