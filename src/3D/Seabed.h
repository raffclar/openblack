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
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

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
	/// The shore at each corner: x the distance to the coast in units (out to sea positive, inland negative), yz the
	/// direction across the ground towards the coast, w the height of the ground (the land, or the sea bed)
	std::vector<glm::vec4> shore;
	/// How much each corner is at the foot of a cliff, 0 to 1: steep land at the coast with deep water close in
	std::vector<float> cliff;
	/// Points along the foot of the cliffs, on the sea's side, with the direction out to sea (zw)
	std::vector<glm::vec4> cliffFoot;

	/// The depth of the sea bed at a point, blended between the four corners round it; the nearest edge outside the
	/// grid, and 0 when there is no grid
	[[nodiscard]] float DepthAt(glm::vec2 world) const;
};

/// The depth for a corner of the sea that is this many cells from the nearest land above the sea
[[nodiscard]] float DepthForDistance(float cells);

/// A coast slopes gently when the land within two cells of it is no higher than this over sea level, in altitude steps
inline constexpr uint8_t k_GentleRise = 8;
/// How much deeper a beach gets for each cell out from a gentle coast: it is under the lowest tide 8 cells out
inline constexpr float k_BeachSlope = 0.35f;
/// A coast is a cliff where the land within two cells of it rises this much over sea level, in altitude steps, to the
/// full weight at the second
inline constexpr float k_CliffRiseStart = 14.0f;
inline constexpr float k_CliffRiseFull = 30.0f;
/// How far out to sea a cliff's foot reaches, in cells
inline constexpr float k_CliffReach = 6.0f;
/// The depth out from a gentle coast, where the land goes on under the water as a beach
[[nodiscard]] float BeachDepth(float cells);

/// The sea bed for a grid of corner altitudes (x first along each row of z). Each corner of the sea goes down by its
/// distance to the nearest land above the sea, as a shelf and then a slope, deeper towards the map's edge, with broad
/// hills and dips over it.
/// The grid is the whole map: past the land's blocks the sea goes on down to the map's edge.
[[nodiscard]] Seabed Build(std::span<const uint8_t> altitudes, glm::ivec2 size, glm::vec2 origin, float spacing);

/// A coral or a plant of the reef on the sea bed
struct ReefPlant
{
	enum class Kind : uint8_t
	{
		Coral,
		Seaweed
	};
	Kind kind;
	/// Where its foot is, on the sea bed
	glm::vec3 position;
	/// How big it is, in units
	float scale;
	/// The angle it is turned to about the up axis, in radians
	float yaw;
	/// Which of the coral atlas's nine cells, or for a plant 0 to 1 for its kind
	float variant;
	/// 0 to 1, which of the soft colours it takes
	float tint;
};

/// The coral: where the sea bed is at least this much under the lowest tide, so that it is never out of the water, and
/// at most this much under it
inline constexpr float k_CoralTopUnderTide = 1.5f;
inline constexpr float k_CoralBottomUnderTide = 12.0f;
/// The plants grow from a little shallower to a little deeper than the coral
inline constexpr float k_PlantTopUnderTide = 1.0f;
inline constexpr float k_PlantBottomUnderTide = 20.0f;
/// The steepest sea bed that coral and plants grow on, in height for each unit across
inline constexpr float k_ReefSteepest = 0.4f;

/// The reef: coral in patches on the sea bed that the lowest tide still covers, and plants in patches of their own,
/// never on the beaches the tide leaves, on steep slopes or in deep water. At most `mostCoral` and `mostPlants`.
[[nodiscard]] std::vector<ReefPlant> PlaceReef(const Seabed& seabed, float lowestTide, size_t mostCoral, size_t mostPlants);

/// The height of the floor at a point: the land's own height where the land is above the sea, else the sea bed
[[nodiscard]] float FloorHeight(const Seabed& seabed, glm::vec2 world, float landHeight);

} // namespace openblack::seabed
