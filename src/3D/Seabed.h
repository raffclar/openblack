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

#include <span>
#include <vector>

#include <glm/vec2.hpp>

/// EXPERIMENT (not for merge): a sea bed under the sea for the underwater view. The original game draws the land at sea
/// level flat in the plane of the sea, so there is nothing under the water. This makes a floor that goes down from the
/// coast into deep water.
namespace openblack::seabed
{

/// The land's altitude at a cell corner that is above the sea: corners at this altitude or lower are at sea level
inline constexpr uint8_t k_SeaLevelAltitude = 3;
/// The deepest the sea bed goes, in units
inline constexpr float k_MaxDepth = 160.0f;
/// The shelf near the coast: it goes down to about this depth, 63 percent of it at k_SlopeCells cells out
inline constexpr float k_ShelfDepth = 26.0f;
inline constexpr float k_SlopeCells = 8.0f;
/// How much deeper the slope past the shelf's edge goes, from 25 to 120 cells out
inline constexpr float k_SlopeDepth = 70.0f;
/// How much deeper the sea bed goes towards the map's edge
inline constexpr float k_EdgeDepth = 40.0f;

/// The depth of the sea bed under each cell corner, in units below sea level (0 where the land is above the sea)
struct Seabed
{
	/// Corners along x and z
	glm::ivec2 size {0, 0};
	/// Where the first corner is, in world units along x and z
	glm::vec2 origin {0.0f, 0.0f};
	/// The distance between two corners, in units
	float spacing {10.0f};
	/// The depth of each corner, x first along each row of z
	std::vector<float> depth;

	/// The depth of the sea bed at a point, blended between the four corners round it; the nearest edge outside the
	/// grid, and 0 when there is no grid
	[[nodiscard]] float DepthAt(glm::vec2 world) const;
};

/// The depth for a corner of the sea that is this many cells from the nearest land above the sea
[[nodiscard]] float DepthForDistance(float cells);

/// The sea bed for a grid of corner altitudes (x first along each row of z). Each corner of the sea goes down by its
/// distance to the nearest land above the sea, as a shelf and then a slope, deeper towards the map's edge, with broad
/// hills and dips over it.
/// The grid is the whole map: past the land's blocks the sea goes on down to the map's edge.
[[nodiscard]] Seabed Build(std::span<const uint8_t> altitudes, glm::ivec2 size, glm::vec2 origin, float spacing);

/// The height of the floor at a point: the land's own height where the land is above the sea, else the sea bed
[[nodiscard]] float FloorHeight(const Seabed& seabed, glm::vec2 world, float landHeight);

} // namespace openblack::seabed
