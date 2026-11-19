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
	/// The stands have the sizes of the groups of the palm island
	bool jungle;
	/// The main kinds of the stands
	std::span<const Choice> mains;
	/// The chance of a dead tree that lies on the ground at a point of its grid
	float deadChance;
	/// The clumps of rocks that start in each square kilometre of open ground. More clumps start near the trees.
	float rockClumps;
	/// The stone of the rocks: limestone, sandstone on dry ground and volcanic stone in the dead woods
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
// The swamps have the main kinds of the groups by the streams of the lands of the story: the groups whose trees stand
// at a median of 30 m or nearer to a stream. The hedges of those groups stay with the towns.
constexpr std::array<Choice, 13> k_SwampTrees {{{Tree::Bush, 37},
                                                {Tree::OakA, 12},
                                                {Tree::Oak, 10},
                                                {Tree::Birch, 9},
                                                {Tree::Olive, 6},
                                                {Tree::ConiferA, 4},
                                                {Tree::Beech, 4},
                                                {Tree::PalmB, 4},
                                                {Tree::PalmA, 3},
                                                {Tree::Conifer, 3},
                                                {Tree::PalmC, 2},
                                                {Tree::BushB, 2},
                                                {Tree::Palm, 1}}};
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
		        .rockClumps = 4.0f,
		        .stone = Stone::Limestone};
	case Biome::Grassland:
		return {.stands = 150.0f,
		        .woods = true,
		        .jungle = false,
		        .mains = k_GrasslandTrees,
		        .deadChance = 0.0f,
		        .rockClumps = 8.0f,
		        .stone = Stone::Limestone};
	case Biome::TemperateForest:
		return {.stands = 170.0f,
		        .woods = true,
		        .jungle = false,
		        .mains = k_ForestTrees,
		        .deadChance = 0.0f,
		        .rockClumps = 8.0f,
		        .stone = Stone::Limestone};
	case Biome::DenseJungle:
		return {.stands = 212.5f,
		        .woods = true,
		        .jungle = true,
		        .mains = k_JungleTrees,
		        .deadChance = 0.0f,
		        .rockClumps = 6.0f,
		        .stone = Stone::Limestone};
	case Biome::Desert:
		return {.stands = 7.5f,
		        .woods = false,
		        .jungle = false,
		        .mains = k_DesertTrees,
		        .deadChance = 0.0f,
		        .rockClumps = 30.0f,
		        .stone = Stone::Sandstone};
	case Biome::Swamp:
		return {.stands = 75.0f,
		        .woods = true,
		        .jungle = false,
		        .mains = k_SwampTrees,
		        .deadChance = 0.01f,
		        .rockClumps = 3.0f,
		        .stone = Stone::Limestone};
	case Biome::DeadWoods:
		return {.stands = 12.5f,
		        .woods = false,
		        .jungle = false,
		        .mains = k_DeadTrees,
		        .deadChance = 0.08f,
		        .rockClumps = 20.0f,
		        .stone = Stone::Volcanic};
	case Biome::NordicWastes:
		return {.stands = 60.0f,
		        .woods = true,
		        .jungle = false,
		        .mains = k_NordicTrees,
		        .deadChance = 0.0f,
		        .rockClumps = 15.0f,
		        .stone = Stone::Limestone};
	case Biome::Alpine:
		return {.stands = 110.0f,
		        .woods = true,
		        .jungle = false,
		        .mains = k_AlpineTrees,
		        .deadChance = 0.0f,
		        .rockClumps = 20.0f,
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
	        .rockClumps = 0.0f,
	        .stone = Stone::Limestone};
}

/// On the lands of the story, a group of 21 trees or more is looser than a small group: for the same number of trees,
/// its radius is 1.5 times larger (18.5 m against 6.4 m at the median sizes of the two classes).
constexpr uint32_t k_LargeStand = 21;
constexpr float k_LargeStandSpread = 1.5f;
/// The outer part of the radius of a stand is its edge, where the bushes grow more often
constexpr float k_StandEdge = 0.7f;
constexpr uint32_t k_EdgeBushFactor = 2;
/// A stand tries this many points for each of its trees, so that it keeps its size on rough ground
constexpr uint32_t k_StandTries = 2;
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
constexpr float k_BigForestChance = 0.8f;
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

/// The largest difference of height between a corner and the four corners next to it, in height units of 0.67 m. On the
/// lands of the story, one height unit is one altitude unit.
float SlopeAt(const Grid<float>& heights, const HeightCurve& curve, int x, int z)
{
	float steepest = 0.0f;
	for (size_t i = 0; i < 4; ++i)
	{
		const int nx = x + k_Neighbours[i].dx;
		const int nz = z + k_Neighbours[i].dz;
		if (heights.Contains(nx, nz))
		{
			steepest = std::max(steepest, std::fabs(curve.UnitsOf(heights.At(x, z)) - curve.UnitsOf(heights.At(nx, nz))));
		}
	}
	return steepest;
}

/// The slope of the ground at a corner as the rise for each metre, from the differences across the corner. The measure
/// of the lands of the story uses the same differences. The rise is in the metres of the curve.
float GradientAt(const Grid<float>& heights, const HeightCurve& curve, int x, int z)
{
	const int side = heights.side;
	const int left = std::max(x - 1, 0);
	const int right = std::min(x + 1, side - 1);
	const int down = std::max(z - 1, 0);
	const int up = std::min(z + 1, side - 1);
	const float metresX = static_cast<float>(std::max(right - left, 1)) * k_MetresPerCorner;
	const float metresZ = static_cast<float>(std::max(up - down, 1)) * k_MetresPerCorner;
	// On the curve of the lands of the story, the difference of the altitudes gives the same numbers as before the curve
	const auto rise = [&heights, &curve](int ax, int az, int bx, int bz) {
		return curve.IsLinear() ? (heights.At(ax, az) - heights.At(bx, bz)) * k_MetresPerAltitude
		                        : curve.MetresOf(heights.At(ax, az)) - curve.MetresOf(heights.At(bx, bz));
	};
	const float dx = rise(right, z, left, z) / metresX;
	const float dz = rise(x, up, x, down) / metresZ;
	return std::sqrt((dx * dx) + (dz * dz));
}

/// The trees or the rocks placed so far, in squares of the map, so that a new one finds its neighbours quickly
class PointSquares
{
public:
	/// The side of a square in metres must be at least the largest gap that AnyNear looks for
	explicit PointSquares(float side = 4.0f)
	    : _side(side)
	{
	}

	/// Whether a point lies nearer to the place than the gap, in metres
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
	[[nodiscard]] int32_t SquareOf(float metres) const { return static_cast<int32_t>(std::floor(metres / _side)); }
	[[nodiscard]] static uint64_t Key(int32_t i, int32_t j)
	{
		return (static_cast<uint64_t>(static_cast<uint32_t>(i)) << 32u) | static_cast<uint64_t>(static_cast<uint32_t>(j));
	}
	float _side;
	std::unordered_map<uint64_t, std::vector<std::pair<float, float>>> _squares;
};

/// The look of one rock of the story: its shape, its scale, its height above the land in metres and its lean about the
/// two horizontal axes in radians
struct RockPose
{
	RockShape shape;
	float scale;
	float altitude;
	float xAngle;
	float zAngle;
};

