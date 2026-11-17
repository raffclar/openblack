/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "landgen/Features.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <map>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "landgen/Noise.h"
#include "landgen/Random.h"
#include "landgen/Terrain.h"

using namespace openblack::landgen;
using namespace openblack::landgen::features;
using biomes::Biome;

namespace
{
constexpr float k_MetresPerCorner = 10.0f;
constexpr float k_MetresPerAltitude = 0.67f;
constexpr float k_FullTurn = 6.28318531f;
constexpr float k_Anywhere = 1.0e9f;

struct Choice
{
	Tree tree;
	uint32_t weight;
};

/// The limits of a kind of tree, and the look of a stand that has this kind as its main kind
struct KindRule
{
	Tree tree;
	/// The heights above the sea in metres between which the kind grows
	float lowest;
	float highest;
	/// The steepest slope, as the rise for each metre
	float steepest;
	/// The nearest that the kind grows to the sea, in metres
	float nearestSea;
	/// The distances from the town between which the kind grows, in metres
	float nearestTown;
	float farthestTown;
	/// The scales of the kind
	float smallest;
	float largest;
	/// The radius of a stand in metres is this value times the square root of its number of trees
	float spread;
	/// The percentage of the trees of a stand that are of its main kind. The other trees are of the companion kinds.
	uint32_t mainShare;
	std::span<const Choice> companions;
};

// The kinds that grow in the groups of each main kind on the lands of the story, with their numbers of trees
constexpr std::array<Choice, 4> k_WithBush {{{Tree::Birch, 11}, {Tree::Beech, 8}, {Tree::Oak, 7}, {Tree::OakA, 3}}};
constexpr std::array<Choice, 4> k_WithConiferA {{{Tree::Conifer, 12}, {Tree::Pine, 11}, {Tree::Bush, 2}, {Tree::BushB, 1}}};
constexpr std::array<Choice, 3> k_WithPalmB {{{Tree::Palm, 33}, {Tree::PalmA, 5}, {Tree::PalmC, 1}}};
constexpr std::array<Choice, 4> k_WithPalm {{{Tree::PalmA, 20}, {Tree::PalmB, 15}, {Tree::PalmC, 5}, {Tree::Bush, 1}}};
constexpr std::array<Choice, 4> k_WithPalmA {{{Tree::PalmB, 19}, {Tree::Palm, 19}, {Tree::PalmC, 11}, {Tree::Bush, 7}}};
constexpr std::array<Choice, 3> k_WithPalmC {{{Tree::PalmB, 19}, {Tree::Palm, 17}, {Tree::PalmA, 13}}};
constexpr std::array<Choice, 4> k_WithConifer {{{Tree::ConiferA, 23}, {Tree::BushB, 16}, {Tree::Pine, 8}, {Tree::Bush, 5}}};
constexpr std::array<Choice, 3> k_WithPine {{{Tree::ConiferA, 26}, {Tree::Conifer, 20}, {Tree::Bush, 7}}};
constexpr std::array<Choice, 4> k_WithBushB {{{Tree::ConiferA, 16}, {Tree::Cedar, 10}, {Tree::Pine, 7}, {Tree::Conifer, 6}}};
constexpr std::array<Choice, 4> k_WithOak {{{Tree::OakA, 21}, {Tree::Bush, 17}, {Tree::Palm, 5}, {Tree::PalmB, 3}}};
constexpr std::array<Choice, 4> k_WithOakA {{{Tree::Oak, 15}, {Tree::Bush, 15}, {Tree::Beech, 5}, {Tree::BushA, 4}}};
constexpr std::array<Choice, 4> k_WithBeech {{{Tree::Bush, 10}, {Tree::Birch, 7}, {Tree::PalmC, 7}, {Tree::Oak, 5}}};
constexpr std::array<Choice, 2> k_WithBirch {{{Tree::Bush, 38}, {Tree::Beech, 25}}};
constexpr std::array<Choice, 4> k_WithOlive {{{Tree::Bush, 15}, {Tree::BushB, 8}, {Tree::Oak, 5}, {Tree::OakA, 5}}};
constexpr std::array<Choice, 1> k_WithCedar {{{Tree::BushB, 38}}};
constexpr std::array<Choice, 1> k_WithBushA {{{Tree::BushB, 31}}};
constexpr std::array<Choice, 3> k_WithCypress {{{Tree::Bush, 24}, {Tree::CypressA, 16}, {Tree::Olive, 4}}};
constexpr std::array<Choice, 1> k_WithCypressA {{{Tree::Cypress, 40}}};

// The limits of each kind come from the trees of the lands of the story:
// - the heights from the 2nd to the 98th percentile, the slope at the 95th percentile;
// - the distance from the sea at the 2nd percentile, the scales from the 5th to the 95th percentile;
// - the spread is the median of the groups of the kind, 1.2 times larger, because the outline of a group is smaller
//   than the circle that holds it.
constexpr std::array<KindRule, 19> k_Rules {{
    {Tree::Bush, 2.0f, 113.9f, 1.0649f, 30.0f, 0.0f, k_Anywhere, 1.0f, 2.3f, 2.81f, 59, k_WithBush},
    {Tree::ConiferA, 8.0f, 105.9f, 0.8332f, 53.9f, 0.0f, k_Anywhere, 0.7f, 1.1f, 3.13f, 73, k_WithConiferA},
    {Tree::PalmB, 2.7f, 66.3f, 0.8785f, 30.0f, 0.0f, k_Anywhere, 0.5f, 1.4f, 1.72f, 61, k_WithPalmB},
    {Tree::Palm, 2.7f, 65.7f, 0.7618f, 30.0f, 0.0f, k_Anywhere, 0.8f, 1.4f, 2.40f, 59, k_WithPalm},
    {Tree::Conifer, 13.4f, 112.5f, 0.8601f, 60.0f, 0.0f, k_Anywhere, 0.6f, 1.1f, 3.01f, 46, k_WithConifer},
    {Tree::Pine, 14.7f, 109.8f, 0.9004f, 51.4f, 0.0f, k_Anywhere, 0.7f, 1.0f, 3.18f, 46, k_WithPine},
    {Tree::Beech, 2.7f, 146.7f, 1.0392f, 42.7f, 0.0f, k_Anywhere, 0.6f, 1.1f, 3.61f, 69, k_WithBeech},
    {Tree::OakA, 10.4f, 101.2f, 1.0575f, 58.8f, 0.0f, k_Anywhere, 0.8f, 1.3f, 4.64f, 56, k_WithOakA},
    {Tree::Oak, 2.0f, 98.5f, 0.8391f, 30.0f, 0.0f, k_Anywhere, 0.7f, 1.2f, 4.61f, 46, k_WithOak},
    {Tree::Birch, 2.0f, 146.7f, 0.9457f, 30.0f, 0.0f, k_Anywhere, 0.6f, 1.2f, 2.64f, 38, k_WithBirch},
    {Tree::BushB, 10.7f, 148.7f, 0.7346f, 61.0f, 0.0f, k_Anywhere, 0.6f, 2.8f, 1.27f, 58, k_WithBushB},
    {Tree::PalmA, 2.7f, 126.6f, 1.6643f, 40.1f, 0.0f, k_Anywhere, 0.7f, 1.1f, 1.72f, 44, k_WithPalmA},
    {Tree::Olive, 2.7f, 148.1f, 0.6847f, 40.0f, 0.0f, k_Anywhere, 0.6f, 1.0f, 4.12f, 58, k_WithOlive},
    {Tree::PalmC, 2.1f, 145.0f, 0.6371f, 41.0f, 0.0f, k_Anywhere, 0.3f, 1.1f, 2.11f, 50, k_WithPalmC},
    {Tree::Cedar, 3.9f, 135.5f, 0.4986f, 70.0f, 0.0f, k_Anywhere, 0.5f, 1.0f, 1.76f, 62, k_WithCedar},
    {Tree::BushA, 10.0f, 148.7f, 0.4081f, 70.1f, 0.0f, k_Anywhere, 0.8f, 1.9f, 2.44f, 39, k_WithBushA},
    // The cypresses grow only near a town
    {Tree::Cypress, 36.8f, 148.4f, 0.2254f, 187.0f, 0.0f, 300.0f, 0.6f, 1.0f, 3.25f, 56, k_WithCypress},
    {Tree::CypressA, 35.2f, 113.2f, 0.2419f, 113.5f, 0.0f, 300.0f, 0.7f, 1.0f, 2.30f, 60, k_WithCypressA},
    // The burnt trees stand only on rocky slopes far from a town
    {Tree::Burnt, 55.9f, 63.6f, 0.7590f, 190.0f, 600.0f, k_Anywhere, 1.0f, 1.7f, 3.0f, 100, {}},
}};

const KindRule* RuleOf(Tree tree)
{
	const auto rule = std::ranges::find_if(k_Rules, [tree](const KindRule& entry) { return entry.tree == tree; });
	return rule == k_Rules.end() ? nullptr : &*rule;
}

/// The steps of the count of the groups of trees: each tenth, then 95%, 98% and 100%
constexpr std::array<float, 13> k_SizeSteps {0.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 0.95f, 0.98f, 1.0f};
/// The sizes of the groups at the steps, on all the lands of the story
constexpr std::array<float, 13> k_GroupSizes {1.0f, 1.0f, 1.0f,  2.0f,  3.0f,  3.0f,  4.0f,
                                              6.0f, 8.0f, 15.0f, 24.0f, 39.0f, 207.0f};
/// The sizes of the groups at the steps, on the palm island of the story
constexpr std::array<float, 13> k_JungleGroupSizes {1.0f,  1.0f,  1.0f,  2.0f,  3.0f,   4.0f,  6.0f,
                                                    12.0f, 18.4f, 30.0f, 85.6f, 113.7f, 154.0f};

/// What grows and lies in a biome
struct Planting
{
	/// The stands of trees that start in each square kilometre
	float stands;
	/// The stands grow close together in woods, with open land between the woods
	bool woods;
	/// The stands have the sizes of the groups of the palm island, and their trees stand closer
	bool jungle;
	/// The main kinds of the stands
	std::span<const Choice> mains;
	/// The chance of a dead tree that lies on the ground, and of a rock, at a point of the grid of rocks
	float deadChance;
	float rockChance;
	Stone stone;
};

// The main kinds of the stands of each biome, with the number of groups of each main kind on the lands of the story
constexpr std::array<Choice, 4> k_BeachTrees {{{Tree::PalmB, 59}, {Tree::Palm, 17}, {Tree::PalmA, 3}, {Tree::PalmC, 3}}};
constexpr std::array<Choice, 6> k_GrasslandTrees {
    {{Tree::Bush, 62}, {Tree::Oak, 10}, {Tree::Beech, 7}, {Tree::OakA, 6}, {Tree::Olive, 5}, {Tree::Birch, 1}}};
constexpr std::array<Choice, 6> k_ForestTrees {
    {{Tree::Bush, 31}, {Tree::Conifer, 24}, {Tree::Oak, 10}, {Tree::Beech, 7}, {Tree::OakA, 6}, {Tree::Birch, 1}}};
constexpr std::array<Choice, 5> k_JungleTrees {
    {{Tree::PalmB, 59}, {Tree::Palm, 17}, {Tree::BushB, 11}, {Tree::PalmA, 3}, {Tree::PalmC, 3}}};
constexpr std::array<Choice, 4> k_DesertTrees {{{Tree::Olive, 5}, {Tree::BushB, 2}, {Tree::Cypress, 2}, {Tree::CypressA, 1}}};
constexpr std::array<Choice, 4> k_SwampTrees {{{Tree::Bush, 62}, {Tree::Birch, 1}, {Tree::Cedar, 1}, {Tree::BushA, 1}}};
constexpr std::array<Choice, 1> k_DeadTrees {{{Tree::Burnt, 1}}};
constexpr std::array<Choice, 3> k_NordicTrees {{{Tree::ConiferA, 70}, {Tree::Conifer, 24}, {Tree::Pine, 22}}};
constexpr std::array<Choice, 6> k_AlpineTrees {
    {{Tree::ConiferA, 70}, {Tree::Conifer, 24}, {Tree::Pine, 22}, {Tree::BushB, 11}, {Tree::Cedar, 1}, {Tree::BushA, 1}}};

Planting PlantingOf(Biome biome)
{
	switch (biome)
	{
	case Biome::Beach:
		return {.stands = 50.0f,
		        .woods = false,
		        .jungle = false,
		        .mains = k_BeachTrees,
		        .deadChance = 0.0f,
		        .rockChance = 0.004f,
		        .stone = Stone::Sandstone};
	case Biome::Grassland:
		return {.stands = 37.5f,
		        .woods = true,
		        .jungle = false,
		        .mains = k_GrasslandTrees,
		        .deadChance = 0.0f,
		        .rockChance = 0.01f,
		        .stone = Stone::Limestone};
	case Biome::TemperateForest:
		return {.stands = 275.0f,
		        .woods = true,
		        .jungle = false,
		        .mains = k_ForestTrees,
		        .deadChance = 0.0f,
		        .rockChance = 0.01f,
		        .stone = Stone::Limestone};
	case Biome::DenseJungle:
		return {.stands = 212.5f,
		        .woods = true,
		        .jungle = true,
		        .mains = k_JungleTrees,
		        .deadChance = 0.0f,
		        .rockChance = 0.005f,
		        .stone = Stone::Limestone};
	case Biome::Desert:
		return {.stands = 7.5f,
		        .woods = false,
		        .jungle = false,
		        .mains = k_DesertTrees,
		        .deadChance = 0.0f,
		        .rockChance = 0.03f,
		        .stone = Stone::Sandstone};
	case Biome::Swamp:
		return {.stands = 75.0f,
		        .woods = true,
		        .jungle = false,
		        .mains = k_SwampTrees,
		        .deadChance = 0.01f,
		        .rockChance = 0.0f,
		        .stone = Stone::Limestone};
	case Biome::DeadWoods:
		return {.stands = 12.5f,
		        .woods = false,
		        .jungle = false,
		        .mains = k_DeadTrees,
		        .deadChance = 0.08f,
		        .rockChance = 0.015f,
		        .stone = Stone::Volcanic};
	case Biome::NordicWastes:
		return {.stands = 25.0f,
		        .woods = true,
		        .jungle = false,
		        .mains = k_NordicTrees,
		        .deadChance = 0.0f,
		        .rockChance = 0.03f,
		        .stone = Stone::Chalk};
	case Biome::Alpine:
		return {.stands = 56.25f,
		        .woods = true,
		        .jungle = false,
		        .mains = k_AlpineTrees,
		        .deadChance = 0.0f,
		        .rockChance = 0.05f,
		        .stone = Stone::Limestone};
	case Biome::Sea:
	case Biome::_Count:
		break;
	}
	return {.stands = 0.0f,
	        .woods = false,
	        .jungle = false,
	        .mains = {},
	        .deadChance = 0.0f,
	        .rockChance = 0.0f,
	        .stone = Stone::Limestone};
}

/// The trees of a jungle stand closer than the trees of a stand of the same kind elsewhere
constexpr float k_JungleSpread = 0.8f;
/// The outer part of the radius of a stand is its edge, where the bushes grow more often
constexpr float k_StandEdge = 0.7f;
constexpr uint32_t k_EdgeBushFactor = 2;
/// No two trees stand nearer than this, in metres
constexpr float k_TreeGap = 1.0f;
/// The share of the trees that are still young, and the smallest part of the full size of a young tree
constexpr float k_YoungShare = 0.03f;
constexpr float k_YoungestSize = 0.12f;

// The big forests come from the big forests of the lands of the story:
// - the heights and the slopes from the 5th to the 95th percentile;
// - the distances from the sea and from the town at the 5th percentile;
// - the gap is the smallest distance between two big forests.
constexpr float k_BigForestLowest = 12.9f;
constexpr float k_BigForestHighest = 83.8f;
constexpr float k_BigForestSteepest = 1.2527f;
constexpr float k_BigForestNearestSea = 80.0f;
constexpr float k_BigForestNearestTown = 84.0f;
constexpr float k_BigForestGap = 55.0f;
/// The chance of a big forest at a point of its grid where the ground suits it
constexpr float k_BigForestChance = 0.5f;
/// A big forest stands only in the woods, where the noise of the woods gives at least this share of the stands
constexpr float k_BigForestWoods = 0.5f;
/// The trees and the rocks keep this far from a big forest of scale 1, in metres
constexpr float k_BigForestClearing = 33.0f;

template <typename Value>
struct Weighted
{
	Value value;
	uint32_t weight;
};
/// The kinds of the big forests on the lands of the story, with their numbers
constexpr std::array<Weighted<uint8_t>, 3> k_BigForestKinds {{{0, 62}, {1, 1}, {3, 1}}};
/// The scales of the big forests on the lands of the story, with their numbers
constexpr std::array<Weighted<float>, 10> k_BigForestScales {
    {{0.58f, 3}, {0.64f, 7}, {0.68f, 4}, {0.76f, 1}, {0.78f, 6}, {0.88f, 2}, {0.89f, 6}, {0.99f, 11}, {1.0f, 23}, {1.07f, 1}}};

/// A pick from the entries by their weights
template <typename Value, size_t N>
Value PickWeighted(const std::array<Weighted<Value>, N>& entries, Random& random)
{
	uint32_t total = 0;
	for (const auto& entry : entries)
	{
		total += entry.weight;
	}
	auto pick = random.NextBelow(total);
	for (const auto& entry : entries)
	{
		if (pick < entry.weight)
		{
			return entry.value;
		}
		pick -= entry.weight;
	}
	return entries.back().value;
}

/// The biomes where the big forests grow: the big forests of the story stand on grass and rock, away from the beaches
bool BigForestGrowsIn(Biome biome)
{
	switch (biome)
	{
	case Biome::Grassland:
	case Biome::TemperateForest:
	case Biome::DenseJungle:
	case Biome::NordicWastes:
	case Biome::Alpine:
		return true;
	default:
		return false;
	}
}

/// A pick from the trees by the weights that the function gives them, or nothing when all weights are 0
template <typename WeightOf>
std::optional<Tree> PickTree(std::span<const Choice> trees, Random& random, WeightOf&& weightOf)
{
	uint32_t total = 0;
	for (const auto& choice : trees)
	{
		total += weightOf(choice);
	}
	if (total == 0)
	{
		return std::nullopt;
	}
	auto pick = random.NextBelow(total);
	for (const auto& choice : trees)
	{
		const auto weight = weightOf(choice);
		if (pick < weight)
		{
			return choice.tree;
		}
		pick -= weight;
	}
	return std::nullopt;
}

/// The largest difference of altitude between a corner and the four corners next to it
float SlopeAt(const Grid<float>& heights, int x, int z)
{
	float steepest = 0.0f;
	for (size_t i = 0; i < 4; ++i)
	{
		const int nx = x + k_Neighbours[i].dx;
		const int nz = z + k_Neighbours[i].dz;
		if (heights.Contains(nx, nz))
		{
			steepest = std::max(steepest, std::fabs(heights.At(x, z) - heights.At(nx, nz)));
		}
	}
	return steepest;
}

/// The slope of the ground at a corner as the rise for each metre, from the differences across the corner. The measure
/// of the lands of the story uses the same differences.
float GradientAt(const Grid<float>& heights, int x, int z)
{
	const int side = heights.side;
	const int left = std::max(x - 1, 0);
	const int right = std::min(x + 1, side - 1);
	const int down = std::max(z - 1, 0);
	const int up = std::min(z + 1, side - 1);
	const float metresX = static_cast<float>(std::max(right - left, 1)) * k_MetresPerCorner;
	const float metresZ = static_cast<float>(std::max(up - down, 1)) * k_MetresPerCorner;
	const float dx = (heights.At(right, z) - heights.At(left, z)) * k_MetresPerAltitude / metresX;
	const float dz = (heights.At(x, up) - heights.At(x, down)) * k_MetresPerAltitude / metresZ;
	return std::sqrt((dx * dx) + (dz * dz));
}

/// The trees placed so far, in squares of the map, so that a new tree finds its neighbours quickly
class TreeSquares
{
public:
	/// Whether a tree stands nearer to the point than the gap, in metres
	[[nodiscard]] bool AnyNear(float x, float z, float gap) const
	{
		const auto sx = SquareOf(x);
		const auto sz = SquareOf(z);
		for (int32_t i = sx - 1; i <= sx + 1; ++i)
		{
			for (int32_t j = sz - 1; j <= sz + 1; ++j)
			{
				const auto found = _squares.find(Key(i, j));
				if (found == _squares.end())
				{
					continue;
				}
				const bool near = std::ranges::any_of(found->second, [x, z, gap](const std::pair<float, float>& tree) {
					const float dx = tree.first - x;
					const float dz = tree.second - z;
					return (dx * dx) + (dz * dz) < gap * gap;
				});
				if (near)
				{
					return true;
				}
			}
		}
		return false;
	}

