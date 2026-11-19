/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <span>
#include <vector>

#include "landgen/Biomes.h"
#include "landgen/Features.h"
#include "landgen/Grid.h"
#include "landgen/Hydrology.h"

/// The streams of a generated land, as the streams of the lands of the story: their count for the area, their length,
/// the spacing of their points, the water that they reach and the trees on their banks
namespace openblack::landgen::streams
{
struct Settings
{
	/// The seed picks the streams for each square kilometre of land between these numbers. The lands of the story that
	/// have streams have 1.17 to 7.36 streams for each square kilometre.
	float fewestPerKm2 {1.2f};
	float mostPerKm2 {7.3f};
	/// Each land has at least this many streams, as the land of the story with the fewest streams
	size_t fewest {2};
	/// The length of a step from one corner of a stream to the next, in metres. A stream has the number of corners for
	/// a length from the spread of the streams of the story, from 27 m to 639 m with the median 183 m.
	float metresPerCorner {12.0f};
	/// The shortest stream in corners, which does not join another stream. The shortest stream of the story is 27 m long.
	size_t shortest {3};
	/// The springs are at the points of a grid with this spacing in corners, which the seed moves inside their squares
	int springSpacing {5};
	/// A spring is between these heights in metres of the lands of the story, 0.67 m for each altitude unit. A
	/// generated land with high mountains has its springs at the same altitudes. A tenth of the streams of the story
	/// start below 16 m, and the highest starts at 100 m. A stream on a river also starts at springLowest or higher.
	float springLowest {16.0f};
	float springHighest {100.0f};
	/// A stream starts this many corners or more from the streams that stay, along each axis. On the lands of the
	/// story, half of the streams start 90 m or more from another stream.
	int sourceGap {5};
	/// The points of a stream move by at most this part of a corner from the middle of their corner, so that the points
	/// are not on a grid
	float wander {0.25f};
	/// The order of the streams goes by their fall, times a random number from 1 - mix to 1 + mix
	float mix {0.25f};
};

/// The streams of a land:
/// - A stream starts at a spring on a hill, or on a river. A stream from a river keeps the end of the river.
/// - The water of a stream flows down along `receivers`: the corner that the water of each corner flows to, or below 0
///   in the sea. A stream ends in the water or on a corner of another stream.
/// - Each stream has a length from the spread of the streams of the story. A stream that does not get to the water or
///   to another stream in that length does not stay.
/// - The seed picks the number of streams for the area of the land. The streams that fall the most come first.
/// - The points of a stream are one corner apart, at the corners of the stream, moved a little by the seed.
///
/// The corners of each river go from the source down. `heights` are the altitudes of the corners. Each corner that is
/// not 0 in `water` is the sea or a lake. `landCorners` is the number of corners of land, for the area.
[[nodiscard]] std::vector<hydrology::River> Choose(std::span<const hydrology::River> rivers, std::span<const int32_t> receivers,
                                                   const Grid<float>& heights, const Grid<uint8_t>& water, size_t landCorners,
                                                   uint64_t seed, const Settings& settings);

/// Whether the last corner of a river is in the water, or is a corner of one of the other rivers
[[nodiscard]] bool Reaches(const hydrology::River& river, std::span<const hydrology::River> rivers, const Grid<uint8_t>& water);

/// One kind of tree on the banks of the streams, with its weight
struct BankTree
{
	features::Tree tree;
	uint32_t weight;
};

/// The kinds of tree on the banks of the streams of a biome. The weights are the counts of the trees 10 m or less from
/// a stream on the lands of the story: the warm, wet land of palms (Cedar first), the dry land of scrub and sand, and
/// the green lands. A biome with no trees by the streams of the story has none.
[[nodiscard]] std::span<const BankTree> BankTreesOf(biomes::Biome biome);

struct BankSettings
{
	/// The trees of a stand on the bank are this far apart along the stream, in metres
	float treeSpacing {5.0f};
	/// A tree stands this far to the side of the line of the stream, in metres. On the lands of the story, half of the
	/// trees by a stream are 5 m or less from its line.
	float nearest {2.0f};
	float farthest {9.0f};
	/// The part of the length of the streams that has a stand on the bank. On the lands of the story, 17% to 37% of the
	/// length of the streams has a tree 12 m or less from it.
	float cover {0.3f};
};

/// The number of trees of a stand on a bank, from a random number from 0 to below 1. The stands have the spread of the
/// groups of trees 10 m or less from the streams of the story: a quarter have 1 tree, the median 3, nine tenths 12 or
/// fewer, and the largest 74.
[[nodiscard]] uint32_t BankStandSize(float unit);

/// The scale of a tree on a bank, from a random number from 0 to below 1. The scales have the spread of the trees 10 m or
/// less from the streams of the story: 0.35 to 3.07, with the median 1.12.
[[nodiscard]] float BankTreeScale(float unit);

/// The trees on the banks of the streams: stands at random places along each stream, on both sides, of the kinds of
/// the biome of the bank. A tree stands only where `ground` lets a feature stand and the kind suits the ground. The
/// keepClear of `ground` must not hold the banks of the streams.
[[nodiscard]] std::vector<features::Feature> PlaceBankTrees(const features::Ground& ground,
                                                            std::span<const hydrology::River> streams, uint64_t seed,
                                                            const BankSettings& settings);
} // namespace openblack::landgen::streams