// The poses are rocks of the lands of the story, 64 for each class, from the smallest scale to the largest. The choice
// keeps the spread of the scales, the heights, the leans and the shapes of all the rocks of the class. The few rocks
// more than 3 m above the land are not in the choice: they lie on steep slopes or on other things of the story.
constexpr std::array<RockPose, 64> k_PlainPoses {{
    {RockShape::Rock, 0.160f, 0.013f, -0.548f, 0.118f},  {RockShape::Rock, 0.241f, 0.206f, -0.264f, 0.006f},
    {RockShape::Rock, 0.241f, 0.294f, -0.404f, -0.524f}, {RockShape::Rock, 0.241f, 0.238f, -0.551f, 0.724f},
    {RockShape::Rock, 0.241f, 0.205f, -0.190f, 0.069f},  {RockShape::Rock, 0.257f, 0.303f, -0.899f, -0.516f},
    {RockShape::Rock, 0.263f, 0.212f, -0.758f, 0.720f},  {RockShape::Rock, 0.264f, 0.076f, -0.340f, -0.069f},
    {RockShape::Rock, 0.267f, 0.372f, 0.156f, 0.081f},   {RockShape::Rock, 0.267f, 0.190f, -0.423f, -0.536f},
    {RockShape::Rock, 0.267f, 0.278f, -0.234f, 0.704f},  {RockShape::Rock, 0.267f, 0.515f, 1.012f, 2.447f},
    {RockShape::Rock, 0.267f, 0.641f, 1.193f, -2.650f},  {RockShape::Rock, 0.279f, 0.571f, 1.049f, -2.996f},
    {RockShape::Rock, 0.279f, 0.275f, -0.478f, -0.034f}, {RockShape::Rock, 0.339f, 0.380f, -0.670f, -0.388f},
    {RockShape::Rock, 0.358f, 0.242f, -0.470f, -0.179f}, {RockShape::Rock, 0.363f, 0.352f, 0.171f, 0.232f},
    {RockShape::Rock, 0.365f, 0.950f, 1.060f, 2.697f},   {RockShape::Rock, 0.365f, 0.412f, 0.177f, 0.578f},
    {RockShape::Rock, 0.366f, 0.469f, 0.298f, 0.356f},   {RockShape::Rock, 0.366f, 0.533f, -0.225f, -1.081f},
    {RockShape::Rock, 0.368f, 0.342f, -0.126f, 0.807f},  {RockShape::Rock, 0.369f, 0.328f, -0.299f, 0.604f},
    {RockShape::Rock, 0.382f, 0.899f, 0.635f, 1.589f},   {RockShape::Rock, 0.384f, 0.212f, -0.310f, 0.197f},
    {RockShape::Rock, 0.387f, 0.049f, -0.023f, 0.193f},  {RockShape::Rock, 0.423f, 0.474f, -0.502f, -0.372f},
    {RockShape::Rock, 0.423f, 0.678f, -0.498f, 0.683f},  {RockShape::Rock, 0.455f, 0.349f, -0.517f, 0.076f},
    {RockShape::Rock, 0.474f, 0.305f, -0.420f, 0.021f},  {RockShape::Rock, 0.474f, 0.613f, 0.278f, 0.687f},
    {RockShape::Rock, 0.474f, 0.382f, -0.794f, 0.447f},  {RockShape::Rock, 0.491f, 0.396f, -0.478f, 0.306f},
    {RockShape::Rock, 0.557f, 0.924f, 0.221f, 0.282f},   {RockShape::Rock, 0.559f, 0.425f, -0.437f, 0.162f},
    {RockShape::Rock, 0.577f, -0.007f, -0.465f, 0.246f}, {RockShape::Rock, 0.577f, -0.009f, -0.454f, 0.249f},
    {RockShape::Rock, 0.582f, 0.824f, 0.636f, 0.292f},   {RockShape::Rock, 0.597f, 0.510f, -0.579f, 0.019f},
    {RockShape::Rock, 0.597f, 1.346f, 1.113f, -2.117f},  {RockShape::Rock, 0.605f, 0.905f, -0.482f, 0.556f},
    {RockShape::Rock, 0.605f, 0.956f, 0.299f, -0.045f},  {RockShape::Rock, 0.605f, 2.012f, -0.274f, -2.861f},
    {RockShape::Rock, 0.640f, 0.641f, -0.461f, 0.308f},  {RockShape::Rock, 0.681f, 0.000f, -0.501f, -0.334f},
    {RockShape::Rock, 0.749f, 0.787f, -0.350f, 0.727f},  {RockShape::Rock, 0.780f, 0.875f, -0.501f, -0.384f},
    {RockShape::Rock, 0.784f, 0.000f, 0.764f, -0.366f},  {RockShape::Rock, 0.787f, 0.847f, -0.608f, -0.158f},
    {RockShape::Rock, 0.920f, 0.993f, -0.565f, -0.195f}, {RockShape::Rock, 1.000f, 1.020f, -0.374f, 0.186f},
    {RockShape::Rock, 1.000f, 0.046f, -0.528f, 0.201f},  {RockShape::Rock, 1.000f, 0.020f, -0.477f, 0.261f},
    {RockShape::Rock, 1.000f, 0.136f, -0.424f, -0.307f}, {RockShape::Rock, 1.000f, 0.020f, -0.477f, 0.261f},
    {RockShape::Rock, 1.000f, 0.187f, 0.089f, 0.410f},   {RockShape::Rock, 1.000f, 0.031f, -0.486f, 0.429f},
    {RockShape::Rock, 1.000f, 0.023f, -0.477f, 0.348f},  {RockShape::Rock, 1.000f, 0.020f, -0.477f, 0.261f},
    {RockShape::Rock, 1.000f, 0.216f, 0.130f, 0.430f},   {RockShape::Rock, 1.000f, 0.060f, -0.378f, 0.260f},
    {RockShape::Rock, 1.000f, 0.020f, -0.477f, 0.261f},  {RockShape::Rock, 1.000f, 0.020f, -0.477f, 0.261f},
}};
constexpr std::array<RockPose, 64> k_StonePoses {{
    {RockShape::Rock, 0.219f, -0.025f, 0.064f, 0.343f},   {RockShape::Rock, 0.279f, -0.051f, 0.055f, -0.099f},
    {RockShape::Rock, 0.339f, 0.042f, -0.071f, -0.575f},  {RockShape::Rock, 0.339f, -0.086f, 0.000f, 0.000f},
    {RockShape::Rock, 0.339f, 0.118f, 0.836f, 0.125f},    {RockShape::Rock, 0.339f, -0.051f, 0.002f, 0.244f},
    {RockShape::Rock, 0.370f, -0.012f, 0.204f, 0.039f},   {RockShape::Rock, 0.399f, 0.051f, 0.121f, 0.648f},
    {RockShape::Rock, 0.399f, -0.107f, 0.000f, 0.000f},   {RockShape::Rock, 0.399f, -0.024f, -0.430f, 0.012f},
    {RockShape::Rock, 0.399f, 0.274f, -1.187f, 0.526f},   {RockShape::Rock, 0.399f, -0.029f, -0.309f, -0.206f},
    {RockShape::Rock, 0.399f, 0.054f, 0.052f, -0.572f},   {RockShape::Rock, 0.459f, -0.126f, 0.000f, 0.000f},
    {RockShape::Rock, 0.459f, -0.126f, 0.000f, 0.000f},   {RockShape::Rock, 0.459f, -0.060f, -0.162f, 0.038f},
    {RockShape::Rock, 0.459f, -0.126f, 0.000f, 0.000f},   {RockShape::Rock, 0.459f, -0.126f, 0.000f, 0.000f},
    {RockShape::Rock, 0.459f, -0.126f, 0.000f, 0.000f},   {RockShape::Rock, 0.519f, 0.027f, 0.487f, 0.442f},
    {RockShape::Rock, 0.520f, 0.069f, 0.016f, -0.614f},   {RockShape::Rock, 0.520f, -0.145f, 0.000f, 0.000f},
    {RockShape::Rock, 0.579f, -0.163f, 0.000f, 0.000f},   {RockShape::Rock, 0.579f, -0.163f, 0.000f, 0.000f},
    {RockShape::Rock, 0.579f, -0.037f, -0.204f, -0.209f}, {RockShape::Rock, 0.580f, 0.074f, -0.007f, -0.420f},
    {RockShape::Rock, 0.639f, 0.075f, 0.007f, -0.390f},   {RockShape::Rock, 0.639f, 0.000f, -0.046f, 0.505f},
    {RockShape::Rock, 0.639f, 0.561f, -0.828f, -1.613f},  {RockShape::Rock, 0.640f, -0.149f, 0.000f, 0.000f},
    {RockShape::Rock, 0.681f, 1.216f, 0.222f, 2.263f},    {RockShape::Rock, 0.699f, -0.012f, -0.243f, -0.199f},
    {RockShape::Rock, 0.699f, 0.089f, -0.021f, 0.489f},   {RockShape::Rock, 0.699f, 0.039f, -0.368f, 0.402f},
    {RockShape::Rock, 0.759f, -0.002f, -0.322f, -0.151f}, {RockShape::Rock, 0.759f, 0.101f, -0.041f, -0.503f},
    {RockShape::Rock, 0.759f, 0.120f, 0.032f, -0.698f},   {RockShape::Rock, 0.759f, 0.113f, -0.075f, -0.612f},
    {RockShape::Rock, 0.759f, 0.011f, -0.360f, 0.240f},   {RockShape::Rock, 0.759f, -0.025f, 0.327f, 0.101f},
    {RockShape::Rock, 0.759f, 0.095f, -0.149f, 0.530f},   {RockShape::Rock, 0.759f, 0.096f, 0.172f, -0.595f},
    {RockShape::Rock, 0.759f, 0.413f, -0.149f, -1.376f},  {RockShape::Rock, 0.819f, 0.110f, 0.259f, 0.653f},
    {RockShape::Rock, 0.880f, 0.022f, -0.246f, 0.321f},   {RockShape::Rock, 0.939f, 0.026f, -0.259f, 0.244f},
    {RockShape::Rock, 0.939f, -0.003f, -0.096f, -0.253f}, {RockShape::Rock, 0.954f, 0.105f, 0.024f, 0.647f},
    {RockShape::Rock, 0.999f, 0.137f, -0.051f, -0.685f},  {RockShape::Rock, 1.000f, 0.240f, -0.385f, 0.730f},
    {RockShape::Rock, 1.000f, 1.781f, 0.385f, 2.730f},    {RockShape::Rock, 1.024f, 0.120f, 0.005f, 0.653f},
    {RockShape::Rock, 1.060f, -0.030f, 0.164f, -0.148f},  {RockShape::Rock, 1.060f, 0.114f, 0.010f, 0.471f},
    {RockShape::Rock, 1.095f, 0.034f, -0.266f, 0.249f},   {RockShape::Rock, 1.119f, 1.122f, 0.715f, 1.989f},
    {RockShape::Rock, 1.179f, 1.231f, 0.024f, -1.704f},   {RockShape::Rock, 1.239f, 0.156f, -0.156f, 0.539f},
    {RockShape::Rock, 1.239f, 0.166f, 0.090f, 0.560f},    {RockShape::Rock, 1.299f, 0.145f, -0.103f, 0.573f},
    {RockShape::Rock, 1.539f, 0.230f, -0.026f, 0.728f},   {RockShape::Rock, 2.648f, 0.270f, 0.132f, 0.113f},
    {RockShape::Rock, 2.855f, 1.277f, 0.008f, -0.939f},   {RockShape::Rock, 3.939f, 0.672f, 0.234f, -0.476f},
}};
constexpr std::array<RockPose, 64> k_BoulderPoses {{
    {RockShape::Boulder1, 0.219f, 0.030f, 0.002f, -0.006f},   {RockShape::Boulder3, 0.279f, -0.011f, -0.001f, -0.070f},
    {RockShape::Boulder2, 0.399f, -0.057f, -0.124f, 0.151f},  {RockShape::Boulder1, 0.468f, -0.040f, 0.062f, 0.096f},
    {RockShape::Boulder1, 0.519f, 0.012f, 0.001f, -0.003f},   {RockShape::Boulder1, 0.579f, 0.016f, -0.048f, 0.051f},
    {RockShape::Boulder2, 0.579f, -0.018f, 0.004f, -0.001f},  {RockShape::Boulder2, 0.579f, -0.062f, -0.019f, 0.082f},
    {RockShape::Boulder3, 0.639f, 0.095f, 0.079f, -0.279f},   {RockShape::Boulder3, 0.699f, 0.046f, 0.259f, 0.234f},
    {RockShape::Boulder2, 0.759f, -0.018f, -0.015f, 0.188f},  {RockShape::Boulder3, 0.760f, 0.025f, 0.493f, -0.535f},
    {RockShape::Boulder3, 0.939f, 0.999f, -1.039f, -0.014f},  {RockShape::Boulder3, 0.947f, 0.183f, 0.018f, 0.081f},
    {RockShape::Boulder1, 0.986f, 0.045f, 0.121f, -0.215f},   {RockShape::Boulder3, 0.987f, 0.000f, 0.000f, 0.000f},
    {RockShape::Boulder1, 0.992f, 0.052f, -0.085f, -0.046f},  {RockShape::Boulder3, 1.000f, 0.277f, 0.760f, 0.266f},
    {RockShape::Boulder2, 1.000f, 0.009f, -0.857f, -0.223f},  {RockShape::Boulder2, 1.000f, 0.070f, 0.588f, 0.679f},
    {RockShape::Boulder2, 1.000f, -0.014f, -0.171f, 0.353f},  {RockShape::Boulder1, 1.000f, 0.065f, -0.046f, -0.040f},
    {RockShape::Boulder2, 1.000f, -0.046f, -0.065f, 0.198f},  {RockShape::Boulder3, 1.000f, 0.049f, 0.071f, -0.000f},
    {RockShape::Boulder3, 1.000f, 0.649f, -0.178f, 0.279f},   {RockShape::Boulder1, 1.008f, 0.053f, 0.000f, -0.001f},
    {RockShape::Boulder2, 1.027f, 0.065f, 0.072f, 0.110f},    {RockShape::Boulder3, 1.034f, 0.059f, 0.001f, 0.001f},
    {RockShape::Boulder1, 1.036f, 0.059f, -0.255f, -0.119f},  {RockShape::Boulder3, 1.059f, 0.000f, -0.067f, -0.167f},
    {RockShape::Boulder1, 1.119f, 0.063f, -0.044f, 0.376f},   {RockShape::Boulder1, 1.202f, 0.097f, 0.209f, 0.057f},
    {RockShape::Boulder2, 1.299f, -0.081f, 0.028f, 0.172f},   {RockShape::Boulder3, 1.302f, 0.072f, -0.204f, 0.066f},
    {RockShape::Boulder3, 1.302f, 0.065f, -0.186f, -0.353f},  {RockShape::Boulder2, 1.359f, 0.000f, -0.004f, -0.157f},
    {RockShape::Boulder1, 1.370f, 0.073f, 0.102f, 0.188f},    {RockShape::Boulder2, 1.419f, -0.038f, 0.139f, 0.339f},
    {RockShape::Boulder1, 1.440f, 0.086f, -0.310f, -0.216f},  {RockShape::Boulder3, 1.599f, 0.108f, 0.001f, -0.000f},
    {RockShape::Boulder3, 1.599f, 0.113f, 0.151f, 0.114f},    {RockShape::Boulder3, 1.599f, 0.161f, 0.046f, -0.001f},
    {RockShape::Boulder1, 1.659f, 0.126f, 0.043f, -0.275f},   {RockShape::Boulder1, 1.834f, 0.342f, 0.418f, -0.129f},
    {RockShape::Boulder1, 1.839f, 0.267f, -0.344f, 0.661f},   {RockShape::Boulder3, 1.839f, 0.479f, 0.081f, 0.018f},
    {RockShape::Boulder3, 1.839f, 0.437f, 0.045f, 0.059f},    {RockShape::Boulder1, 1.839f, 0.462f, -0.022f, -0.302f},
    {RockShape::Boulder1, 2.019f, 0.182f, 0.243f, -0.584f},   {RockShape::Boulder2, 2.139f, -0.058f, 0.002f, 0.001f},
    {RockShape::Boulder1, 2.162f, 0.191f, -0.030f, 0.243f},   {RockShape::Boulder3, 2.199f, 0.272f, 0.037f, -0.091f},
    {RockShape::Boulder2, 2.199f, 0.814f, 0.315f, -0.385f},   {RockShape::Boulder3, 2.259f, 0.158f, -0.042f, 0.049f},
    {RockShape::Boulder1, 2.379f, 0.148f, 0.120f, 0.012f},    {RockShape::Boulder2, 2.439f, 0.000f, -0.124f, -0.214f},
    {RockShape::Boulder1, 2.739f, 0.185f, -0.068f, -0.003f},  {RockShape::Boulder2, 2.752f, 0.162f, -0.367f, 0.120f},
    {RockShape::Boulder2, 2.799f, -0.110f, -0.243f, -0.101f}, {RockShape::Boulder2, 2.799f, -0.032f, 0.615f, 0.844f},
    {RockShape::Boulder1, 2.919f, 0.000f, -0.279f, 0.070f},   {RockShape::Boulder1, 3.159f, 0.635f, -0.064f, 0.133f},
    {RockShape::Boulder1, 3.819f, 0.310f, -0.188f, -0.068f},  {RockShape::Boulder2, 3.939f, 1.238f, -0.061f, 0.222f},
}};
constexpr std::array<RockPose, 64> k_SlabPoses {{
    {RockShape::Long, 0.370f, 0.018f, 0.224f, 0.138f},     {RockShape::Long, 0.370f, -0.055f, 0.000f, 0.000f},
    {RockShape::Long, 0.370f, 0.071f, -0.024f, 0.512f},    {RockShape::Long, 0.378f, 0.744f, 0.124f, -2.944f},
    {RockShape::Square, 0.577f, -0.033f, 0.001f, -0.011f}, {RockShape::Flat, 0.579f, -0.027f, 0.000f, 0.000f},
    {RockShape::Flat, 0.580f, -0.026f, 0.213f, 0.016f},    {RockShape::Long, 0.681f, 0.171f, 0.070f, 0.618f},
    {RockShape::Square, 0.699f, 0.080f, -0.000f, 0.338f},  {RockShape::Sharp, 0.699f, -0.180f, 0.126f, -0.105f},
    {RockShape::Sharp, 0.699f, 0.030f, -0.329f, -0.466f},  {RockShape::Square, 0.759f, 0.097f, -0.227f, 0.313f},
    {RockShape::Square, 0.784f, -0.027f, 0.001f, -0.011f}, {RockShape::Long, 0.793f, 0.000f, -0.564f, 1.155f},
    {RockShape::Long, 0.858f, 0.000f, 0.000f, 0.000f},     {RockShape::Sharp, 0.939f, -0.093f, -0.011f, -0.021f},
    {RockShape::Long, 0.939f, 0.055f, 0.008f, -0.179f},    {RockShape::Sharp, 0.939f, 0.000f, 0.057f, -0.060f},
    {RockShape::Sharp, 0.954f, 0.587f, 0.482f, -0.835f},   {RockShape::Long, 1.000f, -0.066f, 0.106f, 0.031f},
    {RockShape::Square, 1.000f, 0.205f, -0.289f, -0.210f}, {RockShape::Long, 1.000f, 0.079f, 0.181f, 0.537f},
    {RockShape::Square, 1.000f, 0.082f, 0.568f, -0.119f},  {RockShape::Flat, 1.000f, -0.022f, 0.000f, 0.000f},
    {RockShape::Flat, 1.000f, 1.125f, -0.030f, -2.747f},   {RockShape::Sharp, 1.000f, 1.722f, 0.119f, 0.757f},
    {RockShape::Flat, 1.000f, 0.416f, 0.585f, -0.643f},    {RockShape::Long, 1.000f, -0.025f, 0.835f, -0.257f},
    {RockShape::Long, 1.000f, -0.004f, -0.164f, -0.376f},  {RockShape::Long, 1.000f, 2.954f, 0.689f, 2.978f},
    {RockShape::Square, 1.000f, 0.000f, -0.011f, -0.290f}, {RockShape::Sharp, 1.001f, -0.125f, -0.221f, -0.183f},
    {RockShape::Flat, 1.004f, -0.107f, 0.080f, -0.080f},   {RockShape::Sharp, 1.015f, -0.085f, -0.131f, 0.124f},
    {RockShape::Sharp, 1.026f, 0.123f, -0.064f, -0.354f},  {RockShape::Flat, 1.033f, -0.076f, 0.055f, 0.013f},
    {RockShape::Long, 1.059f, 0.053f, -0.124f, 0.293f},    {RockShape::Long, 1.059f, 0.000f, -0.007f, 0.107f},
    {RockShape::Long, 1.239f, -0.056f, 0.205f, -0.012f},   {RockShape::Flat, 1.266f, 0.076f, -0.098f, -0.152f},
    {RockShape::Square, 1.359f, 0.064f, 0.303f, 0.129f},   {RockShape::Sharp, 1.406f, 1.416f, 0.535f, -0.745f},
    {RockShape::Square, 1.419f, 0.464f, 0.459f, -0.228f},  {RockShape::Long, 1.539f, 0.192f, 0.005f, 0.289f},
    {RockShape::Long, 1.539f, -0.052f, -0.087f, -0.055f},  {RockShape::Sharp, 1.659f, -0.478f, 0.105f, 0.079f},
    {RockShape::Sharp, 1.659f, -0.121f, -0.336f, -0.167f}, {RockShape::Sharp, 1.719f, 0.455f, -0.077f, -0.088f},
    {RockShape::Flat, 1.719f, -0.017f, 0.116f, -0.047f},   {RockShape::Long, 1.839f, 0.038f, 0.519f, -0.118f},
    {RockShape::Long, 1.839f, 0.504f, -0.503f, 0.512f},    {RockShape::Flat, 1.899f, 2.816f, 0.305f, -0.945f},
    {RockShape::Sharp, 1.959f, -0.442f, 0.005f, -0.150f},  {RockShape::Flat, 2.139f, 0.356f, 0.160f, -0.333f},
    {RockShape::Long, 2.259f, -0.019f, 0.722f, -0.032f},   {RockShape::Long, 2.319f, 1.478f, -0.029f, 0.072f},
    {RockShape::Sharp, 2.379f, -0.086f, 0.050f, -0.071f},  {RockShape::Sharp, 2.739f, 0.351f, -0.208f, -0.415f},
    {RockShape::Long, 3.219f, 0.028f, -0.182f, -0.077f},   {RockShape::Sharp, 3.269f, -0.154f, -0.468f, 0.137f},
    {RockShape::Sharp, 3.339f, 0.097f, 0.328f, 0.067f},    {RockShape::Sharp, 3.399f, -1.380f, 0.326f, -0.496f},
    {RockShape::Sharp, 3.994f, -0.419f, 0.293f, -0.098f},  {RockShape::Long, 3.999f, 2.731f, 0.036f, -0.020f},
}};