	void Add(float x, float z) { _squares[Key(SquareOf(x), SquareOf(z))].emplace_back(x, z); }

private:
	/// The side of a square in metres, larger than the gap between trees
	static constexpr float k_Side = 4.0f;
	[[nodiscard]] static int32_t SquareOf(float metres) { return static_cast<int32_t>(std::floor(metres / k_Side)); }
	[[nodiscard]] static uint64_t Key(int32_t i, int32_t j)
	{
		return (static_cast<uint64_t>(static_cast<uint32_t>(i)) << 32u) | static_cast<uint64_t>(static_cast<uint32_t>(j));
	}
	std::unordered_map<uint64_t, std::vector<std::pair<float, float>>> _squares;
};

/// The shapes of rock, the round ones more often
constexpr std::array<RockShape, 12> k_RockShapes {
    RockShape::Rock,     RockShape::Rock, RockShape::Rock, RockShape::Boulder1, RockShape::Boulder2, RockShape::Boulder3,
    RockShape::Boulder1, RockShape::Flat, RockShape::Long, RockShape::Sharp,    RockShape::Square,   RockShape::Rock};

/// The ground of the land with the circles that the big forests keep clear
class Placement
{
public:
	explicit Placement(const Ground& ground)
	    : _ground(ground)
	    , _side(ground.biomes.side)
	{
	}

