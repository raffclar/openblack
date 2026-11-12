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

#include <vector>

#include "landgen/Grid.h"

/// How water runs on the land: where it collects, where it flows and where the rivers and lakes are
namespace openblack::landgen::hydrology
{
/// The path of water from each corner to the sea
struct Drainage
{
	/// The heights with each pit filled to the height where it spills over, and a very small rise on flat ground so that
	/// water can always flow down to the sea
	Grid<float> filled;
	/// The index of the next corner down from each corner, or -1 for a corner of the sea
	std::vector<int32_t> receivers;
	/// The corners from the sea up: each corner comes after the corner that it flows to
	std::vector<uint32_t> order;
};

/// Fills the pits from the edge of the map up (a priority flood), and gives each corner the steepest way down on the
/// filled heights. The corners at or below the sea level that the sea reaches are the sea.
[[nodiscard]] Drainage Drain(const Grid<float>& heights, float seaLevel);

/// How much water flows through each corner: the rain of the corner and of all the corners that flow to it
[[nodiscard]] Grid<float> Accumulate(const Drainage& drainage, const Grid<float>& rain);

struct LakeSettings
{
	/// A corner that the filled heights cover by less than this is not in a lake
	float minDepth {0.6f};
	/// A lake with fewer corners than this is a pit, which the ground fills
	int minCorners {16};
	/// A lake that spills at or below this altitude goes down to the sea level and joins the sea through a channel
	float seaLakeSpill {32.0f};
	/// The altitude of the bed of a lake at the sea level, and of the channel to the sea
	float bed {-3.0f};
	float channel {1.0f};
};

struct Lakes
{
	/// The corners of each lake at the sea level, by index
	std::vector<std::vector<uint32_t>> seaLakes;
	/// 1 for each corner of a filled basin: a flat, wet hollow
	Grid<uint8_t> wet;
};

/// Finds the basins that the filled heights cover. A low basin becomes a lake at the sea level, with a channel along the
/// way that its water flows to the sea. A higher basin fills with ground and stays wet, because the game draws water
/// only at the sea level.
[[nodiscard]] Lakes FormLakes(Grid<float>& heights, const Drainage& drainage, const LakeSettings& settings);

struct RiverSettings
{
	/// A corner is on a river when the water of this many corners flows through it
	float sourceFlow {900.0f};
	/// A river with fewer corners than this, which joins no other river, is not kept
	int minCorners {12};
	/// The depth of the bed for the water of sourceFlow corners, and the deepest bed
	float depth {1.5f};
	float maxDepth {5.0f};
	/// The lowest altitude of a bed above the sea, so that the bed does not go below the sea level inland
	float lowestBed {4.0f};
	/// A point of a river every this many corners
	int pointSpacing {3};
};

struct Point
{
	/// A place on the map in corners, 10 m apart
	float x;
	float z;
};

struct River
{
	/// The corners that the river flows through, from its source to the sea or to the river that it joins
	std::vector<uint32_t> corners;
	/// The points of the river for the game, from the source down, a few corners apart
	std::vector<Point> points;
};

/// Follows the water down from each source: a corner on a river with no river above it. A river stops at the sea or at
/// a river that it joins.
[[nodiscard]] std::vector<River> TraceRivers(const Drainage& drainage, const Grid<float>& flow, const RiverSettings& settings);

/// Cuts the bed of each river into the land, deeper for more water, always down from the source, with sloped banks
void CarveRivers(Grid<float>& heights, const std::vector<River>& rivers, const Grid<float>& flow,
                 const RiverSettings& settings);
} // namespace openblack::landgen::hydrology