/// The classes of rock in the order of their weights
constexpr std::array<RockClass, 3> k_RockClasses {RockClass::Small, RockClass::Boulder, RockClass::Slab};

/// The steps of the count of the spreads of the clumps of rocks: the 1st, 5th, 10th, 25th, 50th, 75th, 90th, 95th and
/// 99th percentiles
constexpr std::array<float, 9> k_PoseSteps {0.0f, 0.05f, 0.1f, 0.25f, 0.5f, 0.75f, 0.9f, 0.95f, 1.0f};
/// The mean distance in metres from the middle of a clump of rocks of the story to its rocks, divided by the square
/// root of the number of its rocks. A clump links the rocks that are 20 m or less apart.
constexpr std::array<float, 9> k_ClumpSpreads {0.611f, 0.820f, 0.948f, 1.341f, 2.199f, 3.562f, 4.718f, 5.317f, 6.243f};
/// The sizes of the clumps of each main class on the lands of the story, at the steps of the groups of trees
constexpr std::array<float, 13> k_SmallClumpSizes {1.0f, 1.0f, 1.0f, 2.0f, 2.0f,  3.0f, 3.0f,
                                                   4.0f, 5.0f, 7.0f, 8.0f, 10.1f, 15.0f};
constexpr std::array<float, 13> k_BoulderClumpSizes {1.0f, 1.0f, 1.0f, 1.0f, 1.0f,  2.0f, 2.0f,
                                                     3.0f, 3.0f, 4.0f, 5.0f, 11.9f, 20.0f};
