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

#include "landgen/Grid.h"

/// The shape of the land: the first heights and the erosion that wears them. Heights are in the altitude units of a
/// landscape (0.67 m each), with the sea bed below 0. The corners are 10 m apart.
namespace openblack::landgen::terrain
{
/// The highest altitude that a landscape can hold
constexpr float k_HighestAltitude = 255.0f;
/// The highest altitude that the game draws at sea level
constexpr float k_SeaLevel = 3.0f;

struct BaseSettings
{
	/// The corners next to each edge of the map that stay under the sea, so that the land is an island
	int seaMargin {40};
	/// How many mountain ranges cross the island
	int ranges {3};
	/// The height of a crest of a range, before the erosion wears it
	float mountainHeight {300.0f};
	/// The height of the inland plains, and the part of the land from the coast inwards where the ground rises to them
	float plainHeight {55.0f};
	float plainRise {0.1f};
	/// The height of the hills on the plains, up or down
	float hillHeight {55.0f};
	/// How much of the map the island takes, from about -0.3 (small) to 0.3 (large)
	float landAmount {0.0f};
	/// How many hollows the low land has, each a radius in corners, to hold the lakes and lagoons
	int hollows {6};
	float hollowRadius {16.0f};
	/// The highest ground where a hollow can be, and the altitude of its floor
	float hollowGround {28.0f};
	float hollowFloor {-6.0f};
};

/// The first heights of the island: an outline of warped noise, plains and hills, and mountain ranges along random
/// lines, each with ridged noise for its crests and valleys. Hollows in the low land below the hollow ground hold the
/// lakes.
[[nodiscard]] Grid<float> BaseHeights(int side, uint64_t seed, const BaseSettings& settings);

struct HydraulicSettings
{
	/// How many drops of rain fall, each at a random corner
	int drops {400000};
	/// The most steps that a drop takes before it dries up
	int steps {80};
	/// How much a drop keeps its direction rather than flow down the slope, from 0 to 1
	float inertia {0.05f};
	/// How much sediment a drop can hold, for its speed, water and the drop of its height
	float capacity {4.0f};
	float minCapacity {0.01f};
	/// The part of the free capacity that a drop takes from the ground in a step
	float erosion {0.3f};
	/// The part of the sediment above the capacity that a drop puts down in a step
	float deposition {0.3f};
	/// The part of its water that a drop loses in a step
	float evaporation {0.012f};
	float gravity {4.0f};
	/// The radius in corners from which a drop takes ground
	int radius {3};
	/// The scale from altitude units to the units of the drop's slope. Lower values make the slopes gentler for the drops.
	float heightScale {0.067f};
};

/// Rain wears the land: drops run down the slopes, take ground where they speed up and put it down where they slow
/// down or go into the sea. Valleys cut into the slopes and the sediment fills the low ground.
void ErodeHydraulic(Grid<float>& heights, uint64_t seed, const HydraulicSettings& settings);

struct ThermalSettings
{
	int passes {30};
	/// The steepest difference of altitude to the next corner along an axis that the ground keeps on the plains and on
	/// the mountains. Between them the limit changes with the altitude.
	float plainTalus {9.0f};
	float mountainTalus {18.0f};
	/// The part of the ground above the limit that slides down in a pass
	float rate {0.4f};
};

/// The ground of the slopes that are too steep slides down to the corners below them, until no slope is steeper than
/// its limit. All the corners slide at the same time in a pass, so that the order of the corners has no effect. The
/// corners that have a value other than 0 in the fixed span do not move, and no ground slides to or from them.
void ErodeThermal(Grid<float>& heights, const ThermalSettings& settings, std::span<const uint8_t> fixed = {});

struct TerraceSettings
{
	/// The altitude of the lowest edge of a terrace, from the lowest to the highest that the seed can pick
	float firstLevelLow {18.0f};
	float firstLevelHigh {35.0f};
	/// The difference of altitude between two edges, from the lowest to the highest that the seed can pick
	float spacingLow {45.0f};
	float spacingHigh {100.0f};
	/// No edge is higher than this altitude
	float highestLevel {235.0f};
	/// The part of the shape of the ground that a terrace keeps: less on the low land, more on the mountains
	float keepLow {0.75f};
	float keepHigh {0.55f};
	float keepLowAltitude {90.0f};
	float keepHighAltitude {190.0f};
	/// The noise that moves the edges of the terraces: its largest change of altitude, and its waves across the map
	float edgeNoise {340.0f};
	float edgeFrequency {16.0f};
	/// How far the warp moves the noise of the edges, in waves of the noise, and the part of the noise that is ridged
	float edgeWarp {2.0f};
	float edgeRidge {0.7f};
	/// The largest change of the edge noise on the calm parts of the land, and the waves of the calm and rough parts
	/// across the map. The edge noise is the change on the rough parts.
	float calmNoise {100.0f};
	float roughFrequency {3.0f};
	/// The rise of the face of a cliff in altitude units for each corner, from the lowest to the highest that the noise
	/// gives. 16 is about 47 degrees and 34 is about 66 degrees.
	float cliffRiseLow {16.0f};
	float cliffRiseHigh {34.0f};
	float cliffFrequency {11.0f};
};

/// The altitudes of the edges of the terraces for the seed, from the lowest up
[[nodiscard]] std::vector<float> TerraceLevels(uint64_t seed, const TerraceSettings& settings);

/// The step of a terrace for each corner: the index of the edge of the terrace that the corner is on (0 below the first
/// edge). The edge noise moves each corner up or down before the step is found. A corner that is higher or lower than
/// its four neighbours along the axes takes the nearest step of the neighbours, so that no step holds one corner only.
[[nodiscard]] Grid<uint8_t> TerraceSteps(const Grid<float>& heights, std::span<const float> levels, uint64_t seed,
                                         const TerraceSettings& settings);

/// Cuts the land above the first edge into flat terraces with sharp cliffs between them, as on the lands of the story.
/// Each terrace keeps a part of the shape of its ground. The cliff is on the low side of an edge: the ground rises to the
/// edge in the width of the cliff. The land below the first edge, and the sea, do not change. The result has 1 for each
/// corner of a cliff, so that the erosion after this stage can keep the cliffs.
[[nodiscard]] Grid<uint8_t> Terrace(Grid<float>& heights, uint64_t seed, const TerraceSettings& settings);

struct ShelfSettings
{
	/// The width of the flat strip along the sea in corners, from the lowest to the highest that the noise gives
	float widthLow {3.0f};
	float widthHigh {4.5f};
	/// The altitude of the strip at the water, and how much it rises for each corner from the water
	float altitude {3.6f};
	float rise {0.25f};
	/// The rise of the bank behind the strip, in altitude units for each corner, from the lowest to the highest that the
	/// noise gives. 10 is about 34 degrees and 22 is about 56 degrees.
	float bankLow {10.0f};
	float bankHigh {22.0f};
	float frequency {9.0f};
};

/// Makes the flat strip along the sea, with a bank or a cliff behind it, as on the lands of the story. The ground near
/// the water goes down to the strip and to the bank, and lower ground does not change.
void ShoreShelf(Grid<float>& heights, uint64_t seed, const ShelfSettings& settings);

/// The largest difference of height to the next corner along an axis, at each corner
[[nodiscard]] Grid<float> Slopes(const Grid<float>& heights);
} // namespace openblack::landgen::terrain
