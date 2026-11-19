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

/// The lakes above the sea level of a generated land. Each lake has its own water level and the corners that its water
/// and its shore cover. The game draws the water of each lake at its level.
namespace openblack::landgen::lakes
{
struct Settings
{
	/// A corner that the filled heights cover by less than this is not in a basin
	float minDepth {0.6f};
	/// A basin with fewer corners than this, or less deep than this at its deepest, does not hold a lake
	int minCorners {40};
	float minDeepest {2.0f};
	/// The water stays this far below the height where the basin spills over
	float freeboard {0.25f};
	/// The shore of a lake: the ground that is less than this above the level, next to the water. The water surface
	/// covers the shore and fades into the land there, as the sea fades into its coast.
	float shoreReach {1.0f};
	/// The most corners that the shore goes from the water
	int shoreSteps {3};
	/// When two lakes join, the bed of the higher lake goes down to this far below the level of the joined lake
	float bedBelow {0.5f};
	/// A lake has its level at least this far above the sea level
	float lowestAboveSea {1.5f};
};

struct Lake
{
	/// The height of the water, in the units of the heights
	float level;
	/// The corners under the water and on the shore, by index, in order
	std::vector<uint32_t> corners;
};

/// A circle of ground in corners, such as the ground of a town, with its middle and its radius
struct Keep
{
	float x;
	float z;
	float radius;
};

/// Finds the lakes: each basin that the heights hold above the sea, filled to a little below the height where it spills
/// over. When the water or the shore of two lakes touch, the two become one lake at the lower level, and the bed of the
/// higher lake goes down under that level. Thus no two water surfaces at different levels touch. The lakes are in the
/// order of their lowest corner.
///
/// The water and the shore of a lake never cover a corner in a circle of `dry`, such as the ground of a town or a start.
/// A lake that reaches such a circle gets a lower level. If the lower lake is too small or too shallow, the basin stays
/// dry.
[[nodiscard]] std::vector<Lake> Find(Grid<float>& heights, float seaLevel, const Settings& settings,
                                     std::span<const Keep> dry = {});

struct DigSettings
{
	/// How many round hollows the high flat ground gets, to hold lakes on the hills
	int count {3};
	/// The radius of a hollow in corners, from the first to the second
	float smallest {5.0f};
	float largest {9.0f};
	/// A hollow is on ground this high at least
	float lowest {30.0f};
	/// The ground round a hollow differs by at most this much
	float roughest {6.0f};
	/// The depth of a hollow at its middle, more than the roughness of its ground
	float depth {4.0f};
	/// A hollow keeps this many corners from the sea, a river and the start area
	float clearance {6.0f};
};

/// Digs round hollows into high, flat ground, which then hold lakes on the hills. A hollow keeps away from the corners
/// that keepAway marks and from the places to keep. The seed picks the places.
void DigHollows(Grid<float>& heights, const Grid<uint8_t>& keepAway, const std::vector<Keep>& keep, uint64_t seed,
                const DigSettings& settings);
} // namespace openblack::landgen::lakes