constexpr std::array<float, 13> k_SlabClumpSizes {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 2.0f, 2.0f, 2.0f, 3.0f, 4.0f, 4.0f, 4.0f, 5.0f};
/// The share of the rocks of a clump of the story that are of the main class of the clump
constexpr float k_ClumpMainShare = 0.8f;
/// The share of the small rocks of a clump that have almost the same scale as the first small rock of the clump
constexpr float k_ClumpSameScaleShare = 0.5f;
/// A clump tries this many points for each of its rocks, so that it keeps its size on rough ground
constexpr uint32_t k_ClumpTries = 3;
/// A small rock is a plain rock on three of the four lands of the story with small rocks. On the other land, the small
/// rocks are of the stone of the ground.
constexpr float k_PlainRockShare = 0.75f;
/// No rock of the story lies nearer to the sea than this, in metres
constexpr float k_RockNearestSea = 30.0f;
/// A clump of rocks that has a tree within this distance in metres starts with the second value times the chance of a
/// clump in the open
constexpr float k_RockWoodsReach = 25.0f;
constexpr float k_RockWoodsFactor = 15.0f;
/// No two rocks of the story lie nearer than this, in metres, and no rock lies nearer to a tree than the second value
constexpr float k_RockGap = 1.2f;
constexpr float k_RockTreeGap = 1.0f;
/// No dead tree lies on a slope steeper than this, in height units of 0.67 m for each corner
constexpr float k_DeadTreeSlope = 16.0f;
/// The rise for each metre at the slopes of 10 and 20 degrees, and the height of the high ground in metres
constexpr float k_TenDegrees = 0.1763f;
constexpr float k_TwentyDegrees = 0.3640f;
constexpr float k_HighGround = 80.0f;

