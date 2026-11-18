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

/// The things on the land: trees, bushes and rocks of the kinds that the lands of the story use
namespace openblack::landgen::features
{
/// The kinds of tree of the game, with the game's numbers
enum class Tree : uint8_t
{
	Beech = 0,
	Birch = 1,
	Cedar = 2,
	Conifer = 3,
	ConiferA = 4,
	Oak = 5,
	OakA = 6,
	Olive = 7,
	Palm = 8,
	PalmA = 9,
	PalmB = 10,
	PalmC = 11,
	Pine = 12,
	Bush = 13,
	BushA = 14,
	BushB = 15,
	Cypress = 16,
	CypressA = 17,
	Burnt = 22,
};

/// The game's number of the dead tree of the mobile statics
constexpr uint8_t k_DeadTreeStatic = 3;
/// The game's number of the plain rock of the mobile statics, which has no stone
constexpr uint8_t k_PlainRockStatic = 2;
/// The game's number of the spiky pillar of chalk. The game's number of a spiky pillar is this number plus its stone.
constexpr uint8_t k_SpikyPillarFeature = 41;

/// The shapes of rock of the game. The game's number of a rock is the number of its shape plus its stone.
enum class RockShape : uint8_t
{
	Boulder1 = 16,
	Boulder2 = 20,
	Boulder3 = 24,
	Flat = 28,
	Long = 32,
	Rock = 36,
	Sharp = 40,
	Square = 44,
};
enum class Stone : uint8_t
{
	Chalk = 0,
	Limestone = 1,
	Sandstone = 2,
	Volcanic = 3,
};
[[nodiscard]] constexpr uint8_t RockType(RockShape shape, Stone stone)
{
	return static_cast<uint8_t>(static_cast<uint8_t>(shape) + static_cast<uint8_t>(stone));
}

enum class Kind : uint8_t
{
	/// A living tree or bush; type is a Tree
	Tree,
	/// A mobile static, a rock or a dead tree; type is the game's number of the mobile static
	Static,
	/// A big forest: one model of many pines that grows pines round its edge when the villagers take its wood. Type is
	/// the game's number of the big forest, and size and maxSize are its scale.
	BigForest,
	/// A feature of the scenery, such as a pillar of rock; type is the game's number of the feature, and size and
	/// maxSize are its scale
	Feature,
};

struct Feature
{
	Kind kind;
	uint8_t type;
	/// The place on the map in metres
	float x;
	float z;
	/// The turn about the vertical in radians
	float angle;
	/// For a tree, its size now and the size that it grows to; for a static, its scale in both
	float size;
	float maxSize;
	/// The number of the forest of a tree, or 0 for a tree in no forest
	uint32_t forest {0};
	/// The lean of a static about the two horizontal axes in radians, and its height above the land in metres, as a land
	/// script gives them to a rock
	float xAngle {0.0f};
	float zAngle {0.0f};
	float altitude {0.0f};
};

struct Settings
{
	/// The distance between the points where a dead tree can be, in corners. Each point moves by a random amount inside
	/// its square.
	float spacing {2.0f};
	/// The distance between the points where a clump of rocks can start, in corners
	float rockSpacing {4.0f};
	/// The distance between the points where a group of pillars on the beach can start, in corners
	float stackSpacing {12.0f};
	/// The distance between the points where a stand of trees can start, in corners
	float standSpacing {4.0f};
	/// The distance between the points where a big forest can be, in corners
	float bigForestSpacing {20.0f};
	/// The most features on the land, so that the game keeps its frame rate. The big forests are not in this count.
	size_t most {12000};
	/// How many more stands of trees the land has than the generator usually gives, for tests of the frame rate. At 1,
	/// the land has the usual stands. The stands keep their kinds, sizes and spreads.
	float treeDensity {1.0f};
};

/// A place that features keep away from, such as the start area: in corners
struct Clearing
{
	float x;
	float z;
	float radius;
};

/// The ground that the features stand on
struct Ground
{
	const Grid<biomes::Biome>& biomes;
	/// The altitude of each corner
	const Grid<float>& heights;
	/// The distance from each corner to the sea, in corners
	const Grid<float>& seaDistance;
	/// A corner that is not 0 holds no feature, such as a corner of a river
	const Grid<uint8_t>& keepClear;
	std::span<const Clearing> clearings;
	/// The middle of the town of the start, in corners
	float townX;
	float townZ;
	/// A corner that is not 0 is on a river or its bank. Without it, the features on the shore keep off the corners
	/// of keepClear.
	const Grid<uint8_t>* rivers {nullptr};
	/// The middles of the other towns of the land, in corners. The distance to the town is the distance to the nearest
	/// town.
	std::span<const Clearing> towns {};
};

/// The ground at one point, in the units of the lands of the story
struct Spot
{
	/// The height above the sea in metres
	float height;
	/// The slope as the rise for each metre along the ground
	float slope;
	/// The distances to the sea and to the nearest town in metres
	float sea;
	float town;
};

/// Whether a feature can stand at a place in corners: on the map, out of the water, off the rivers and out of the
/// clearings
[[nodiscard]] bool IsFree(const Ground& ground, float x, float z);

/// The ground at a place in corners
[[nodiscard]] Spot SpotAt(const Ground& ground, float x, float z);

/// Whether a kind of tree can grow at a spot. The limits of each kind come from the trees of the lands of the story:
/// the height, the slope, the distance from the sea and the distance from a town where almost all trees of the kind
/// stand.
[[nodiscard]] bool Suits(Tree tree, const Spot& spot);

/// The number of trees of a stand, from a random number from 0 to below 1. The sizes have the spread of the groups of
/// trees on the lands of the story. A jungle has the larger groups of the palm island of the story.
[[nodiscard]] uint32_t StandSize(float unit, bool jungle);

/// The three classes of rock on the lands of the story. Each class has its own scales, lean and sizes of clumps.
enum class RockClass : uint8_t
{
	/// The small rocks: the plain rock, or the small rock of a stone
	Small,
	Boulder,
	/// The flat, long, sharp and square rocks
	Slab,
};

/// The number of rocks of a clump, from a random number from 0 to below 1. The sizes have the spread of the clumps of
/// each class on the lands of the story: most boulders and slabs lie alone or in pairs, and small rocks lie in clumps.
[[nodiscard]] uint32_t ClumpSize(RockClass rockClass, float unit);

/// The main class of a clump of rocks on ground of a slope (the rise for each metre) and a height above the sea in
/// metres, from a random number from 0 to below 1. Slabs lie on steeper ground, and boulders on high ground.
[[nodiscard]] RockClass MainClassAt(float slope, float height, float unit);

/// The features of the land:
/// - The big forests stand alone on ground with no trees round them, far from the sea and the town.
/// - The trees grow in stands. A stand has a main kind and some trees of the kinds that grow with it. Each biome has
///   its own number of stands, and the stands have the sizes and the spread of the groups on the lands of the story.
/// - The rocks lie in clumps. Each rock takes the scale, the lean and the height of a rock of the story.
/// - One ring of eight rocks stands on flat ground, and groups of spiky pillars stand on the beaches.
/// - The dead trees lie at the points of a grid, by the chances of the biome.
///
/// A point draws its random numbers from its own place, so a change elsewhere does not move it. Nothing stands in
/// water, on a river or in a clearing.
[[nodiscard]] std::vector<Feature> Place(const Ground& ground, uint64_t seed, const Settings& settings);
/// A forest of the land: its number and its place in metres, which is the place of its first tree, as on the lands of
/// the story
struct Forest
{
	uint32_t id;
	float x;
	float z;
};

struct ForestSettings
{
	/// The side of the squares of the map that hold one forest each, in metres
	float square {120.0f};
	/// A square with fewer trees than this holds no forest
	size_t fewest {12};
	/// The most forests on the land. Each forest grows its trees on each game turn, so that the forests keep the frame
	/// rate. The lands of the story have up to 20 forests.
	size_t most {16};
};

/// Puts trees into forests. The trees of each square of the map make one forest when the square has enough trees. The
/// squares nearest the point in metres make the forests first, and get the first numbers from 1. The other trees stay in
/// no forest. The burnt trees stay in no forest, as on the lands of the story.
[[nodiscard]] std::vector<Forest> GroupForests(std::span<Feature> features, float nearX, float nearZ,
                                               const ForestSettings& settings);
} // namespace openblack::landgen::features
