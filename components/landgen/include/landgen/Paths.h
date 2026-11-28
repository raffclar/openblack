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

#include <optional>
#include <span>
#include <vector>

#include "landgen/Grid.h"
#include "landgen/HeightCurve.h"

/// The winding paths between the towns of a generated land, drawn on the ground, and the lanterns along some of them
namespace openblack::landgen::paths
{
/// A point on the map, in corners
struct Point
{
	float x;
	float z;
};

/// A place that paths join, such as a town centre or the temple, in corners
struct Place
{
	float x;
	float z;
};

/// Two places that a path joins, by their index in the list of places
struct Link
{
	uint32_t from;
	uint32_t to;
};

/// A circle in corners where no part of a path is drawn and no lantern stands, such as a building or a field
struct Clear
{
	float x;
	float z;
	float radius;
};

/// The kinds of lantern, as the lands of the story have them: the street lantern near a town, and the country lantern
/// (a lantern on a fire) farther out
enum class LanternKind : uint8_t
{
	Street,
	Country,
};

/// A lantern beside a path, in metres
struct Lantern
{
	LanternKind kind;
	float x;
	float z;
};

/// A path between two places: its points in corners, from the first place to the second place
struct Path
{
	Link link;
	std::vector<Point> points;
	/// The path has lanterns along it
	bool lit {false};
};

struct Settings
{
	/// The cost of a step grows with the square of its slope (the rise over the run) times this value, so that a path
	/// keeps to easy ground
	float slopeWeight {40.0f};
	/// The cost of a step also grows with the square of the slope across the step times this value. Thus a path keeps
	/// away from the steep sides of the hills.
	float sideWeight {5.0f};
	/// A slope across the step steeper than this value costs as much as this value. Thus a path does not go far round a
	/// long ridge with steep sides.
	float steepestSide {1.0f};
	/// A step with more rise over run than this is a cliff, and no path takes it (about 35 degrees)
	float steepest {0.7f};
	/// The cost of each step onto the bank of a river. A path crosses a river only where it must.
	float riverCost {40.0f};
	/// The cost of a step grows by up to this share with a noise over the land, so that a path winds
	float wander {2.5f};
	/// The size of the bumps of that noise, in corners
	float wanderScale {14.0f};
	/// The cost of a step onto a path that the land has already, as a share of the usual cost. Paths join and share
	/// their way to a town, as tracks do.
	float reuse {0.6f};
	/// The passes of the smoothing over the points of a path
	int smoothPasses {3};
	/// The share of the paths that have lanterns along them
	float litShare {0.35f};
	/// The distance between two lanterns along a path, in metres: from the first value to the second value. The values
	/// are the 25th and the 75th percentiles of the distance from a lantern of the story to the nearest lantern of its kind.
	float streetSpacingLow {32.0f};
	float streetSpacingHigh {73.0f};
	float countrySpacingLow {50.0f};
	float countrySpacingHigh {91.0f};
	/// A lantern stands this far from the middle of the path, in metres, on a random side. The values are the 10th and
	/// the 90th percentiles of the lanterns of the story near a footpath (the median is about 3.5 m).
	float nearestSide {0.5f};
	float farthestSide {8.5f};
	/// A lantern this near a town centre or the temple, in metres, is a street lantern. Farther out it is a country
	/// lantern.
	float streetReach {180.0f};
	/// The most lanterns that a land has: the most that a land of the story has
	uint32_t mostLanterns {48};
};

/// The ground that the paths cross
struct Ground
{
	/// The altitude of each corner, and the curve that gives the metres of an altitude
	const Grid<float>& heights;
	const HeightCurve& curve;
	/// Not 0 where no path goes: the sea, the lakes and their shores
	const Grid<uint8_t>& water;
	/// Not 0 on the rivers and their banks
	const Grid<uint8_t>& rivers;
};

/// The parts of the land that the ways can join. Each corner out of the water gets the number of its part, from 1. Two
/// corners have the same number when a way can go from one to the other with no step into the water or up a cliff. The
/// corners in the water get 0.
[[nodiscard]] Grid<uint32_t> Parts(const Ground& ground, const Settings& settings);

/// The links that join each place after the first to the nearest place before it. The places of the towns of a land
/// make a chain from the first town, so these links join all the towns.
[[nodiscard]] std::vector<Link> ChainLinks(std::span<const Place> places);

/// The way of least cost from one corner to another, as a list of corners from the first to the last. A step costs
/// its length, more on slopes, on river banks and where the wander noise is high, and less on the corners of `laid`
/// that are not 0. No step goes into the water or up a cliff. Without such a way, nothing.
[[nodiscard]] std::optional<std::vector<Point>> FindWay(const Ground& ground, const Grid<uint8_t>& laid, Point from, Point to,
                                                        uint64_t seed, const Settings& settings);

/// Smooths the bends of a way: each point moves to the mean of the points next to it. The two ends stay.
[[nodiscard]] std::vector<Point> Smooth(std::span<const Point> points, int passes);

/// Smooths the bends of a way as `Smooth` does, but keeps the climb of the way. Where a smoothed step is steeper than the
/// steepest rise over run and steeper than the step before the pass, the two points of the step stay where they were.
/// Thus a zigzag up a steep slope keeps its bends. The metres are the height of each corner.
[[nodiscard]] std::vector<Point> Smooth(std::span<const Point> points, int passes, const Grid<float>& metres, float steepest);

/// The path of each link between the places. The seed picks the wander of the paths and which paths have lanterns.
/// Where water or cliffs divide the two places of a link, the path goes to the end of the link from another place on the
/// same part of the land as the end. This is the nearest place before the end, or the nearest place after the end when
/// no place before it is on that part. The `link` of the path is the link that it uses. A link whose two places a path
/// joins already gets no second path.
[[nodiscard]] std::vector<Path> Lay(const Ground& ground, std::span<const Place> places, std::span<const Link> links,
                                    uint64_t seed, const Settings& settings);

/// The corners that the paths cover: a band two corners wide, joined along the axes so that each path shows as one
/// track. No corner is in the water or in a clear circle.
[[nodiscard]] Grid<uint8_t> Draw(std::span<const Path> paths, const Grid<uint8_t>& water, std::span<const Clear> clear);

/// The corners that the paths cover, as the other `Draw` gives them, but on the ground of the metres. A corner that is
/// higher or lower than the corner at the point of the path by more than the rise of a cliff over the distance between
/// them is not covered. Thus a path at the foot of a cliff does not cover the top of the cliff. The cliff slope is a rise
/// over run.
[[nodiscard]] Grid<uint8_t> Draw(std::span<const Path> paths, const Grid<uint8_t>& water, std::span<const Clear> clear,
                                 const Grid<float>& metres, float cliffSlope);

/// The lanterns along the lit paths. The places are the town centres and the temple, which make a lantern a street
/// lantern when it is near them. No lantern stands in the water or in a clear circle.
[[nodiscard]] std::vector<Lantern> PlaceLanterns(std::span<const Path> paths, std::span<const Place> places,
                                                 const Grid<uint8_t>& water, std::span<const Clear> clear, uint64_t seed,
                                                 const Settings& settings);

/// The length of a path in corners, and its winding: the length over the straight distance between its ends
[[nodiscard]] float LengthOf(std::span<const Point> points);
[[nodiscard]] float WindingOf(std::span<const Point> points);
} // namespace openblack::landgen::paths