// The ring of rocks of the first land of the story: eight plain rocks at 20 m to 42 m from the middle, on grass at
// 34 m to 39 m, far from the sea. The radius spread of a ring is 0.22 of its mean radius.
constexpr size_t k_RingRocks = 8;
constexpr float k_RingRadius = 30.4f;
constexpr float k_RingRadiusSpread = 0.38f;
/// Each rock of the ring moves along the ring by a random part of the angle between two rocks, up to this part
constexpr float k_RingTurnSpread = 0.3f;
/// The rocks of the ring differ by this many metres in height or less
constexpr float k_RingRise = 6.0f;
constexpr float k_RingLowest = 10.0f;
constexpr float k_RingHighest = 110.0f;
constexpr float k_RingNearestSea = 60.0f;
constexpr float k_RingSmallest = 0.47f;
constexpr float k_RingLargest = 0.79f;
/// The distance between the points where the ring can stand, in corners
constexpr float k_RingSpacing = 8.0f;

// The sea stacks of the story: groups of three to seven spiky pillars of limestone on the beach, 2 m to 11 m above the
// sea, with a mean distance of 7 m to 16 m from the middle of the group to a pillar
constexpr std::array<Weighted<uint32_t>, 3> k_StackGroupSizes {{{3, 2}, {4, 1}, {7, 1}}};
/// The scales of the spiky pillars on the beaches of the story
constexpr std::array<float, 19> k_StackScales {0.458f, 0.458f, 0.458f, 0.578f, 0.578f, 0.662f, 0.671f, 0.694f, 0.759f, 0.860f,
                                               0.884f, 0.912f, 0.912f, 1.054f, 1.116f, 1.118f, 1.121f, 1.353f, 1.396f};
constexpr Stone k_StackStone = Stone::Limestone;
constexpr float k_StackSmallestRadius = 11.0f;
constexpr float k_StackLargestRadius = 23.0f;
constexpr float k_StackHighest = 12.0f;
constexpr float k_StackFarthestSea = 100.0f;
constexpr float k_StackGap = 2.0f;
/// The chance of a group at a point of its grid where the beach suits it, the most groups on a land, and the smallest
/// distance between two groups in metres
constexpr float k_StackChance = 0.25f;
constexpr size_t k_MostStackGroups = 4;
constexpr float k_StackGroupGap = 300.0f;

/// A value from a table of values at steps of a random number from 0 to 1, along the line between two steps
template <size_t N>
float Interpolate(const std::array<float, N>& steps, const std::array<float, N>& values, float unit)
{
	const float u = std::clamp(unit, 0.0f, 1.0f);
	size_t step = 1;
	while (step + 1 < N && u > steps[step])
	{
		++step;
	}
	const float low = steps[step - 1];
	const float high = steps[step];
	const float along = (u - low) / (high - low);
	return values[step - 1] + ((values[step] - values[step - 1]) * along);
}