	[[nodiscard]] int Side() const { return _side; }

	/// A feature can stand at a place in corners in the map, out of the water, off the rivers and out of the clearings
	[[nodiscard]] bool Free(float x, float z) const
	{
		const auto last = static_cast<float>(_side - 1);
		if (x < 0.0f || z < 0.0f || x > last || z > last)
		{
			return false;
		}
		const auto [cx, cz] = CornerOf(x, z);
		if (_ground.keepClear.At(cx, cz) != 0 || _ground.heights.At(cx, cz) <= terrain::k_SeaLevel + 1.0f)
		{
			return false;
		}
		const auto outside = [x, z](const Clearing& clearing) {
			const float dx = x - clearing.x;
			const float dz = z - clearing.z;
			return (dx * dx) + (dz * dz) >= clearing.radius * clearing.radius;
		};
		return std::ranges::all_of(_ground.clearings, outside) && std::ranges::all_of(_bigForests, outside);
	}

	/// The ground at a place in corners
	[[nodiscard]] Spot SpotAt(float x, float z) const
	{
		const auto [cx, cz] = CornerOf(x, z);
		const float tx = (x - _ground.townX) * k_MetresPerCorner;
		const float tz = (z - _ground.townZ) * k_MetresPerCorner;
		return {.height = _ground.heights.At(cx, cz) * k_MetresPerAltitude,
		        .slope = GradientAt(_ground.heights, cx, cz),
		        .sea = _ground.seaDistance.At(cx, cz) * k_MetresPerCorner,
		        .town = std::sqrt((tx * tx) + (tz * tz))};
	}

