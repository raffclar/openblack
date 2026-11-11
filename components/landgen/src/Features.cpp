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
#include <utility>

#include "landgen/Noise.h"
#include "landgen/Random.h"
#include "landgen/Terrain.h"

using namespace openblack::landgen;
using namespace openblack::landgen::features;
using biomes::Biome;

namespace
{
constexpr float k_MetresPerCorner = 10.0f;
constexpr float k_FullTurn = 6.28318531f;

struct Choice
{
	Tree tree;
	uint32_t weight;
};

/// What grows and lies in a biome
struct Planting
{
	/// The chance of a tree at a point, which can be more than 1 for more than one tree
	float treeChance;
	/// The trees grow in clumps with clearings between them
	bool clumps;
	std::span<const Choice> trees;
	/// The chance of a dead tree that lies on the ground, and of a rock
	float deadChance;
	float rockChance;
	Stone stone;
	/// No tree grows above this altitude
	float treeLine;
};

constexpr std::array<Choice, 3> k_BeachTrees {{{Tree::Palm, 3}, {Tree::PalmA, 2}, {Tree::PalmC, 1}}};
constexpr std::array<Choice, 5> k_GrasslandTrees {
    {{Tree::Oak, 3}, {Tree::OakA, 2}, {Tree::Beech, 2}, {Tree::Bush, 3}, {Tree::BushA, 2}}};
constexpr std::array<Choice, 6> k_ForestTrees {
    {{Tree::Beech, 3}, {Tree::Oak, 3}, {Tree::OakA, 2}, {Tree::Birch, 2}, {Tree::Conifer, 1}, {Tree::BushB, 1}}};
constexpr std::array<Choice, 7> k_JungleTrees {{{Tree::Palm, 3},
                                                {Tree::PalmA, 3},
                                                {Tree::PalmB, 3},
                                                {Tree::PalmC, 2},
                                                {Tree::Bush, 2},
                                                {Tree::BushA, 2},
                                                {Tree::BushB, 1}}};
constexpr std::array<Choice, 4> k_DesertTrees {{{Tree::Olive, 2}, {Tree::Cypress, 2}, {Tree::CypressA, 1}, {Tree::BushB, 1}}};
constexpr std::array<Choice, 4> k_SwampTrees {{{Tree::Birch, 3}, {Tree::Bush, 3}, {Tree::BushA, 2}, {Tree::Cypress, 1}}};
constexpr std::array<Choice, 1> k_DeadTrees {{{Tree::Burnt, 1}}};
constexpr std::array<Choice, 3> k_NordicTrees {{{Tree::Conifer, 3}, {Tree::ConiferA, 3}, {Tree::Pine, 3}}};
constexpr std::array<Choice, 4> k_AlpineTrees {{{Tree::Conifer, 3}, {Tree::ConiferA, 3}, {Tree::Pine, 3}, {Tree::Cedar, 1}}};

Planting PlantingOf(Biome biome)
{
	constexpr float k_NoLine = 256.0f;
	switch (biome)
	{
	case Biome::Beach:
		return {.treeChance = 0.03f,
		        .clumps = false,
		        .trees = k_BeachTrees,
		        .deadChance = 0.0f,
		        .rockChance = 0.004f,
		        .stone = Stone::Sandstone,
		        .treeLine = k_NoLine};
	case Biome::Grassland:
		return {.treeChance = 0.06f,
		        .clumps = true,
		        .trees = k_GrasslandTrees,
		        .deadChance = 0.0f,
		        .rockChance = 0.01f,
		        .stone = Stone::Limestone,
		        .treeLine = 150.0f};
	case Biome::TemperateForest:
		return {.treeChance = 0.8f,
		        .clumps = true,
		        .trees = k_ForestTrees,
		        .deadChance = 0.0f,
		        .rockChance = 0.01f,
		        .stone = Stone::Limestone,
		        .treeLine = 150.0f};
	case Biome::DenseJungle:
		return {.treeChance = 1.6f,
		        .clumps = false,
		        .trees = k_JungleTrees,
		        .deadChance = 0.0f,
		        .rockChance = 0.005f,
		        .stone = Stone::Limestone,
		        .treeLine = 150.0f};
	case Biome::Desert:
		return {.treeChance = 0.012f,
		        .clumps = false,
		        .trees = k_DesertTrees,
		        .deadChance = 0.0f,
		        .rockChance = 0.03f,
		        .stone = Stone::Sandstone,
		        .treeLine = k_NoLine};
	case Biome::Swamp:
		return {.treeChance = 0.12f,
		        .clumps = true,
		        .trees = k_SwampTrees,
		        .deadChance = 0.01f,
		        .rockChance = 0.0f,
		        .stone = Stone::Limestone,
		        .treeLine = k_NoLine};
	case Biome::DeadWoods:
		return {.treeChance = 0.3f,
		        .clumps = true,
		        .trees = k_DeadTrees,
		        .deadChance = 0.08f,
		        .rockChance = 0.015f,
		        .stone = Stone::Volcanic,
		        .treeLine = 150.0f};
	case Biome::NordicWastes:
		return {.treeChance = 0.04f,
		        .clumps = true,
		        .trees = k_NordicTrees,
		        .deadChance = 0.0f,
		        .rockChance = 0.03f,
		        .stone = Stone::Chalk,
		        .treeLine = 150.0f};
	case Biome::Alpine:
		return {.treeChance = 0.03f,
		        .clumps = true,
		        .trees = k_AlpineTrees,
		        .deadChance = 0.0f,
		        .rockChance = 0.05f,
		        .stone = Stone::Limestone,
		        .treeLine = 165.0f};
	case Biome::Sea:
	case Biome::_Count:
		break;
	}
	return {.treeChance = 0.0f,
	        .clumps = false,
	        .trees = {},
	        .deadChance = 0.0f,
	        .rockChance = 0.0f,
	        .stone = Stone::Limestone,
	        .treeLine = 0.0f};
}

/// The shapes of rock, the round ones more often
constexpr std::array<RockShape, 12> k_RockShapes {
    RockShape::Rock,     RockShape::Rock, RockShape::Rock, RockShape::Boulder1, RockShape::Boulder2, RockShape::Boulder3,
    RockShape::Boulder1, RockShape::Flat, RockShape::Long, RockShape::Sharp,    RockShape::Square,   RockShape::Rock};

Tree PickTree(std::span<const Choice> trees, Random& random)
{
	uint32_t total = 0;
	for (const auto& choice : trees)
	{
		total += choice.weight;
	}
	auto pick = random.NextBelow(total);
	for (const auto& choice : trees)
	{
		if (pick < choice.weight)
		{
			return choice.tree;
		}
		pick -= choice.weight;
	}
	return trees.back().tree;
}

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
} // namespace