/// The weights of the main classes of the clumps of the story on ground of a slope and a height, in the order of
/// k_RockClasses
std::array<float, 3> ClassWeightsAt(float slope, float height)
{
	if (height >= k_HighGround)
	{
		return {26.0f, 59.0f, 15.0f};
	}
	if (slope < k_TenDegrees)
	{
		return {50.0f, 37.0f, 13.0f};
	}
	if (slope < k_TwentyDegrees)
	{
		return {44.0f, 35.0f, 21.0f};
	}
	return {37.0f, 36.0f, 27.0f};
}

/// The biomes where the ring of rocks stands: open grass and woods, as on the first land of the story
bool RingStandsIn(Biome biome)
{
	return biome == Biome::Grassland || biome == Biome::TemperateForest;
}

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

	/// A feature can stand at a place in corners at the edge of the sea: on land, off the rivers and out of the clearings
	[[nodiscard]] bool FreeOnShore(float x, float z) const
	{
		const auto last = static_cast<float>(_side - 1);
		if (x < 0.0f || z < 0.0f || x > last || z > last)
		{
			return false;
		}
		const auto [cx, cz] = CornerOf(x, z);
		const auto& rivers = _ground.rivers != nullptr ? *_ground.rivers : _ground.keepClear;
		if (rivers.At(cx, cz) != 0 || _ground.heights.At(cx, cz) <= terrain::k_SeaLevel)
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
		const auto townDistance = [x, z](float townX, float townZ) {
			const float tx = (x - townX) * k_MetresPerCorner;
			const float tz = (z - townZ) * k_MetresPerCorner;
			return std::sqrt((tx * tx) + (tz * tz));
		};
		float town = townDistance(_ground.townX, _ground.townZ);
		for (const auto& other : _ground.towns)
		{
			town = std::min(town, townDistance(other.x, other.z));
		}
		return {.height = _ground.curve.MetresOf(_ground.heights.At(cx, cz)),
		        .slope = GradientAt(_ground.heights, _ground.curve, cx, cz),
		        .sea = _ground.seaDistance.At(cx, cz) * k_MetresPerCorner,
		        .town = town};
	}

	[[nodiscard]] Biome BiomeAt(float x, float z) const
	{
		const auto [cx, cz] = CornerOf(x, z);
		return _ground.biomes.At(cx, cz);
	}

	/// The largest difference of height to a corner next to the place, in height units of 0.67 m
	[[nodiscard]] float SlopeUnitsAt(float x, float z) const
	{
		const auto [cx, cz] = CornerOf(x, z);
		return SlopeAt(_ground.heights, _ground.curve, cx, cz);
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
void PlaceStand(const Placement& placement, float x, float z, Tree main, uint32_t count, Random& random, PointSquares& trees,
                std::vector<Feature>& placed)
{
	const auto& rule = *RuleOf(main);
	const float looser = count >= k_LargeStand ? k_LargeStandSpread : 1.0f;
	const float radius = rule.spread * looser * std::sqrt(static_cast<float>(count)) / k_MetresPerCorner;
	uint32_t grown = 0;
	for (uint32_t attempt = 0; attempt < count * k_StandTries && grown < count; ++attempt)
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
		++grown;
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

void PlaceStands(const Placement& placement, uint64_t seed, const Settings& settings, PointSquares& trees,
                 std::vector<Feature>& placed)
{
	const auto standSeed = StageSeed(seed, 43);
	const float last = static_cast<float>(placement.Side() - 1);
	// A higher density puts the points closer together. Each point keeps the chance of a stand of the usual spacing, so
	// the land has that many times more stands.
	const float spacing = settings.standSpacing / std::sqrt(std::max(settings.treeDensity, 1.0f));
	const int points = static_cast<int>(last / spacing);
	const float squareSide = settings.standSpacing * k_MetresPerCorner;
	const float squareKm = squareSide * squareSide / 1.0e6f;
	for (int i = 0; i < points; ++i)
	{
		for (int j = 0; j < points; ++j)
		{
			auto random = RandomAt(standSeed, i, j);
			const float x = (static_cast<float>(i) + random.NextUnit()) * spacing;
			const float z = (static_cast<float>(j) + random.NextUnit()) * spacing;
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
				PlaceStand(placement, x, z, *main, StandSize(sizeRoll, planting.jungle), random, trees, placed);
			}
		}
	}
}

void PlaceDeadTrees(const Placement& placement, uint64_t seed, const Settings& settings, std::vector<Feature>& placed)
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
			if (!placement.Free(x, z) || placement.SlopeUnitsAt(x, z) > k_DeadTreeSlope)
			{
				continue;
			}
			if (roll < PlantingOf(placement.BiomeAt(x, z)).deadChance)
			{
				placed.push_back({.kind = Kind::Static,
				                  .type = k_DeadTreeStatic,
				                  .x = x * k_MetresPerCorner,
				                  .z = z * k_MetresPerCorner,
				                  .angle = angle,
				                  .size = size,
				                  .maxSize = size});
			}
		}
	}
}

/// The rocks of the land so far, and the trees that they keep away from
struct RockPlaces
{
	const Placement& placement;
	const PointSquares& trees;
	/// The same trees in larger squares, so that a clump finds the woods near it
	const PointSquares& woods;
	PointSquares rocks;
	/// The small rocks of this land are plain rocks, or the small rocks of the stone of the ground
	bool plain;
};

std::span<const RockPose> PosesOf(RockClass rockClass, bool plain)
{
	switch (rockClass)
	{
	case RockClass::Small:
		return plain ? std::span<const RockPose>(k_PlainPoses) : std::span<const RockPose>(k_StonePoses);
	case RockClass::Boulder:
		return k_BoulderPoses;
	case RockClass::Slab:
		return k_SlabPoses;
	}
	return k_BoulderPoses;
}

/// The steepest ground of a class of rock, as the rise for each metre: the 95th percentile of the rocks of the story
float SteepestOf(RockClass rockClass, bool plain)
{
	switch (rockClass)
	{
	case RockClass::Small:
		return plain ? 1.213f : 0.804f;
	case RockClass::Boulder:
		return 1.010f;
	case RockClass::Slab:
		return 1.397f;
	}
	return 1.0f;
}

/// Puts one rock at a place in corners with a pose, if the ground there takes it. Gives true when it puts the rock.
bool PlaceRock(RockPlaces& places, float x, float z, RockClass rockClass, const RockPose& pose, float angle,
               std::vector<Feature>& placed)
{
	const auto& placement = places.placement;
	if (!placement.Free(x, z))
	{
		return false;
	}
	const auto spot = placement.SpotAt(x, z);
	if (spot.sea < k_RockNearestSea || spot.slope > SteepestOf(rockClass, places.plain))
	{
		return false;
	}
	const float metresX = x * k_MetresPerCorner;
	const float metresZ = z * k_MetresPerCorner;
	if (places.rocks.AnyNear(metresX, metresZ, k_RockGap) || places.trees.AnyNear(metresX, metresZ, k_RockTreeGap))
	{
		return false;
	}
	places.rocks.Add(metresX, metresZ);
	const bool plainType = rockClass == RockClass::Small && places.plain;
	const auto stone = PlantingOf(placement.BiomeAt(x, z)).stone;
	placed.push_back({.kind = Kind::Static,
	                  .type = plainType ? k_PlainRockStatic : RockType(pose.shape, stone),
	                  .x = metresX,
	                  .z = metresZ,
	                  .angle = angle,
	                  .size = pose.scale,
	                  .maxSize = pose.scale,
	                  .xAngle = pose.xAngle,
	                  .zAngle = pose.zAngle,
	                  .altitude = pose.altitude});
	return true;
}