	[[nodiscard]] Biome BiomeAt(float x, float z) const
	{
		const auto [cx, cz] = CornerOf(x, z);
		return _ground.biomes.At(cx, cz);
	}

	[[nodiscard]] float SlopeUnitsAt(float x, float z) const
	{
		const auto [cx, cz] = CornerOf(x, z);
		return SlopeAt(_ground.heights, cx, cz);
	}

	/// A big forest keeps the trees and the rocks out of a circle round it, in corners
	void AddBigForest(float x, float z, float radius) { _bigForests.push_back({.x = x, .z = z, .radius = radius}); }
	[[nodiscard]] std::span<const Clearing> BigForests() const { return _bigForests; }

private:
	/// The corner nearest a place in corners
	[[nodiscard]] std::pair<int, int> CornerOf(float x, float z) const
	{
		return {std::clamp(static_cast<int>(x + 0.5f), 0, _side - 1), std::clamp(static_cast<int>(z + 0.5f), 0, _side - 1)};
	}

	const Ground& _ground;
	int _side;
	std::vector<Clearing> _bigForests;
};

/// The share of the stands that the woods let start at a place in corners: many where the noise is high, few where it
/// is low
float WoodsAt(uint64_t seed, float x, float z, float last)
{
	const float woods = noise::Fractal(StageSeed(seed, 41), x / last * 14.0f, z / last * 14.0f, 3);
	return std::max(noise::SmoothStep(-0.15f, 0.35f, woods), 0.15f);
}

/// The random numbers of a point of a grid, from the seed of the stage and the place of the point only
Random RandomAt(uint64_t stageSeed, int i, int j)
{
	return Random(static_cast<uint64_t>(Hash(stageSeed, i, j)) | (static_cast<uint64_t>(Hash(stageSeed, j, i)) << 32u));
}

void PlaceBigForests(Placement& placement, uint64_t seed, const Settings& settings, std::vector<Feature>& placed)
{
	const auto stageSeed = StageSeed(seed, 42);
	const float last = static_cast<float>(placement.Side() - 1);
	const int points = static_cast<int>(last / settings.bigForestSpacing);
	constexpr float k_Gap = k_BigForestGap / k_MetresPerCorner;
	for (int i = 0; i < points; ++i)
	{
		for (int j = 0; j < points; ++j)
		{
			auto random = RandomAt(stageSeed, i, j);
			const float x = (static_cast<float>(i) + random.NextUnit()) * settings.bigForestSpacing;
			const float z = (static_cast<float>(j) + random.NextUnit()) * settings.bigForestSpacing;
			const float roll = random.NextUnit();
			const auto kind = PickWeighted(k_BigForestKinds, random);
			const float scale = PickWeighted(k_BigForestScales, random);
			const float angle = random.NextUnit() * k_FullTurn;
			if (roll >= k_BigForestChance || !placement.Free(x, z) || !BigForestGrowsIn(placement.BiomeAt(x, z)) ||
			    WoodsAt(seed, x, z, last) < k_BigForestWoods)
			{
				continue;
			}
			const auto spot = placement.SpotAt(x, z);
			if (spot.height < k_BigForestLowest || spot.height > k_BigForestHighest || spot.slope > k_BigForestSteepest ||
			    spot.sea < k_BigForestNearestSea || spot.town < k_BigForestNearestTown)
			{
				continue;
			}
			const bool crowded = std::ranges::any_of(placement.BigForests(), [x, z](const Clearing& other) {
				const float dx = other.x - x;
				const float dz = other.z - z;
				return (dx * dx) + (dz * dz) < k_Gap * k_Gap;
			});
			if (crowded)
			{
				continue;
			}
			placement.AddBigForest(x, z, k_BigForestClearing * scale / k_MetresPerCorner);
			placed.push_back({.kind = Kind::BigForest,
			                  .type = kind,
			                  .x = x * k_MetresPerCorner,
			                  .z = z * k_MetresPerCorner,
			                  .angle = angle,
			                  .size = scale,
			                  .maxSize = scale});
		}
	}
}

/// The trees of one stand round its middle in corners
void PlaceStand(const Placement& placement, float x, float z, Tree main, uint32_t count, bool jungle, Random& random,
                TreeSquares& trees, std::vector<Feature>& placed)
{
	const auto& rule = *RuleOf(main);
	const float radius =
	    rule.spread * (jungle ? k_JungleSpread : 1.0f) * std::sqrt(static_cast<float>(count)) / k_MetresPerCorner;
	for (uint32_t t = 0; t < count; ++t)
	{
		// A point in the circle of the stand. A stand of one tree has its tree at the middle.
		float dx = 0.0f;
		float dz = 0.0f;
		if (count > 1)
		{
			do
			{
				dx = random.NextRange(-1.0f, 1.0f);
				dz = random.NextRange(-1.0f, 1.0f);
			} while ((dx * dx) + (dz * dz) > 1.0f);
		}
		const bool edge = (dx * dx) + (dz * dz) > k_StandEdge * k_StandEdge;
		const float kindRoll = random.NextUnit();
		const float angle = random.NextUnit() * k_FullTurn;
		const float scaleRoll = random.NextUnit();
		const float youngRoll = random.NextUnit();
		const float youngSize = random.NextRange(k_YoungestSize, 1.0f);
		const auto companion = PickTree(rule.companions, random, [edge](const Choice& choice) {
			const bool bush = choice.tree == Tree::Bush || choice.tree == Tree::BushB;
			return edge && bush ? choice.weight * k_EdgeBushFactor : choice.weight;
		});
		const float tx = x + (dx * radius);
		const float tz = z + (dz * radius);
		if (!placement.Free(tx, tz) || placement.BiomeAt(tx, tz) == Biome::Sea)
		{
			continue;
		}
		const auto spot = placement.SpotAt(tx, tz);
		auto tree = main;
		if (kindRoll * 100.0f >= static_cast<float>(rule.mainShare) && companion && Suits(*companion, spot))
		{
			tree = *companion;
		}
		if (!Suits(tree, spot))
		{
			continue;
		}
		const float metresX = tx * k_MetresPerCorner;
		const float metresZ = tz * k_MetresPerCorner;
		if (trees.AnyNear(metresX, metresZ, k_TreeGap))
		{
			continue;
		}
		trees.Add(metresX, metresZ);
		const auto& kind = *RuleOf(tree);
		const float full = kind.smallest + ((kind.largest - kind.smallest) * scaleRoll);
		// Almost all trees of the story are at their full size; a few are still young
		const float now = youngRoll < k_YoungShare ? full * youngSize : full;
		placed.push_back({.kind = Kind::Tree,
		                  .type = static_cast<uint8_t>(tree),
		                  .x = metresX,
		                  .z = metresZ,
		                  .angle = angle,
		                  .size = now,
		                  .maxSize = full});
	}
}

void PlaceStands(const Placement& placement, uint64_t seed, const Settings& settings, std::vector<Feature>& placed)
{
	const auto standSeed = StageSeed(seed, 43);
	const float last = static_cast<float>(placement.Side() - 1);
	const int points = static_cast<int>(last / settings.standSpacing);
	const float squareSide = settings.standSpacing * k_MetresPerCorner;
	const float squareKm = squareSide * squareSide / 1.0e6f;
	TreeSquares trees;
	for (int i = 0; i < points; ++i)
	{
		for (int j = 0; j < points; ++j)
		{
			auto random = RandomAt(standSeed, i, j);
			const float x = (static_cast<float>(i) + random.NextUnit()) * settings.standSpacing;
			const float z = (static_cast<float>(j) + random.NextUnit()) * settings.standSpacing;
			const float roll = random.NextUnit();
			const float sizeRoll = random.NextUnit();
			if (!placement.Free(x, z))
			{
				continue;
			}
			const auto planting = PlantingOf(placement.BiomeAt(x, z));
			float chance = planting.stands * squareKm;
			if (planting.woods)
			{
				chance *= WoodsAt(seed, x, z, last);
			}
			if (roll >= chance)
			{
				continue;
			}
			const auto centre = placement.SpotAt(x, z);
			const auto main = PickTree(planting.mains, random, [&centre](const Choice& choice) {
				return Suits(choice.tree, centre) ? choice.weight : 0;
			});
			if (main)
			{
				PlaceStand(placement, x, z, *main, StandSize(sizeRoll, planting.jungle), planting.jungle, random, trees,
				           placed);
			}
		}
	}
}

void PlaceRocks(const Placement& placement, uint64_t seed, const Settings& settings, std::vector<Feature>& placed)
{
	const auto pointSeed = StageSeed(seed, 40);
	const int points = static_cast<int>(static_cast<float>(placement.Side() - 1) / settings.spacing);
	for (int i = 0; i < points; ++i)
	{
		for (int j = 0; j < points; ++j)
		{
			auto random = RandomAt(pointSeed, i, j);
			const float x = (static_cast<float>(i) + random.NextUnit()) * settings.spacing;
			const float z = (static_cast<float>(j) + random.NextUnit()) * settings.spacing;
			const float roll = random.NextUnit();
			const float angle = random.NextUnit() * k_FullTurn;
			const float size = random.NextRange(0.8f, 1.05f);
			if (!placement.Free(x, z) || placement.SlopeUnitsAt(x, z) > settings.rockSlope)
			{
				continue;
			}
			const auto planting = PlantingOf(placement.BiomeAt(x, z));
			const auto metresX = x * k_MetresPerCorner;
			const auto metresZ = z * k_MetresPerCorner;
			if (roll < planting.deadChance)
			{
				placed.push_back({.kind = Kind::Static,
				                  .type = k_DeadTreeStatic,
				                  .x = metresX,
				                  .z = metresZ,
				                  .angle = angle,
				                  .size = size,
				                  .maxSize = size});
			}
			else if (roll < planting.deadChance + planting.rockChance)
			{
				const auto shape = k_RockShapes[random.NextBelow(static_cast<uint32_t>(k_RockShapes.size()))];
				const float scale = random.NextRange(0.6f, 1.8f);
				placed.push_back({.kind = Kind::Static,
				                  .type = RockType(shape, planting.stone),
				                  .x = metresX,
				                  .z = metresZ,
				                  .angle = angle,
				                  .size = scale,
				                  .maxSize = scale});
			}
		}
	}
}
} // namespace

