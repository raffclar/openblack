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
};

struct Settings
{
	/// The distance between the points where a feature can be, in corners. Each point moves by a random amount inside
	/// its square.
	float spacing {2.0f};
	/// No tree grows on a slope steeper than this, and no rock lies on one steeper than the second
	float treeSlope {9.0f};
	float rockSlope {16.0f};
	/// The most features on the land, so that the game keeps its frame rate
	size_t most {12000};
};

/// A place that features keep away from, such as the start area: in corners
struct Clearing
{
	float x;
	float z;
	float radius;
};

/// The features of the land. Each point of a grid over the map takes a feature by the biome under it: its chance of a
/// tree, of a rock, and the kinds of tree and rock. The trees grow in clumps where the clump noise is high. A point
/// draws its random numbers from its own place, so a change elsewhere does not move it. Nothing stands in water, on a
/// river, on a steep slope or in a clearing.
[[nodiscard]] std::vector<Feature> Place(const Grid<biomes::Biome>& biomes, const Grid<float>& heights,
                                         const Grid<uint8_t>& keepClear, std::span<const Clearing> clearings, uint64_t seed,
                                         const Settings& settings);
} // namespace openblack::landgen::features