std::vector<Feature> features::Place(const Grid<Biome>& biomes, const Grid<float>& heights, const Grid<uint8_t>& keepClear,
                                     std::span<const Clearing> clearings, uint64_t seed, const Settings& settings)
{
	std::vector<Feature> placed;
	const int side = biomes.side;
	const int points = static_cast<int>(static_cast<float>(side - 1) / settings.spacing);
	const auto pointSeed = StageSeed(seed, 40);
	const auto clumpSeed = StageSeed(seed, 41);
	const float last = static_cast<float>(side - 1);
	// A feature can stand at a place out of the water, off the rivers and out of the clearings
	const auto free = [&](float x, float z) {
		const auto cx = std::clamp(static_cast<int>(x + 0.5f), 0, side - 1);
		const auto cz = std::clamp(static_cast<int>(z + 0.5f), 0, side - 1);
		if (keepClear.At(cx, cz) != 0 || heights.At(cx, cz) <= terrain::k_SeaLevel + 1.0f)
		{
			return false;
		}
		return std::ranges::none_of(clearings, [x, z](const Clearing& clearing) {
			const float dx = x - clearing.x;
			const float dz = z - clearing.z;
			return (dx * dx) + (dz * dz) < clearing.radius * clearing.radius;
		});
	};

	for (int i = 0; i < points; ++i)
	{
		for (int j = 0; j < points; ++j)
		{
			Random random(static_cast<uint64_t>(Hash(pointSeed, i, j)) | (static_cast<uint64_t>(Hash(pointSeed, j, i)) << 32));
			const float x = (static_cast<float>(i) + random.NextUnit()) * settings.spacing;
			const float z = (static_cast<float>(j) + random.NextUnit()) * settings.spacing;
			if (!free(x, z))
			{
				continue;
			}
			const auto cx = std::clamp(static_cast<int>(x + 0.5f), 0, side - 1);
			const auto cz = std::clamp(static_cast<int>(z + 0.5f), 0, side - 1);
			const auto planting = PlantingOf(biomes.At(cx, cz));
			const float height = heights.At(cx, cz);
			const float slope = SlopeAt(heights, cx, cz);

			float treeChance =
			    planting.trees.empty() || slope > settings.treeSlope || height > planting.treeLine ? 0.0f : planting.treeChance;
			if (planting.clumps)
			{
				const float clump = noise::Fractal(clumpSeed, x / last * 14.0f, z / last * 14.0f, 3);
				treeChance *= std::max(noise::SmoothStep(-0.15f, 0.35f, clump), 0.15f);
			}
			const float roll = random.NextUnit();
			const float angle = random.NextUnit() * k_FullTurn;
			const float size = random.NextRange(0.8f, 1.05f);
			const float growth = random.NextRange(0.0f, 0.25f);
			const auto metresX = x * k_MetresPerCorner;
			const auto metresZ = z * k_MetresPerCorner;
			if (roll < treeChance)
			{
				placed.push_back({.kind = Kind::Tree,
				                  .type = static_cast<uint8_t>(PickTree(planting.trees, random)),
				                  .x = metresX,
				                  .z = metresZ,
				                  .angle = angle,
				                  .size = size,
				                  .maxSize = size + growth});
				// A chance above 1 gives a second tree at another place in the square
				if (treeChance > 1.0f && random.NextUnit() < treeChance - 1.0f)
				{
					const auto type = static_cast<uint8_t>(PickTree(planting.trees, random));
					const float secondX = (static_cast<float>(i) + random.NextUnit()) * settings.spacing;
					const float secondZ = (static_cast<float>(j) + random.NextUnit()) * settings.spacing;
					const float secondAngle = random.NextUnit() * k_FullTurn;
					if (free(secondX, secondZ))
					{
						placed.push_back({.kind = Kind::Tree,
						                  .type = type,
						                  .x = secondX * k_MetresPerCorner,
						                  .z = secondZ * k_MetresPerCorner,
						                  .angle = secondAngle,
						                  .size = size,
						                  .maxSize = size + growth});
					}
				}
				continue;
			}
			if (slope > settings.rockSlope)
			{
				continue;
			}
			const float other = roll - treeChance;
			if (other < planting.deadChance)
			{
				placed.push_back({.kind = Kind::Static,
				                  .type = k_DeadTreeStatic,
				                  .x = metresX,
				                  .z = metresZ,
				                  .angle = angle,
				                  .size = size,
				                  .maxSize = size});
			}
			else if (other < planting.deadChance + planting.rockChance)
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
	return placed;
}