bool features::Suits(Tree tree, const Spot& spot)
{
	const auto* rule = RuleOf(tree);
	return rule != nullptr && spot.height >= rule->lowest && spot.height <= rule->highest && spot.slope <= rule->steepest &&
	       spot.sea >= rule->nearestSea && spot.town >= rule->nearestTown && spot.town <= rule->farthestTown;
}

uint32_t features::StandSize(float unit, bool jungle)
{
	const auto& sizes = jungle ? k_JungleGroupSizes : k_GroupSizes;
	const float u = std::clamp(unit, 0.0f, 1.0f);
	size_t step = 1;
	while (step + 1 < k_SizeSteps.size() && u > k_SizeSteps[step])
	{
		++step;
	}
	const float low = k_SizeSteps[step - 1];
	const float high = k_SizeSteps[step];
	const float along = (u - low) / (high - low);
	const float size = sizes[step - 1] + ((sizes[step] - sizes[step - 1]) * along);
	return std::max(static_cast<uint32_t>(std::floor(size + 0.5f)), 1u);
}

std::vector<Feature> features::Place(const Ground& ground, uint64_t seed, const Settings& settings)
{
	Placement placement(ground);
	std::vector<Feature> bigForests;
	PlaceBigForests(placement, seed, settings, bigForests);
	std::vector<Feature> placed;
	PlaceStands(placement, seed, settings, placed);
	PlaceRocks(placement, seed, settings, placed);

	// Too many features: keep an even share of them all over the land
	if (placed.size() > settings.most)
	{
		std::vector<Feature> kept;
		kept.reserve(settings.most);
		for (size_t i = 0; i < placed.size(); ++i)
		{
			if ((i * settings.most) / placed.size() != ((i + 1) * settings.most) / placed.size())
			{
				kept.push_back(placed[i]);
			}
		}
		placed = std::move(kept);
	}
	// The cap does not remove a big forest. The big forests come first in the list.
	bigForests.insert(bigForests.end(), placed.begin(), placed.end());
	return bigForests;
}

