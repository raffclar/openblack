/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <algorithm>
#include <array>
#include <iterator>
#include <vector>

#include <gtest/gtest.h>
#include <landgen/Features.h>
#include <landgen/Generator.h>

using namespace openblack::landgen;
using biomes::Biome;

namespace
{
/// The hash of the cells of the small land of seed 77
constexpr uint64_t k_SmallLandHash = 0x3BE2FF15B006F707ull;

/// A small land that generates quickly
Settings SmallSettings(uint64_t seed)
{
	Settings settings;
	settings.seed = seed;
	settings.side = 129;
	settings.base.seaMargin = 12;
	settings.base.hollows = 2;
	settings.base.hollowRadius = 6.0f;
	settings.hydraulic.drops = 8000;
	settings.thermal.passes = 10;
	settings.rivers.sourceFlow = 150.0f;
	settings.features.most = 100000;
	settings.wind = climate::Wind::West;
	return settings;
}

/// The bytes of the cells, in a 64 bit FNV-1a hash
uint64_t HashOf(const Grid<openblack::lnd::LNDCell>& cells)
{
	uint64_t hash = 0xCBF29CE484222325ull;
	const auto add = [&hash](uint8_t byte) {
		hash ^= byte;
		hash *= 0x100000001B3ull;
	};
	for (const auto& cell : cells.values)
	{
		add(cell.altitude);
		add(cell.luminosity);
		add(cell.flags);
		add(static_cast<uint8_t>(cell.properties.country | (cell.properties.hasWater << 4u) |
		                         (cell.properties.coastLine << 5u) | (cell.properties.fullWater << 6u) |
		                         (cell.properties.split << 7u)));
	}
	return hash;
}

Grid<uint8_t> Clear(int side)
{
	return {side, 0};
}

/// The town is far from a small test land
constexpr float k_FarTown = -1000.0f;

/// The features of a land of one biome at one altitude, far from the sea and the town
std::vector<features::Feature> PlaceOn(Biome biome, float altitude, uint64_t seed, const features::Settings& settings = {},
                                       int side = 64)
{
	const Grid<Biome> biomes(side, biome);
	const Grid<float> heights(side, altitude);
	const Grid<float> sea(side, 40.0f);
	const auto keepClear = Clear(side);
	return features::Place({.biomes = biomes,
	                        .heights = heights,
	                        .seaDistance = sea,
	                        .keepClear = keepClear,
	                        .clearings = {},
	                        .townX = k_FarTown,
	                        .townZ = k_FarTown},
	                       seed, settings);
}

bool IsTree(const features::Feature& feature)
{
	return feature.kind == features::Kind::Tree;
}

/// Ground of grassland at 20 m, 400 m from the sea and 10 km from the town
constexpr features::Spot k_Lowland {.height = 20.0f, .slope = 0.1f, .sea = 400.0f, .town = 10000.0f};
} // namespace

TEST(LandgenFeatures, NothingStandsInTheWaterOrInAClearing)
{
	Grid<Biome> biomes(64, Biome::DenseJungle);
	Grid<float> heights(64, 30.0f);
	const Grid<float> sea(64, 40.0f);
	auto keepClear = Clear(64);
	for (int z = 0; z < 64; ++z)
	{
		for (int x = 0; x < 16; ++x)
		{
			heights.At(x, z) = 0.0f;
			biomes.At(x, z) = Biome::Sea;
		}
		keepClear.At(40, z) = 1;
	}
	const std::array clearings {features::Clearing {.x = 50.0f, .z = 50.0f, .radius = 6.0f}};
	const features::Ground ground {.biomes = biomes,
	                               .heights = heights,
	                               .seaDistance = sea,
	                               .keepClear = keepClear,
	                               .clearings = clearings,
	                               .townX = k_FarTown,
	                               .townZ = k_FarTown};
	const auto placed = features::Place(ground, 5, {});
	ASSERT_FALSE(placed.empty());
	for (const auto& feature : placed)
	{
		const float x = feature.x / 10.0f;
		const float z = feature.z / 10.0f;
		EXPECT_GE(x, 15.5f);
		EXPECT_GE(std::hypot(x - 50.0f, z - 50.0f), 6.0f);
		EXPECT_NE(static_cast<int>(x + 0.5f), 40);
	}
	EXPECT_EQ(placed.size(), features::Place(ground, 5, {}).size());
}

