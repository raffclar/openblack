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

namespace openblack::lnd
{
struct LNDCountry;
}

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

/// The height of a step of altitude, and the height of the lowest land above the sea (altitude 4)
inline constexpr float k_HeightUnit = 0.67f;
inline constexpr float k_LowestLandHeight = 4.0f * k_HeightUnit;
/// The gentlest and the steepest slope a coast goes on down at under the water, in height for each unit across
inline constexpr float k_GentlestSlope = 0.06f;
inline constexpr float k_SteepestSlope = 6.0f;
/// A coast is a cliff where the land within two cells of it rises this much over sea level, in altitude steps, to the
/// full weight at the second
inline constexpr float k_CliffRiseStart = 14.0f;
inline constexpr float k_CliffRiseFull = 30.0f;
/// How far out to sea a cliff's foot reaches, in cells
inline constexpr float k_CliffReach = 6.0f;

/// The slope of a beach under the water, in height for each unit across: a long and shallow grade
inline constexpr float k_BeachSlope = 0.02f;
/// How far out a beach's grade goes before it eases into the shelf, from and to, in units
inline constexpr float k_BeachEaseStart = 150.0f;
inline constexpr float k_BeachEaseEnd = 450.0f;
/// The deepest the water is at high tide where a creature can walk, so that it only covers the creature's feet
inline constexpr float k_WadeDepth = 0.45f;

/// The depth at high tide of a corner at sea level with an altitude of 1 to 3. The original game draws these corners
/// flat at sea level and fades the land out over them, so its coast is at about altitude 3. The water's edge at high
/// tide is at the corners of altitude 3, and the lower corners go down a little.
[[nodiscard]] float ShallowDepth(uint8_t altitude);

/// How much of the ground of a country at an altitude is the beach's sand, 0 to 1: the share of its two blended
/// materials whose terrain type is sand or wet sand. `materialTypes` is the terrain type of each of the land's
/// materials.
[[nodiscard]] float BeachShare(const lnd::LNDCountry& country, uint8_t altitude, std::span<const uint16_t> materialTypes);

/// Whether a creature can walk on the cell with these four corner altitudes: some corner is over the bottom and the
/// cell is not too steep (see creature_route::WalkableLand)
[[nodiscard]] bool Walkable(uint8_t a, uint8_t b, uint8_t c, uint8_t d);

/// The sea bed for a grid of corner altitudes (x first along each row of z). Each corner of the sea goes down by its
/// distance to the nearest land above the sea, as a shelf and then a slope, deeper towards the map's edge, with broad
/// hills and dips over it. The grid is the whole map: past the land's blocks the sea goes on down to the map's edge.
/// The coast keeps the original game's line:
/// - The corners at sea level that the original game draws with land over them are only a little under the water.
/// - Where a creature can walk, the water is never deeper than k_WadeDepth at high tide.
/// - `beach` is how much of each corner's ground is the beach's sand, 0 to 1 (empty for none). A beach goes on down
///   under the water at the shallow k_BeachSlope. Other coasts keep their own slope, and cliffs stay steep.
[[nodiscard]] Seabed Build(std::span<const uint8_t> altitudes, glm::ivec2 size, glm::vec2 origin, float spacing,
                           std::span<const float> beach = {});

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
