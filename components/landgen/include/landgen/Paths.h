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

/// The path of each link between the places. The seed picks the wander of the paths and which paths have lanterns.
[[nodiscard]] std::vector<Path> Lay(const Ground& ground, std::span<const Place> places, std::span<const Link> links,
                                    uint64_t seed, const Settings& settings);

/// The corners that the paths cover: a band two corners wide, joined along the axes so that each path shows as one
/// track. No corner is in the water or in a clear circle.
[[nodiscard]] Grid<uint8_t> Draw(std::span<const Path> paths, const Grid<uint8_t>& water, std::span<const Clear> clear);

/// The lanterns along the lit paths. The places are the town centres and the temple, which make a lantern a street
/// lantern when it is near them. No lantern stands in the water or in a clear circle.
[[nodiscard]] std::vector<Lantern> PlaceLanterns(std::span<const Path> paths, std::span<const Place> places,
                                                 const Grid<uint8_t>& water, std::span<const Clear> clear, uint64_t seed,
                                                 const Settings& settings);

/// The length of a path in corners, and its winding: the length over the straight distance between its ends
[[nodiscard]] float LengthOf(std::span<const Point> points);
[[nodiscard]] float WindingOf(std::span<const Point> points);
} // namespace openblack::landgen::paths