TEST(LandgenFeatures, TheSameSeedPlacesTheSameFeatures)
{
	const auto first = PlaceOn(Biome::TemperateForest, 60.0f, 11, {}, 128);
	const auto second = PlaceOn(Biome::TemperateForest, 60.0f, 11, {}, 128);
	ASSERT_EQ(first.size(), second.size());
	for (size_t i = 0; i < first.size(); ++i)
	{
		EXPECT_EQ(first[i].type, second[i].type);
		EXPECT_EQ(first[i].x, second[i].x);
		EXPECT_EQ(first[i].z, second[i].z);
		EXPECT_EQ(first[i].maxSize, second[i].maxSize);
	}
}

TEST(LandgenFeatures, TheJungleIsDenserThanTheGrassland)
{
	const auto jungle = PlaceOn(Biome::DenseJungle, 30.0f, 3, {}, 128);
	const auto grassland = PlaceOn(Biome::Grassland, 30.0f, 3, {}, 128);
	EXPECT_GT(std::ranges::count_if(jungle, IsTree), std::ranges::count_if(grassland, IsTree) * 3);
	// Most trees of the jungle are palms
	const auto palms = std::ranges::count_if(jungle, [](const auto& feature) {
		const auto tree = static_cast<features::Tree>(feature.type);
		return IsTree(feature) && tree >= features::Tree::Palm && tree <= features::Tree::PalmC;
	});
	EXPECT_GT(palms * 10, std::ranges::count_if(jungle, IsTree) * 7);
}

TEST(LandgenFeatures, BurntTreesStandOnlyOnHighGroundFarFromTheTown)
{
	// 60 m is in the band of the burnt trees of the story, 20 m is under it
	const auto high = PlaceOn(Biome::DeadWoods, 90.0f, 3, {}, 128);
	EXPECT_TRUE(std::ranges::any_of(high, [](const auto& feature) {
		return IsTree(feature) && feature.type == static_cast<uint8_t>(features::Tree::Burnt);
	}));
	const auto low = PlaceOn(Biome::DeadWoods, 30.0f, 3, {}, 128);
	EXPECT_EQ(std::ranges::count_if(low, IsTree), 0);
	EXPECT_FALSE(features::Suits(features::Tree::Burnt, {.height = 60.0f, .slope = 0.5f, .sea = 400.0f, .town = 300.0f}));
}

TEST(LandgenFeatures, TheMostKeepsAnEvenShare)
{
	features::Settings settings;
	settings.most = 50;
	const auto placed = PlaceOn(Biome::DenseJungle, 30.0f, 3, settings);
	EXPECT_EQ(std::ranges::count_if(placed, [](const auto& feature) { return feature.kind != features::Kind::BigForest; }), 50);
}

TEST(LandgenFeatures, EachKindGrowsWhereItGrowsOnTheLandsOfTheStory)
{
	using features::Suits;
	using features::Tree;
	// Conifers never grow under 10 m
	EXPECT_FALSE(Suits(Tree::Conifer, {.height = 9.0f, .slope = 0.1f, .sea = 400.0f, .town = 10000.0f}));
	EXPECT_TRUE(Suits(Tree::Conifer, k_Lowland));
	// Palms grow on the coast and on plateaus inland up to about 66 m
	EXPECT_TRUE(Suits(Tree::Palm, {.height = 5.0f, .slope = 0.1f, .sea = 40.0f, .town = 10000.0f}));
	EXPECT_TRUE(Suits(Tree::PalmB, {.height = 50.0f, .slope = 0.1f, .sea = 400.0f, .town = 10000.0f}));
	EXPECT_FALSE(Suits(Tree::PalmB, {.height = 80.0f, .slope = 0.1f, .sea = 400.0f, .town = 10000.0f}));
	// Trees grow on slopes of 40 degrees, and bushes on slopes of 45 degrees
	EXPECT_TRUE(Suits(Tree::Pine, {.height = 40.0f, .slope = 0.84f, .sea = 400.0f, .town = 10000.0f}));
	EXPECT_TRUE(Suits(Tree::Bush, {.height = 40.0f, .slope = 1.0f, .sea = 400.0f, .town = 10000.0f}));
	EXPECT_FALSE(Suits(Tree::ConiferA, {.height = 40.0f, .slope = 1.0f, .sea = 400.0f, .town = 10000.0f}));
	// Cypresses grow only near a town
	EXPECT_FALSE(Suits(Tree::Cypress, {.height = 50.0f, .slope = 0.1f, .sea = 400.0f, .town = 1000.0f}));
	EXPECT_TRUE(Suits(Tree::Cypress, {.height = 50.0f, .slope = 0.1f, .sea = 400.0f, .town = 100.0f}));
}

