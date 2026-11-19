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

#include "landgen/Biomes.h"
#include "landgen/Grid.h"
#include "landgen/Lakes.h"

/// The sand round the lakes above the sea. Each lake gets the same edge as the sea shore: wet sand on its bed and its
/// shore, then a beach of the same width rule as the beaches of the sea. The rule measures each height from the level of
/// the nearest lake, not from the sea level.
namespace openblack::landgen::lakeshore
{
/// The ground of the land that the rule reads, one value at each corner
struct Ground
{
	/// The altitudes that the game draws
	const Grid<float>& altitudes;
	/// The largest difference of altitude to the next corner
	const Grid<float>& slopes;
	const Grid<float>& temperature;
	/// The width of a beach at each corner, in corners from the water, as the beaches of the sea have
	const Grid<float>& beachWidths;
};

enum class Shore : uint8_t
{
	/// Not a part of the edge of a lake
	Land,
	/// The water and the shore of a lake: wet sand, as the bed of the sea
	Bed,
	/// The beach round a lake: sand, as the beaches of the sea
	Beach,
};

struct Shores
{
	Grid<Shore> shore;
	/// The distance to the water of the nearest lake, in corners. A corner far from all lakes has a very large value.
	Grid<float> distance;
};

/// Finds the edge of each lake. A corner is a beach when all of these conditions are true:
/// - It is not more than the beach width from the water of a lake.
/// - It is above the level of that lake, by less than the beach altitude over the sea level of the thresholds.
/// - It is gentler than the beach slope and warmer than the beach temperature.
/// - It is below the alpine altitude.
/// Other ground near the water is wet sand, as the coast of the sea: the ground on the corners of the shore steps of the
/// sea, lower than the top of the wet sand of the coast over the level.
[[nodiscard]] Shores Find(const Ground& ground, std::span<const lakes::Lake> lakes, const biomes::Thresholds& thresholds);

struct StepSettings
{
	/// The shore of a lake is this far over its level, as the lakes give it
	float freeboard {0.25f};
	/// The steepest rise of the ground behind the shore steps, in altitude units for each corner, as the sea cliffs
	float rise {28.0f};
};

/// Gives each lake the shore steps of the sea, measured from the shore of the lake. The rule lowers the ground near the
/// water and does not lift it:
/// - The first three corners from the water are at the height of the shore. The game draws them flush with the water.
/// - The fourth corner is one altitude over the shore, as the first ground above the sea.
/// - Behind the steps, the ground rises by the rise of the settings for each corner at most.
/// No corner of a lake changes, and no corner goes under the shore of a lake that is near it. Thus no lake spills over.
/// The ground in the places to keep (for example the flat ground of the towns) does not change.
/// The result is the list of the corners that went down.
[[nodiscard]] std::vector<uint32_t> ShoreSteps(Grid<float>& heights, std::span<const lakes::Lake> lakes,
                                               std::span<const lakes::Keep> keep, const StepSettings& settings);
} // namespace openblack::landgen::lakeshore