std::vector<Forest> features::GroupForests(std::span<Feature> features, float nearX, float nearZ,
                                           const ForestSettings& settings)
{
	// The trees of each square, in the order of the features
	std::map<std::pair<int, int>, std::vector<size_t>> squares;
	for (size_t i = 0; i < features.size(); ++i)
	{
		const auto& feature = features[i];
		if (feature.kind != Kind::Tree)
		{
			continue;
		}
		const auto key = std::pair {static_cast<int>(std::floor(feature.x / settings.square)),
		                            static_cast<int>(std::floor(feature.z / settings.square))};
		squares[key].push_back(i);
	}

	struct Candidate
	{
		float distance;
		std::pair<int, int> key;
	};
	std::vector<Candidate> candidates;
	for (const auto& [key, trees] : squares)
	{
		if (trees.size() < settings.fewest)
		{
			continue;
		}
		const float dx = ((static_cast<float>(key.first) + 0.5f) * settings.square) - nearX;
		const float dz = ((static_cast<float>(key.second) + 0.5f) * settings.square) - nearZ;
		candidates.push_back({.distance = (dx * dx) + (dz * dz), .key = key});
	}
	std::ranges::sort(candidates, [](const Candidate& a, const Candidate& b) {
		return a.distance != b.distance ? a.distance < b.distance : a.key < b.key;
	});

	std::vector<Forest> forests;
	for (const auto& candidate : candidates)
	{
		if (forests.size() >= settings.most)
		{
			break;
		}
		const auto id = static_cast<uint32_t>(forests.size() + 1);
		const auto& trees = squares.at(candidate.key);
		const auto& first = features[trees.front()];
		forests.push_back({.id = id, .x = first.x, .z = first.z});
		for (const auto tree : trees)
		{
			features[tree].forest = id;
		}
	}
	return forests;
}