TEST(LandgenFeatures, StandsHaveTheSizesOfTheGroupsOfTheStory)
{
	EXPECT_EQ(features::StandSize(0.0f, false), 1u);
	EXPECT_EQ(features::StandSize(0.5f, false), 3u);
	EXPECT_EQ(features::StandSize(0.9f, false), 15u);
	EXPECT_EQ(features::StandSize(0.9f, true), 30u);
	EXPECT_GE(features::StandSize(0.999f, false), 150u);
	for (float u = 0.0f; u < 0.99f; u += 0.01f)
	{
		EXPECT_LE(features::StandSize(u, false), features::StandSize(u + 0.01f, false));
	}
}

TEST(LandgenFeatures, TheTreesOfAForestStandInGroupsOfMixedKinds)
{
	const auto placed = PlaceOn(Biome::TemperateForest, 60.0f, 7, {}, 128);
	std::vector<features::Feature> trees;
	std::ranges::copy_if(placed, std::back_inserter(trees), IsTree);
	ASSERT_GT(trees.size(), 200u);
	size_t lone = 0;
	size_t pairs = 0;
	size_t mixed = 0;
	size_t tooNear = 0;
	for (size_t i = 0; i < trees.size(); ++i)
	{
		bool near = false;
		for (size_t j = 0; j < trees.size(); ++j)
		{
			const float distance = std::hypot(trees[i].x - trees[j].x, trees[i].z - trees[j].z);
			if (i != j && distance <= 15.0f)
			{
				near = true;
				if (j > i)
				{
					++pairs;
					mixed += trees[i].type != trees[j].type ? 1 : 0;
				}
			}
			tooNear += i != j && distance < 1.0f ? 1 : 0;
		}
		lone += near ? 0 : 1;
	}
	// No two trees stand nearer than 1 m
	EXPECT_EQ(tooNear, 0u);
	// The lands of the story have 2% to 7% lone trees, and 40% to 64% of the near pairs are of two kinds
	EXPECT_LT(static_cast<float>(lone), 0.1f * static_cast<float>(trees.size()));
	EXPECT_GT(static_cast<float>(mixed), 0.25f * static_cast<float>(pairs));
	EXPECT_LT(static_cast<float>(mixed), 0.75f * static_cast<float>(pairs));
}

TEST(LandgenFeatures, BigForestsStandApartWithNoTreesRoundThem)
{
	const auto placed = PlaceOn(Biome::Grassland, 60.0f, 9, {}, 400);
	ASSERT_TRUE(std::ranges::any_of(placed, [](const auto& feature) { return feature.kind == features::Kind::BigForest; }));
	for (const auto& forest : placed)
	{
		if (forest.kind != features::Kind::BigForest)
		{
			continue;
		}
		EXPECT_TRUE(forest.type == 0 || forest.type == 1 || forest.type == 3);
		EXPECT_GE(forest.size, 0.58f);
		EXPECT_LE(forest.size, 1.07f);
		for (const auto& other : placed)
		{
			const float distance = std::hypot(forest.x - other.x, forest.z - other.z);
			if (&other == &forest)
			{
				continue;
			}
			if (other.kind == features::Kind::BigForest)
			{
				EXPECT_GE(distance, 55.0f);
			}
			else
			{
				EXPECT_GE(distance, 33.0f * forest.size - 0.01f);
			}
		}
	}
	// No big forest stands near the town
	const Grid<Biome> biomes(400, Biome::Grassland);
	const Grid<float> heights(400, 60.0f);
	const Grid<float> sea(400, 40.0f);
	const auto keepClear = Clear(400);
	const auto nearTown = features::Place({.biomes = biomes,
	                                       .heights = heights,
	                                       .seaDistance = sea,
	                                       .keepClear = keepClear,
	                                       .clearings = {},
	                                       .townX = 200.0f,
	                                       .townZ = 200.0f},
	                                      9, {});
	for (const auto& feature : nearTown)
	{
		if (feature.kind == features::Kind::BigForest)
		{
			EXPECT_GE(std::hypot(feature.x - 2000.0f, feature.z - 2000.0f), 84.0f);
		}
	}
}

