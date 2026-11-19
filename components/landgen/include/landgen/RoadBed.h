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
#include "landgen/Paths.h"

/// The bed of the paths of a generated land, as a built road has it. Along a path, the bed follows a smooth profile of
/// the land, with a limit on the grade. Across a path, the bed is level. The bed cuts into the land where the land is
/// higher and fills where the land is lower. A bank of a fixed slope on each side joins the bed to the land, as the
/// cutting or the embankment of a motorway does.
namespace openblack::landgen::roads
{
struct Settings
{
	/// The generator cuts and fills the bed of the paths into the land
	bool grade {true};
	/// The distance between two samples of the profile along a path, in metres
	float step {5.0f};
	/// The half width of the mean that makes the profile smooth, in metres, and the number of times that it is applied
	/// before and after the grade limit. Two means one after the other also limit the change of the grade.
	float smoothing {30.0f};
	int smoothPasses {2};
	/// The largest grade of the bed: the rise over the run
	float steepestGrade {0.2f};
	/// The bed is never more than this distance above or below the land at the middle of the path, in metres. Where the
	/// land climbs faster than the grade limit for a long distance, the bed then follows the land at this distance.
	float deepest {10.0f};
	/// The grade from a place where the bed must stay at the land, such as a river
	float fixedGrade {0.3f};
	/// The grade from the edge of a ring round a building, a field or the temple. It is the largest grade of the bed,
	/// so that the bed of a path that leaves a town on a slope starts its own grade at the edge of the ring.
	float ringGrade {0.2f};
	/// Near a ring, the change to the land becomes less by this rise over run: a short blend as steep as the bank. Where
	/// this makes a cliff on steep land, the change becomes less more slowly.
	float ringTaper {0.5f};
	/// Near a fixed corner other than a ring, such as a shore or a river, the change to the land becomes less by this rise
	/// over run, or more slowly where this makes a cliff on steep land. Thus the bed stays level close to the water.
	float fixedTaper {0.5f};
	/// The half width of the level bed, in corners from the middle of the path. The bed of a straight path is four corners
	/// (30 m) wide.
	float halfWidth {1.6f};
	/// A corner near the middle of the path that is farther than this distance above or below the bed, in metres, is not
	/// part of the bed. The bank goes to it. Thus a path at the foot of a high cliff does not cut the cliff away.
	float deepestCorner {12.0f};
	/// The slope of the bank on the high side of the bed, where the bed cuts into the land: the rise over the run (1 in 2,
	/// as the cutting of a motorway)
	float cutSlope {0.5f};
	/// The slope of the bank on the low side of the bed, where the bed fills the land: the rise over the run (1 in 2, as
	/// the embankment of a motorway)
	float fillSlope {0.5f};
	/// The bank stops where it meets the land. Where the land is steeper than the bank, the bank cannot meet it. There,
	/// the change to the land becomes less by this rise over the run. The ground is then
	/// never steeper than the land by more than this value.
	float taper {0.2f};
	/// The deepest cut or the highest fill of a bank of the fixed slope next to the bed, in metres. A steep side that the
	/// bank meets within this height, such as a low cliff, gets the full bank.
	float deepestBank {20.0f};
	/// A path in a valley that rises on each side: a side that rises more steeply than this value is too steep for a bank
	/// of the fixed slope. The bed then moves away from the steeper side and cuts into the gentler side.
	float narrowSide {0.3f};
	/// Where the bed moves away from a steep side, its edge on that side is this distance from the middle of the path, in
	/// corners. The drawn path then stays on the bed.
	float narrowEdge {0.75f};
	/// The slope of a cliff: the rise over the run of 40 degrees, as the measure of the lands has it. Where the bed or a
	/// bank makes a corner as steep as a cliff that was not a cliff before, the change at that corner and the corners next
	/// to it becomes less, until no new cliff stays.
	float cliffSlope {0.8391f};
};

/// The values of a fixed corner or sample
constexpr uint8_t k_Fixed = 1;
constexpr uint8_t k_Ring = 2;

/// The land that the bed changes, in metres
struct Ground
{
	/// The height of each corner in metres
	const Grid<float>& metres;
	/// Not 0 where the land must stay as it is: the sea, the lakes and the rivers (`k_Fixed`), and the rings round the
	/// buildings, the fields and the temple (`k_Ring`)
	const Grid<uint8_t>& fixed;
	/// The bed and the banks do not cut a corner below this height in metres. A corner that is already lower keeps its
	/// height. The generator uses it to keep the sea and the lakes in their shores.
	const Grid<float>& lowest;
};

/// The profile of the bed along a path, from the height of the land at samples a step apart, in metres. The bed keeps
/// to the land at each fixed sample and at the two ends. From a sample in a ring (`k_Ring`) the bed comes back with no
/// more than the ring grade, and from the other fixed samples with no more than the fixed grade.
[[nodiscard]] std::vector<float> Profile(std::span<const float> land, std::span<const uint8_t> fixed, const Settings& settings);

/// The height of the level bed under the paths, in metres. The corners that no bed covers have no value.
struct Bed
{
	Grid<float> height;
	Grid<uint8_t> covered;
};

/// The bed of each path: its profile, and each corner near the middle of the path at the height of the nearest sample.
/// The height of the land at a sample is the median of the corners near it, so that a path beside a cliff keeps to the
/// side of the cliff that has more of the path. Where a path shares the way of a path before it, it keeps the bed of
/// that path, so that two paths on one way have one bed.
[[nodiscard]] Bed BedOf(std::span<const paths::Path> paths, const Ground& ground, const Settings& settings);

/// The land with the bed cut and filled into it, and with the banks that join the bed to the land, in metres
[[nodiscard]] Grid<float> Grade(const Bed& bed, const Ground& ground, const Settings& settings);
} // namespace openblack::landgen::roads