/// The class of a rock of a clump: mostly the main class of the clump, else one of the two other classes by the
/// weights of the ground
RockClass MemberClass(RockClass main, float slope, float height, Random& random)
{
	const float mainRoll = random.NextUnit();
	const float otherRoll = random.NextUnit();
	if (mainRoll < k_ClumpMainShare)
	{
		return main;
	}
	const auto weights = ClassWeightsAt(slope, height);
	std::array<RockClass, 2> others {};
	std::array<float, 2> otherWeights {};
	size_t count = 0;
	for (size_t i = 0; i < k_RockClasses.size(); ++i)
	{
		if (k_RockClasses[i] != main)
		{
			others[count] = k_RockClasses[i];
			otherWeights[count] = weights[i];
			++count;
		}
	}
	return otherRoll * (otherWeights[0] + otherWeights[1]) < otherWeights[0] ? others[0] : others[1];
}

void PlaceRockClumps(RockPlaces& places, uint64_t seed, const Settings& settings, std::vector<Feature>& placed)
{
	const auto& placement = places.placement;
	const auto clumpSeed = StageSeed(seed, 45);
	const float last = static_cast<float>(placement.Side() - 1);
	const int points = static_cast<int>(last / settings.rockSpacing);
	const float squareSide = settings.rockSpacing * k_MetresPerCorner;
	const float squareKm = squareSide * squareSide / 1.0e6f;
	const auto smallPoses = PosesOf(RockClass::Small, places.plain);
	for (int i = 0; i < points; ++i)
	{
		for (int j = 0; j < points; ++j)
		{
			auto random = RandomAt(clumpSeed, i, j);
			const float x = (static_cast<float>(i) + random.NextUnit()) * settings.rockSpacing;
			const float z = (static_cast<float>(j) + random.NextUnit()) * settings.rockSpacing;
			const float roll = random.NextUnit();
			const float classRoll = random.NextUnit();
			const float sizeRoll = random.NextUnit();
			const float spreadRoll = random.NextUnit();
			const auto firstSmall = static_cast<int>(random.NextBelow(static_cast<uint32_t>(smallPoses.size())));
			if (!placement.Free(x, z))
			{
				continue;
			}
			// Most clumps of the story lie in or beside a wood; few lie in the open
			const float metresX = x * k_MetresPerCorner;
			const float metresZ = z * k_MetresPerCorner;
			const float woods = places.woods.AnyNear(metresX, metresZ, k_RockWoodsReach) ? k_RockWoodsFactor : 1.0f;
			if (roll >= PlantingOf(placement.BiomeAt(x, z)).rockClumps * squareKm * woods)
			{
				continue;
			}
			const auto centre = placement.SpotAt(x, z);
			const auto main = MainClassAt(centre.slope, centre.height, classRoll);
			const auto count = ClumpSize(main, sizeRoll);
			// The rocks lie evenly in a circle. The mean distance from the middle to a rock is two thirds of the radius.
			const float spread = Interpolate(k_PoseSteps, k_ClumpSpreads, spreadRoll);
			const float radius = 1.5f * spread * std::sqrt(static_cast<float>(count)) / k_MetresPerCorner;
			uint32_t laid = 0;
			for (uint32_t attempt = 0; attempt < count * k_ClumpTries && laid < count; ++attempt)
			{
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
				const auto rockClass = MemberClass(main, centre.slope, centre.height, random);
				const auto poses = PosesOf(rockClass, places.plain);
				const float sameRoll = random.NextUnit();
				const int near = static_cast<int>(random.NextBelow(5)) - 2;
				auto index = static_cast<int>(random.NextBelow(static_cast<uint32_t>(poses.size())));
				// The small rocks of a clump often have almost the same scale. The poses go from the smallest scale to the
				// largest.
				if (rockClass == RockClass::Small && sameRoll < k_ClumpSameScaleShare)
				{
					index = std::clamp(firstSmall + near, 0, static_cast<int>(poses.size()) - 1);
				}
				const float angle = random.NextUnit() * k_FullTurn;
				if (PlaceRock(places, x + (dx * radius), z + (dz * radius), rockClass, poses[static_cast<size_t>(index)], angle,
				              placed))
				{
					++laid;
				}
			}
		}
	}
}

/// The rocks of a ring round a middle in corners
using Ring = std::array<std::pair<float, float>, k_RingRocks>;

Ring RingAt(Random& random, float x, float z)
{
	Ring ring {};
	for (size_t k = 0; k < ring.size(); ++k)
	{
		const float turn = (static_cast<float>(k) + random.NextRange(-k_RingTurnSpread, k_RingTurnSpread)) /
		                   static_cast<float>(ring.size()) * k_FullTurn;
		const float radius =
		    k_RingRadius * (1.0f + random.NextRange(-k_RingRadiusSpread, k_RingRadiusSpread)) / k_MetresPerCorner;
		ring[k] = {x + (std::cos(turn) * radius), z + (std::sin(turn) * radius)};
	}
	return ring;
}

/// The ring of the story stands away from the sea, on ground where its rocks differ by a few metres in height
bool RingSuits(const Placement& placement, float x, float z, const Ring& ring)
{
	if (!placement.Free(x, z) || !RingStandsIn(placement.BiomeAt(x, z)))
	{
		return false;
	}
	const auto centre = placement.SpotAt(x, z);
	if (centre.height < k_RingLowest || centre.height > k_RingHighest || centre.sea < k_RingNearestSea)
	{
		return false;
	}
	float lowest = centre.height;
	float highest = centre.height;
	for (const auto& [rx, rz] : ring)
	{
		if (!placement.Free(rx, rz))
		{
			return false;
		}
		const auto spot = placement.SpotAt(rx, rz);
		lowest = std::min(lowest, spot.height);
		highest = std::max(highest, spot.height);
	}
	return highest - lowest <= k_RingRise;
}

/// One ring of eight plain rocks with a radius of about 30 metres, as on the first land of the story. The ring stands at
/// the point of a grid that has the lowest random key of all the points where the ground suits it.
void PlaceStoneRing(RockPlaces& places, uint64_t seed, std::vector<Feature>& placed)
{
	const auto& placement = places.placement;
	const auto ringSeed = StageSeed(seed, 46);
	const int points = static_cast<int>(static_cast<float>(placement.Side() - 1) / k_RingSpacing);
	std::optional<std::pair<uint32_t, std::pair<int, int>>> best;
	for (int i = 0; i < points; ++i)
	{
		for (int j = 0; j < points; ++j)
		{
			auto random = RandomAt(ringSeed, i, j);
			const auto key = random.Next();
			if (best && key >= best->first)
			{
				continue;
			}
			const float x = (static_cast<float>(i) + random.NextUnit()) * k_RingSpacing;
			const float z = (static_cast<float>(j) + random.NextUnit()) * k_RingSpacing;
			if (RingSuits(placement, x, z, RingAt(random, x, z)))
			{
				best = {key, {i, j}};
			}
		}
	}
	if (!best)
	{
		return;
	}
	// The same random numbers give the same ring again
	const auto [i, j] = best->second;
	auto random = RandomAt(ringSeed, i, j);
	random.Next();
	const float x = (static_cast<float>(i) + random.NextUnit()) * k_RingSpacing;
	const float z = (static_cast<float>(j) + random.NextUnit()) * k_RingSpacing;
	const auto ring = RingAt(random, x, z);
	// The rocks of the ring of the story have scales from 0.47 to 0.79
	const auto lower = std::ranges::lower_bound(k_PlainPoses, k_RingSmallest, {}, &RockPose::scale);
	const auto upper = std::ranges::upper_bound(k_PlainPoses, k_RingLargest, {}, &RockPose::scale);
	const auto choices = static_cast<uint32_t>(std::max<std::ptrdiff_t>(std::distance(lower, upper), 1));
	const bool plain = places.plain;
	places.plain = true;
	for (const auto& [rx, rz] : ring)
	{
		const auto& pose = *(lower + random.NextBelow(choices));
		const float angle = random.NextUnit() * k_FullTurn;
		PlaceRock(places, rx, rz, RockClass::Small, pose, angle, placed);
	}
	places.plain = plain;
}