TEST(LandgenGenerator, CellsTakeTheirWaterAndSoundFromTheirCorners)
{
	// Open sea at x 0 and 1, the shallows' edge at x 2, then land
	Grid<uint8_t> altitudes(6, 30);
	for (int z = 0; z < 6; ++z)
	{
		altitudes.At(0, z) = 0;
		altitudes.At(1, z) = 0;
		altitudes.At(2, z) = 2;
	}
	const Grid<uint8_t> light(6, 200);
	const Grid<Biome> biomes(6, Biome::Grassland);
	const auto cells = MakeCells(altitudes, light, biomes, Clear(6), Clear(6));
	EXPECT_EQ(cells.At(0, 2).flags, static_cast<uint8_t>(k_OpenWaterFlag | (static_cast<uint8_t>(Sound::Sea) << 2u)));
	EXPECT_EQ(cells.At(0, 2).properties.hasWater, 1);
	EXPECT_EQ(cells.At(1, 2).properties.hasWater, 1);
	EXPECT_EQ(cells.At(2, 2).properties.coastLine, 1);
	EXPECT_EQ(cells.At(4, 2).flags, static_cast<uint8_t>(static_cast<uint8_t>(Sound::Countryside) << 2u));
	EXPECT_EQ(cells.At(4, 2).properties.country, static_cast<uint8_t>(Biome::Grassland));
	EXPECT_EQ(cells.At(4, 2).luminosity, 200);
}

TEST(LandgenGenerator, TheStartAreaIsFlat)
{
	Grid<float> heights(64, 0.0f);
	for (int x = 0; x < 64; ++x)
	{
		for (int z = 0; z < 64; ++z)
		{
			heights.At(x, z) = 20.0f + static_cast<float>((x * 7 + z * 3) % 5);
		}
	}
	const Grid<float> far(64, 40.0f);
	const StartSettings settings {
	    .flatRadius = 6.0f, .blendRadius = 10.0f, .nearestSea = 10.0f, .farthestSea = 90.0f, .riverDistance = 5.0f};
	const auto start = FindStartArea(heights, far, far, settings);
	FlattenStartArea(heights, start, settings);
	const auto cx = static_cast<int>(start.x);
	const auto cz = static_cast<int>(start.z);
	for (int d = -5; d <= 5; ++d)
	{
		EXPECT_FLOAT_EQ(heights.At(cx + d, cz), heights.At(cx, cz));
	}
}

TEST(LandgenGenerator, TheSameSeedGivesTheSameLand)
{
	const auto first = Generate(SmallSettings(1234));
	const auto second = Generate(SmallSettings(1234));
	const auto other = Generate(SmallSettings(1235));
	EXPECT_EQ(HashOf(first.cells), HashOf(second.cells));
	EXPECT_NE(HashOf(first.cells), HashOf(other.cells));
	EXPECT_EQ(first.features.size(), second.features.size());
	EXPECT_EQ(first.rivers.size(), second.rivers.size());
}

TEST(LandgenGenerator, TheLandIsTheSameOnEveryPlatform)
{
	// The bytes of a small land, as the generator gives them on Windows with MSVC and with Clang. A change of this value
	// on another platform means that the maths of the generator is not the same there.
	EXPECT_EQ(HashOf(Generate(SmallSettings(77)).cells), k_SmallLandHash);
}

TEST(LandgenGenerator, TheLandIsAnIslandWithEachPart)
{
	const auto land = Generate(SmallSettings(99));
	const int side = land.cells.side;
	for (int i = 0; i < side; ++i)
	{
		EXPECT_EQ(land.cells.At(i, 0).altitude, 0);
		EXPECT_EQ(land.cells.At(0, i).altitude, 0);
	}
	EXPECT_EQ(land.countries.size(), biomes::k_BiomeCount);
	EXPECT_EQ(land.materials, countries::UsedSurfaces());
	EXPECT_TRUE(std::ranges::any_of(land.cells.values, [](const auto& cell) { return cell.altitude > 100; }));
	EXPECT_FALSE(land.features.empty());
	EXPECT_FALSE(land.times.empty());
	// Each country index of a cell names a country of the land
	for (const auto& cell : land.cells.values)
	{
		ASSERT_LT(cell.properties.country, land.countries.size());
	}
}

TEST(LandgenGenerator, EveryBlockHasTheSameDiagonals)
{
	// The renderer keeps a list of triangles for each pattern of diagonals. One pattern for all blocks keeps the lists few.
	const auto land = Generate(SmallSettings(77));
	constexpr int k_Block = 16;
	for (int x = 0; x + k_Block < land.cells.side - 1; ++x)
	{
		for (int z = 0; z + k_Block < land.cells.side - 1; ++z)
		{
			ASSERT_EQ(land.cells.At(x, z).properties.split, land.cells.At(x + k_Block, z).properties.split);
			ASSERT_EQ(land.cells.At(x, z).properties.split, land.cells.At(x, z + k_Block).properties.split);
		}
	}
}
