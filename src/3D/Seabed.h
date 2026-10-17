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
	/// How much each corner is in a still pond, 0 to 1: water at sea level that does not join the open sea at the
	/// map's edge, and the land just round it
	std::vector<float> pond;

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
/// A coast is a cliff where the land within one cell of it rises this much over sea level, in altitude steps, to the
/// full weight at the second: a face of about 23 to 37 units over 10 units across, so only sharp cliffs count
inline constexpr float k_CliffRiseStart = 35.0f;
inline constexpr float k_CliffRiseFull = 55.0f;
/// How far out to sea a cliff's foot reaches, in cells
inline constexpr float k_CliffReach = 6.0f;

/// The slope of a beach under the water, in height for each unit across: a long and shallow grade
inline constexpr float k_BeachSlope = 0.02f;
/// How far out a beach's grade goes before it eases into the shelf, from and to, in units
inline constexpr float k_BeachEaseStart = 150.0f;
inline constexpr float k_BeachEaseEnd = 450.0f;
/// The deepest the water is at high tide where a creature can walk, so that it only covers the creature's feet
inline constexpr float k_WadeDepth = 0.45f;

/// The original game draws the land at altitude 3 and lower flat at sea level, and fades it into its sea over a
/// band from altitude 1 to 4. Its coast is about where the land is at this altitude, with the land half shown.
inline constexpr float k_OriginalCoast = 2.8f;
/// How far out from the original coast the sea bed goes down to k_WadeDepth, in units, and how far past that its slope
/// grows to the coast's own
inline constexpr float k_ShallowsWidth = 12.0f;
inline constexpr float k_ShallowsEase = 20.0f;
/// The steepest coast that is gentle: a steeper one starts to fall at its own slope from the original coast
inline constexpr float k_GentleMost = 0.3f;

/// The depth of the sea bed at a distance out from the original coast (in units) for a coast of this slope (height for
/// each unit across). A gentle coast goes down to k_WadeDepth over k_ShallowsWidth, then its slope grows to its own.
/// A cliff falls at its own slope from the line. The depth grows with the distance and has no steps.
[[nodiscard]] float ShelfFromCoastLine(float from, float slope);

/// How much of the ground of a country at an altitude is the beach's sand, 0 to 1: the share of its two blended
/// materials whose terrain type is sand or wet sand. `materialTypes` is the terrain type of each of the land's
/// materials.
[[nodiscard]] float BeachShare(const lnd::LNDCountry& country, uint8_t altitude, std::span<const uint16_t> materialTypes);

/// Whether a creature can walk on the cell with these four corner altitudes: some corner is over the bottom and the
/// cell is not too steep (see creature_route::WalkableLand)
[[nodiscard]] bool Walkable(uint8_t a, uint8_t b, uint8_t c, uint8_t d);

/// The cells where a creature can walk, for a grid of corner altitudes (x first along each row of z): (size.x - 1) by
/// (size.y - 1) cells. A cell is walkable when it is Walkable and it joins the largest region of Walkable cells side by
/// side, as the creature's own map of the land has it. A shallow patch of sea that does not join the land is not walkable.
/// The ponds of a grid of corners that are land (true) or water (false), x first along each row of z: 1 for each water
/// corner that does not join the water at the edge of the grid side by side, then spread out by one corner onto the
/// land and blended over the corners round it, 0 to 1
[[nodiscard]] std::vector<float> Ponds(const std::vector<bool>& land, glm::ivec2 size);

[[nodiscard]] std::vector<bool> WalkableCells(std::span<const uint8_t> altitudes, glm::ivec2 size);

/// The sea bed for a grid of corner altitudes (x first along each row of z). Each corner of the sea goes down by its
/// distance to the nearest land above the sea, as a shelf and then a slope, deeper towards the map's edge, with broad
/// hills and dips over it. The grid is the whole map: past the land's blocks the sea goes on down to the map's edge.
/// The coast keeps the original game's line:
/// - The water's edge at high tide is on the original coast (k_OriginalCoast), and the sea bed goes down from it.
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
/// The least water over the top of a coral or a plant at the lowest tide, in units: no reef shows over the water
inline constexpr float k_ReefClearance = 0.5f;
/// The steepest sea bed that coral and plants grow on, in height for each unit across
inline constexpr float k_ReefSteepest = 0.4f;

/// The reef: coral in patches on the sea bed that the lowest tide still covers, and plants in patches of their own,
/// never on the beaches the tide leaves, on steep slopes or in deep water. At most `mostCoral` and `mostPlants`.
[[nodiscard]] std::vector<ReefPlant> PlaceReef(const Seabed& seabed, float lowestTide, size_t mostCoral, size_t mostPlants);

/// The height of the floor at a point: the land's own height where the land is above the sea, else the sea bed
[[nodiscard]] float FloorHeight(const Seabed& seabed, glm::vec2 world, float landHeight);

} // namespace openblack::seabed