/// Groups of three to seven spiky pillars on the beaches, as the sea stacks of the story. No two groups stand near.
void PlaceSeaStacks(const Placement& placement, uint64_t seed, const Settings& settings, std::vector<Feature>& placed)
{
	const auto stackSeed = StageSeed(seed, 47);
	const int points = static_cast<int>(static_cast<float>(placement.Side() - 1) / settings.stackSpacing);
	std::vector<std::pair<float, float>> groups;
	PointSquares pillars;
	for (int i = 0; i < points; ++i)
	{
		for (int j = 0; j < points; ++j)
		{
			auto random = RandomAt(stackSeed, i, j);
			const float x = (static_cast<float>(i) + random.NextUnit()) * settings.stackSpacing;
			const float z = (static_cast<float>(j) + random.NextUnit()) * settings.stackSpacing;
			const float roll = random.NextUnit();
			const auto count = PickWeighted(k_StackGroupSizes, random);
			const float radius = random.NextRange(k_StackSmallestRadius, k_StackLargestRadius) / k_MetresPerCorner;
			if (groups.size() >= k_MostStackGroups || roll >= k_StackChance || !placement.FreeOnShore(x, z) ||
			    placement.BiomeAt(x, z) != Biome::Beach)
			{
				continue;
			}
			const auto centre = placement.SpotAt(x, z);
			if (centre.sea > k_StackFarthestSea || centre.height > k_StackHighest)
			{
				continue;
			}
			const float metresX = x * k_MetresPerCorner;
			const float metresZ = z * k_MetresPerCorner;
			const bool crowded = std::ranges::any_of(groups, [metresX, metresZ](const std::pair<float, float>& group) {
				const float dx = group.first - metresX;
				const float dz = group.second - metresZ;
				return (dx * dx) + (dz * dz) < k_StackGroupGap * k_StackGroupGap;
			});
			if (crowded)
			{
				continue;
			}
			groups.emplace_back(metresX, metresZ);
			uint32_t stood = 0;
			for (uint32_t attempt = 0; attempt < count * k_ClumpTries && stood < count; ++attempt)
			{
				float dx = 0.0f;
				float dz = 0.0f;
				do
				{
					dx = random.NextRange(-1.0f, 1.0f);
					dz = random.NextRange(-1.0f, 1.0f);
				} while ((dx * dx) + (dz * dz) > 1.0f);
				const float scale = k_StackScales[random.NextBelow(static_cast<uint32_t>(k_StackScales.size()))];
				const float angle = random.NextUnit() * k_FullTurn;
				const float px = x + (dx * radius);
				const float pz = z + (dz * radius);
				if (!placement.FreeOnShore(px, pz) || placement.SpotAt(px, pz).height > k_StackHighest)
				{
					continue;
				}
				const float pillarX = px * k_MetresPerCorner;
				const float pillarZ = pz * k_MetresPerCorner;
				if (pillars.AnyNear(pillarX, pillarZ, k_StackGap))
				{
					continue;
				}
				pillars.Add(pillarX, pillarZ);
				++stood;
				placed.push_back({.kind = Kind::Feature,
				                  .type = static_cast<uint8_t>(k_SpikyPillarFeature + static_cast<uint8_t>(k_StackStone)),
				                  .x = pillarX,
				                  .z = pillarZ,
				                  .angle = angle,
				                  .size = scale,
				                  .maxSize = scale});
			}
		}
	}
}
} // namespace

bool features::IsFree(const Ground& ground, float x, float z)
{
	return Placement(ground).Free(x, z);
}

Spot features::SpotAt(const Ground& ground, float x, float z)
{
	return Placement(ground).SpotAt(x, z);
}

bool features::Suits(Tree tree, const Spot& spot)
{
	const auto* rule = RuleOf(tree);
	return rule != nullptr && spot.height >= rule->lowest && spot.height <= rule->highest && spot.slope <= rule->steepest &&
	       spot.sea >= rule->nearestSea && spot.town >= rule->nearestTown && spot.town <= rule->farthestTown;
}

uint32_t features::StandSize(float unit, bool jungle)
{
	const float size = Interpolate(k_SizeSteps, jungle ? k_JungleGroupSizes : k_GroupSizes, unit);
	return std::max(static_cast<uint32_t>(std::floor(size + 0.5f)), 1u);
}

uint32_t features::ClumpSize(RockClass rockClass, float unit)
{
	const auto& sizes = rockClass == RockClass::Small     ? k_SmallClumpSizes
	                    : rockClass == RockClass::Boulder ? k_BoulderClumpSizes
	                                                      : k_SlabClumpSizes;
	const float size = Interpolate(k_SizeSteps, sizes, unit);
	return std::max(static_cast<uint32_t>(std::floor(size + 0.5f)), 1u);
}

RockClass features::MainClassAt(float slope, float height, float unit)
{
	const auto weights = ClassWeightsAt(slope, height);
	float pick = std::clamp(unit, 0.0f, 1.0f) * (weights[0] + weights[1] + weights[2]);
	for (size_t i = 0; i < k_RockClasses.size(); ++i)
	{
		if (pick < weights[i])
		{
			return k_RockClasses[i];
		}
		pick -= weights[i];
	}
	return k_RockClasses.back();
}

std::vector<Feature> features::Place(const Ground& ground, uint64_t seed, const Settings& settings)
{
	Placement placement(ground);
	std::vector<Feature> bigForests;
	PlaceBigForests(placement, seed, settings, bigForests);
	std::vector<Feature> placed;
	PointSquares trees;
	PlaceStands(placement, seed, settings, trees, placed);
	// The rocks lie in and beside the woods, so they come after the trees
	PointSquares woods(k_RockWoodsReach);
	for (const auto& feature : placed)
	{
		woods.Add(feature.x, feature.z);
	}
	RockPlaces rocks {.placement = placement,
	                  .trees = trees,
	                  .woods = woods,
	                  .rocks = PointSquares {},
	                  .plain = Random(StageSeed(seed, 44)).NextUnit() < k_PlainRockShare};
	PlaceStoneRing(rocks, seed, placed);
	PlaceRockClumps(rocks, seed, settings, placed);
	PlaceSeaStacks(placement, seed, settings, placed);
	PlaceDeadTrees(placement, seed, settings, placed);

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
		if (feature.kind != Kind::Tree || feature.type == static_cast<uint8_t>(Tree::Burnt))
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
