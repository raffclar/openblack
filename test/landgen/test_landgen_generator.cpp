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
#include <landgen/Cursed.h>
#include <landgen/Features.h>
#include <landgen/Generator.h>

using namespace openblack::landgen;
using biomes::Biome;

namespace
{
/// The hash of the cells of the small land of seed 77
constexpr uint64_t k_SmallLandHash = 0x4B6F2735EE622750ull;

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
	// The big island is the shape that the land had before the types of land
	settings.landType.type = landtypes::Type::BigIsland;
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
                                       int side = 64, HeightCurve curve = {})
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
	                        .townZ = k_FarTown,
	                        .curve = curve},
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
		// NOLINTNEXTLINE(bugprone-incorrect-roundings): A different rounding gives a different result.
		EXPECT_NE(static_cast<int>(x + 0.5f), 40);
	}
	EXPECT_EQ(placed.size(), features::Place(ground, 5, {}).size());
}

TEST(LandgenFeatures, TheRulesUseTheMetresOfTheHeightCurve)
{
	// 60 altitude units are 40 m on the curve of the story. On a curve with its knee at 30 units and the top at 1250 m,
	// they are more than 150 m: higher than every tree of the story.
	const auto high = HeightCurve::WithTop(30.0f, 1250.0f);
	ASSERT_GT(high.MetresOf(60.0f), 150.0f);
	EXPECT_TRUE(std::ranges::any_of(PlaceOn(Biome::TemperateForest, 60.0f, 3, {}, 128), IsTree));
	EXPECT_FALSE(std::ranges::any_of(PlaceOn(Biome::TemperateForest, 60.0f, 3, {}, 128, high), IsTree));

	// A ramp that rises one altitude unit for each corner along x, from 40 to 103 units
	const Grid<Biome> biomes(64, Biome::Grassland);
	Grid<float> heights(64, 0.0f);
	for (int x = 0; x < 64; ++x)
	{
		for (int z = 0; z < 64; ++z)
		{
			heights.At(x, z) = 40.0f + static_cast<float>(x);
		}
	}
	const Grid<float> sea(64, 40.0f);
	const auto keepClear = Clear(64);
	const auto spotOn = [&](const HeightCurve& curve, float x) {
		return features::SpotAt({.biomes = biomes,
		                         .heights = heights,
		                         .seaDistance = sea,
		                         .keepClear = keepClear,
		                         .clearings = {},
		                         .townX = k_FarTown,
		                         .townZ = k_FarTown,
		                         .curve = curve},
		                        x, 32.0f);
	};
	// Below the knee, the curve of the story and the high curve give the same ground
	const auto story = spotOn({}, 2.0f);
	EXPECT_NEAR(story.height, 42.0f * 0.67f, 1e-3f);
	EXPECT_NEAR(story.slope, 0.067f, 1e-4f);
	// Above the knee, the height and the slope are the metres that the game draws
	const auto drawn = spotOn(high, 40.0f);
	EXPECT_NEAR(drawn.height, high.MetresOf(80.0f), 1e-2f);
	EXPECT_NEAR(drawn.slope, (high.MetresOf(81.0f) - high.MetresOf(79.0f)) / 20.0f, 1e-3f);
	EXPECT_GT(drawn.slope, spotOn({}, 40.0f).slope * 5.0f);
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
	EXPECT_GT(std::ranges::count_if(jungle, IsTree), std::ranges::count_if(grassland, IsTree) * 2);
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

TEST(LandgenFeatures, AHigherTreeDensityGivesMoreTreesOfTheSameKinds)
{
	features::Settings dense;
	dense.treeDensity = 4.0f;
	dense.most = 100000;
	std::vector<features::Feature> usual;
	std::vector<features::Feature> more;
	std::ranges::copy_if(PlaceOn(Biome::TemperateForest, 60.0f, 7, {}, 128), std::back_inserter(usual), IsTree);
	std::ranges::copy_if(PlaceOn(Biome::TemperateForest, 60.0f, 7, dense, 128), std::back_inserter(more), IsTree);
	ASSERT_GT(usual.size(), 200u);
	// Four times the stands give about four times the trees. Stands that overlap lose some trees.
	EXPECT_GT(more.size(), 3 * usual.size());
	EXPECT_LT(more.size(), 5 * usual.size());
	// The dense land has only the kinds that the usual land has
	std::vector<uint8_t> usualKinds;
	std::ranges::transform(usual, std::back_inserter(usualKinds), &features::Feature::type);
	for (const auto& tree : more)
	{
		EXPECT_NE(std::ranges::find(usualKinds, tree.type), usualKinds.end());
	}
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

namespace
{
bool IsRock(const features::Feature& feature)
{
	return feature.kind == features::Kind::Static && feature.type != features::k_DeadTreeStatic;
}
} // namespace

TEST(LandgenFeatures, ClumpsOfRocksHaveTheSizesOfTheClumpsOfTheStory)
{
	using features::ClumpSize;
	using features::RockClass;
	EXPECT_EQ(ClumpSize(RockClass::Small, 0.0f), 1u);
	EXPECT_EQ(ClumpSize(RockClass::Small, 0.5f), 3u);
	EXPECT_EQ(ClumpSize(RockClass::Boulder, 0.4f), 1u);
	EXPECT_EQ(ClumpSize(RockClass::Slab, 1.0f), 5u);
	EXPECT_GE(ClumpSize(RockClass::Small, 0.999f), 14u);
	for (float u = 0.0f; u < 0.99f; u += 0.01f)
	{
		EXPECT_LE(ClumpSize(RockClass::Boulder, u), ClumpSize(RockClass::Boulder, u + 0.01f));
		// A clump of small rocks is never smaller than a clump of slabs of the same random number
		EXPECT_GE(ClumpSize(RockClass::Small, u), ClumpSize(RockClass::Slab, u));
	}
}

TEST(LandgenFeatures, SlabsLieOnSteepGroundAndBouldersOnHighGround)
{
	using features::MainClassAt;
	using features::RockClass;
	// On flat ground at 20 m: small rocks first, slabs last
	EXPECT_EQ(MainClassAt(0.0f, 20.0f, 0.1f), RockClass::Small);
	EXPECT_EQ(MainClassAt(0.0f, 20.0f, 0.95f), RockClass::Slab);
	EXPECT_EQ(MainClassAt(0.0f, 20.0f, 0.8f), RockClass::Boulder);
	// Steep ground has more slabs, high ground more boulders
	EXPECT_EQ(MainClassAt(0.5f, 20.0f, 0.8f), RockClass::Slab);
	EXPECT_EQ(MainClassAt(0.0f, 100.0f, 0.3f), RockClass::Boulder);
	const auto count = [](float slope, float height, RockClass rockClass) {
		int found = 0;
		for (float u = 0.0f; u < 1.0f; u += 0.001f)
		{
			found += MainClassAt(slope, height, u) == rockClass ? 1 : 0;
		}
		return found;
	};
	EXPECT_GT(count(0.5f, 20.0f, RockClass::Slab), count(0.0f, 20.0f, RockClass::Slab));
	EXPECT_GT(count(0.0f, 100.0f, RockClass::Boulder), count(0.0f, 20.0f, RockClass::Boulder));
}

TEST(LandgenFeatures, RocksLieInClumpsWithTheLeanOfTheStory)
{
	const auto placed = PlaceOn(Biome::Alpine, 90.0f, 21, {}, 128);
	std::vector<features::Feature> rocks;
	std::ranges::copy_if(placed, std::back_inserter(rocks), IsRock);
	ASSERT_GE(rocks.size(), 50u);
	size_t inClump = 0;
	size_t leaning = 0;
	for (size_t i = 0; i < rocks.size(); ++i)
	{
		bool near = false;
		for (size_t j = 0; j < rocks.size(); ++j)
		{
			if (i == j)
			{
				continue;
			}
			const float distance = std::hypot(rocks[i].x - rocks[j].x, rocks[i].z - rocks[j].z);
			EXPECT_GE(distance, 1.2f);
			near = near || distance <= 8.0f;
		}
		inClump += near ? 1 : 0;
		leaning += std::max(std::fabs(rocks[i].xAngle), std::fabs(rocks[i].zAngle)) > 0.05f ? 1 : 0;
		// The poses are the poses of the rocks of the story
		EXPECT_GE(rocks[i].altitude, -1.5f);
		EXPECT_LE(rocks[i].altitude, 3.0f);
		EXPECT_GT(rocks[i].size, 0.1f);
		EXPECT_LT(rocks[i].size, 7.5f);
	}
	// Most rocks of the story lie near another rock, and most lean
	EXPECT_GT(inClump * 2, rocks.size());
	EXPECT_GT(leaning * 2, rocks.size());
	// The rocks are boulders, slabs and small rocks
	EXPECT_TRUE(std::ranges::any_of(rocks, [](const auto& rock) { return rock.type >= 16 && rock.type <= 27; }));
	EXPECT_TRUE(std::ranges::any_of(rocks, [](const auto& rock) { return rock.type >= 28 && rock.type <= 35; }));
	EXPECT_TRUE(std::ranges::any_of(rocks, [](const auto& rock) { return rock.type == 2 || rock.type == 37; }));
}

TEST(LandgenFeatures, TheRingOfRocksStandsOnFlatGrass)
{
	// On a land whose small rocks are of a stone, the ring holds the only plain rocks
	bool found = false;
	for (uint64_t seed = 1; seed <= 40 && !found; ++seed)
	{
		const auto placed = PlaceOn(Biome::Grassland, 60.0f, seed, {}, 96);
		std::vector<features::Feature> plain;
		std::ranges::copy_if(placed, std::back_inserter(plain), [](const auto& feature) {
			return IsRock(feature) && feature.type == features::k_PlainRockStatic;
		});
		if (plain.size() != 8 ||
		    std::ranges::any_of(placed, [](const auto& feature) { return IsRock(feature) && feature.type == 37; }) == false)
		{
			continue;
		}
		found = true;
		float middleX = 0.0f;
		float middleZ = 0.0f;
		for (const auto& rock : plain)
		{
			middleX += rock.x / 8.0f;
			middleZ += rock.z / 8.0f;
		}
		for (const auto& rock : plain)
		{
			const float radius = std::hypot(rock.x - middleX, rock.z - middleZ);
			EXPECT_GE(radius, 15.0f);
			EXPECT_LE(radius, 46.0f);
			EXPECT_GE(rock.size, 0.47f);
			EXPECT_LE(rock.size, 0.79f);
		}
	}
	EXPECT_TRUE(found);
}

namespace
{
/// A fake land: the sea, low grass, then a high plateau above the tree line
struct TreeLineLand
{
	static constexpr int k_Side = 96;
	Grid<Biome> biomes {k_Side, Biome::Grassland};
	Grid<float> heights {k_Side, 20.0f};
	Grid<float> sea {k_Side, 40.0f};

	TreeLineLand()
	{
		for (int z = 0; z < k_Side; ++z)
		{
			for (int x = 0; x < k_Side; ++x)
			{
				if (x < 8)
				{
					biomes.At(x, z) = Biome::Sea;
					heights.At(x, z) = 0.0f;
				}
				else if (x >= 52)
				{
					// 167.5 m on the curve of the lands of the story
					heights.At(x, z) = 250.0f;
				}
				sea.At(x, z) = static_cast<float>(std::max(x - 7, 0));
			}
		}
	}
};
} // namespace

TEST(LandgenFeatures, TheStandFillLeavesOutTheHighAndTheSteepGround)
{
	const TreeLineLand land;
	const auto keepClear = Clear(TreeLineLand::k_Side);
	const auto fill = features::StandFill({.biomes = land.biomes,
	                                       .heights = land.heights,
	                                       .seaDistance = land.sea,
	                                       .keepClear = keepClear,
	                                       .clearings = {},
	                                       .townX = k_FarTown,
	                                       .townZ = k_FarTown});
	// The land is the columns 8 to 95. The low grass is the columns 8 to 51, but the slope at 51 is too steep.
	EXPECT_FLOAT_EQ(fill.at(static_cast<size_t>(Biome::Grassland)), 88.0f / 43.0f);
	// A biome that has no ground keeps its stands
	EXPECT_FLOAT_EQ(fill.at(static_cast<size_t>(Biome::Desert)), 1.0f);
}

TEST(LandgenFeatures, MoreStandsStartWhenPartOfTheLandIsAboveTheTreeLine)
{
	const TreeLineLand land;
	const auto keepClear = Clear(TreeLineLand::k_Side);
	const auto trees = [&](bool fill) {
		features::Settings settings;
		settings.fillGrowableGround = fill;
		size_t count = 0;
		for (uint64_t seed = 1; seed <= 4; ++seed)
		{
			const auto placed = features::Place({.biomes = land.biomes,
			                                     .heights = land.heights,
			                                     .seaDistance = land.sea,
			                                     .keepClear = keepClear,
			                                     .clearings = {},
			                                     .townX = k_FarTown,
			                                     .townZ = k_FarTown},
			                                    seed, settings);
			for (const auto& feature : placed)
			{
				if (feature.kind == features::Kind::Tree)
				{
					// No tree grows on the plateau
					EXPECT_LT(feature.x / 10.0f, 52.5f);
					++count;
				}
			}
		}
		return count;
	};
	// Half of the land holds no tree, so the low grass gets about twice the stands
	EXPECT_GT(trees(true), trees(false) * 4 / 3);
}

TEST(LandgenFeatures, SpikyPillarsStandInGroupsOnTheBeach)
{
	constexpr int k_Side = 96;
	Grid<Biome> biomes(k_Side, Biome::Grassland);
	Grid<float> heights(k_Side, 60.0f);
	Grid<float> sea(k_Side, 40.0f);
	for (int z = 0; z < k_Side; ++z)
	{
		for (int x = 0; x < 30; ++x)
		{
			// The sea, then a beach 2.7 m above the sea, then the grass
			biomes.At(x, z) = x < 10 ? Biome::Sea : Biome::Beach;
			heights.At(x, z) = x < 10 ? 0.0f : 4.0f;
			sea.At(x, z) = x < 10 ? 0.0f : static_cast<float>(x - 9);
		}
	}
	const auto keepClear = Clear(k_Side);
	size_t total = 0;
	for (uint64_t seed = 1; seed <= 10; ++seed)
	{
		const auto placed = features::Place({.biomes = biomes,
		                                     .heights = heights,
		                                     .seaDistance = sea,
		                                     .keepClear = keepClear,
		                                     .clearings = {},
		                                     .townX = k_FarTown,
		                                     .townZ = k_FarTown},
		                                    seed, {});
		std::vector<features::Feature> pillars;
		std::ranges::copy_if(placed, std::back_inserter(pillars),
		                     [](const auto& feature) { return feature.kind == features::Kind::Feature; });
		EXPECT_LE(pillars.size(), 4u * 7u);
		for (const auto& pillar : pillars)
		{
			// Spiky pillars of limestone on the beach
			EXPECT_EQ(pillar.type, features::k_SpikyPillarFeature + 1);
			EXPECT_GE(pillar.x / 10.0f, 9.5f);
			EXPECT_LT(pillar.x / 10.0f, 30.5f);
		}
		total += pillars.size();
	}
	EXPECT_GE(total, 3u);
}

namespace
{
/// A fake land for the cursed places: alpine ground at 60 m, 400 m from the sea, with the town at the middle. The
/// first columns are sea.
struct CursedLand
{
	static constexpr int k_Side = 160;
	static constexpr int k_SeaColumns = 20;
	Grid<Biome> biomes {k_Side, Biome::Alpine};
	Grid<float> heights {k_Side, 90.0f};
	Grid<float> sea {k_Side, 40.0f};
	Grid<uint8_t> keepClear {k_Side, 0};

	CursedLand()
	{
		for (int z = 0; z < k_Side; ++z)
		{
			for (int x = 0; x < k_SeaColumns; ++x)
			{
				biomes.At(x, z) = Biome::Sea;
				heights.At(x, z) = 0.0f;
				sea.At(x, z) = 0.0f;
			}
		}
	}

	[[nodiscard]] features::Ground Ground() const
	{
		return {.biomes = biomes,
		        .heights = heights,
		        .seaDistance = sea,
		        .keepClear = keepClear,
		        .clearings = {},
		        .townX = k_Side / 2.0f,
		        .townZ = k_Side / 2.0f};
	}
};

bool IsVolcanicBoulder(const features::Feature& feature)
{
	using features::RockShape;
	using features::RockType;
	using features::Stone;
	return feature.kind == features::Kind::Static && (feature.type == RockType(RockShape::Boulder1, Stone::Volcanic) ||
	                                                  feature.type == RockType(RockShape::Boulder2, Stone::Volcanic) ||
	                                                  feature.type == RockType(RockShape::Boulder3, Stone::Volcanic));
}

bool IsCrater(const features::Feature& feature)
{
	return feature.kind == features::Kind::Feature && feature.type == cursed::k_CraterFeature;
}

bool IsBurnt(const features::Feature& feature)
{
	return feature.kind == features::Kind::Tree && feature.type == static_cast<uint8_t>(features::Tree::Burnt);
}

bool IsLyingDeadTree(const features::Feature& feature)
{
	return feature.kind == features::Kind::Static && feature.type == features::k_DeadTreeStatic;
}
} // namespace

TEST(LandgenCursed, MostLandsHaveOnePlaceAndSomeHaveThree)
{
	const cursed::Settings settings;
	EXPECT_EQ(cursed::PatchCount(0.0f, settings), 1u);
	EXPECT_EQ(cursed::PatchCount(0.5f, settings), 1u);
	EXPECT_EQ(cursed::PatchCount(0.7f, settings), 2u);
	EXPECT_EQ(cursed::PatchCount(0.999f, settings), 3u);
	EXPECT_EQ(cursed::PatchCount(0.5f, {.fewest = 0, .most = 0}), 0u);
}

TEST(LandgenCursed, PlacesKeepFarFromTheTownTheSeaAndEachOther)
{
	const CursedLand land;
	const auto ground = land.Ground();
	const cursed::Settings settings;
	const float town = CursedLand::k_Side * 5.0f;
	size_t patches = 0;
	for (uint64_t seed = 1; seed <= 12; ++seed)
	{
		std::vector<features::Feature> placed;
		const auto result = cursed::Place(ground, placed, seed, settings);
		ASSERT_GE(result.patches.size(), 1u);
		ASSERT_LE(result.patches.size(), 3u);
		patches += result.patches.size();
		for (size_t i = 0; i < result.patches.size(); ++i)
		{
			const auto& patch = result.patches[i];
			EXPECT_GE(patch.radius, settings.smallestRadius);
			EXPECT_LE(patch.radius, settings.largestRadius);
			EXPECT_GE(std::hypot(patch.x - town, patch.z - town), settings.nearestTown + patch.radius);
			for (size_t j = 0; j < i; ++j)
			{
				const auto& other = result.patches[j];
				EXPECT_GE(std::hypot(patch.x - other.x, patch.z - other.z), patch.radius + other.radius + settings.gap);
			}
		}
		// Nothing of a cursed place stands in the sea
		for (const auto& feature : placed)
		{
			EXPECT_GE(feature.x, CursedLand::k_SeaColumns * 10.0f);
		}
	}
	EXPECT_GT(patches, 12u);
}

TEST(LandgenCursed, EachPlaceHasTheThingsOfTheStory)
{
	const CursedLand land;
	const auto ground = land.Ground();
	for (uint64_t seed = 1; seed <= 12; ++seed)
	{
		std::vector<features::Feature> placed;
		const auto result = cursed::Place(ground, placed, seed, {});
		uint32_t stacks = 0;
		uint32_t craters = 0;
		uint32_t burnt = 0;
		uint32_t lying = 0;
		for (const auto& patch : result.patches)
		{
			EXPECT_GE(patch.stacks, 1u);
			EXPECT_LE(patch.stacks, 3u);
			EXPECT_LE(patch.craters, 2u);
			EXPECT_GE(patch.burntTrees + patch.deadTrees, 10u);
			EXPECT_LE(patch.burntTrees + patch.deadTrees, 40u);
			stacks += patch.stacks;
			craters += patch.craters;
			burnt += patch.burntTrees;
			lying += patch.deadTrees;
		}
		// Three puffs for each stack and one puff of smoke for each crater
		EXPECT_EQ(result.mists.size(), (stacks * 3) + craters);
		EXPECT_EQ(std::ranges::count_if(placed, IsCrater), craters);
		EXPECT_EQ(std::ranges::count_if(placed, IsBurnt), burnt);
		EXPECT_EQ(std::ranges::count_if(placed, IsLyingDeadTree), lying);
		for (const auto& mist : result.mists)
		{
			// The near black mist of the first land, the dark mist of the fourth and the grey smoke of the fifth
			const auto colour = mist.colour & 0xFFFFFFu;
			EXPECT_TRUE(colour == 0x0A0400u || colour == 0x212B29u || colour == 0x505050u);
			if (colour == 0x505050u)
			{
				EXPECT_FLOAT_EQ(mist.edgeShrink, 2.642276f);
				// The smoke is at the middle of a crater
				EXPECT_TRUE(std::ranges::any_of(placed, [&mist](const auto& feature) {
					return IsCrater(feature) && feature.x == mist.x && feature.z == mist.z;
				}));
			}
		}
		for (const auto& patch : result.patches)
		{
			// Each stack of the fourth land has its seven volcanic boulders
			const auto boulders = std::ranges::count_if(placed, [&patch](const auto& feature) {
				return IsVolcanicBoulder(feature) && std::hypot(feature.x - patch.x, feature.z - patch.z) <= patch.radius;
			});
			EXPECT_EQ(boulders, patch.stack == cursed::Stack::Volcanic ? patch.stacks * 7 : 0);
		}
	}
}

TEST(LandgenCursed, TheBurntTreesKeepToTheMostOfTheStoryForTheArea)
{
	const cursed::Settings settings;
	EXPECT_EQ(cursed::BurntBudget(2.06f, settings), 2u);
	EXPECT_EQ(cursed::BurntBudget(12.0f, settings), 17u);
	EXPECT_EQ(cursed::BurntBudget(-1.0f, settings), 0u);

	const CursedLand land;
	const auto ground = land.Ground();
	// The land is 140 columns of 160 corners of 10 m
	constexpr float k_LandKm2 = 2.24f;
	for (uint64_t seed = 1; seed <= 12; ++seed)
	{
		std::vector<features::Feature> placed;
		const auto result = cursed::Place(ground, placed, seed, settings);
		EXPECT_LE(std::ranges::count_if(placed, IsBurnt), cursed::BurntBudget(k_LandKm2, settings));
		for (const auto& patch : result.patches)
		{
			// The other dead trees of the place lie on the ground
			EXPECT_GE(patch.burntTrees + patch.deadTrees, 10u);
		}
	}
}

TEST(LandgenCursed, APlaceWithNoRoomForAStackGivesTheLandItsFeaturesBack)
{
	// A sawtooth of steep slopes: each corner is 15 m above the corner before it, then the ground falls 60 m
	CursedLand land;
	for (int x = CursedLand::k_SeaColumns; x < CursedLand::k_Side; ++x)
	{
		for (int z = 0; z < CursedLand::k_Side; ++z)
		{
			land.heights.At(x, z) = 45.0f + (15.0f * static_cast<float>(x % 5));
		}
	}
	const auto ground = land.Ground();
	std::vector<features::Feature> rocks;
	rocks.reserve(CursedLand::k_Side);
	for (int x = 0; x < CursedLand::k_Side; ++x)
	{
		rocks.push_back({.kind = features::Kind::Static,
		                 .type = features::k_PlainRockStatic,
		                 .x = (static_cast<float>(x) * 10.0f) + 5.0f,
		                 .z = 800.0f,
		                 .angle = 0.0f,
		                 .size = 0.5f,
		                 .maxSize = 0.5f});
	}
	for (uint64_t seed = 1; seed <= 12; ++seed)
	{
		auto placed = rocks;
		const auto result = cursed::Place(ground, placed, seed, {});
		for (const auto& patch : result.patches)
		{
			EXPECT_GE(patch.stacks, 1u);
			EXPECT_GE(patch.burntTrees + patch.deadTrees, 10u);
		}
		if (result.patches.empty())
		{
			EXPECT_TRUE(result.mists.empty());
			ASSERT_EQ(placed.size(), rocks.size());
			for (size_t i = 0; i < placed.size(); ++i)
			{
				EXPECT_EQ(placed[i].x, rocks[i].x);
			}
		}
	}
}

TEST(LandgenCursed, TheDarkGroundHasABurntMiddleAndAnEdgeThatIsNotRound)
{
	const cursed::Settings settings;
	const cursed::Patch patch {.x = 1000.0f,
	                           .z = 1000.0f,
	                           .radius = 100.0f,
	                           .stack = cursed::Stack::Black,
	                           .burntTrees = 0,
	                           .deadTrees = 0,
	                           .stacks = 1,
	                           .craters = 0};
	EXPECT_EQ(cursed::ScorchAt(patch, 1000.0f, 1000.0f, 3, settings), cursed::Scorch::Core);
	EXPECT_EQ(cursed::ScorchAt(patch, 1000.0f, 1101.0f, 3, settings), cursed::Scorch::None);
	EXPECT_EQ(cursed::ScorchAt(patch, 1000.0f, 1000.0f + 0.69f * 0.55f * 100.0f, 3, settings), cursed::Scorch::Core);
	// Along a circle at 85% of the radius, some points are on the dark ground and some are not
	int dark = 0;
	int edge = 0;
	constexpr int k_Points = 72;
	for (int i = 0; i < k_Points; ++i)
	{
		const float turn = static_cast<float>(i) * 6.2831853f / static_cast<float>(k_Points);
		const auto scorch =
		    cursed::ScorchAt(patch, 1000.0f + (85.0f * std::cos(turn)), 1000.0f + (85.0f * std::sin(turn)), 3, settings);
		dark += scorch != cursed::Scorch::None ? 1 : 0;
		edge += scorch == cursed::Scorch::Edge ? 1 : 0;
		EXPECT_NE(scorch, cursed::Scorch::Core);
	}
	EXPECT_GT(dark, 0);
	EXPECT_LT(dark, k_Points);
	EXPECT_EQ(edge, dark);
}

TEST(LandgenCursed, TheCountriesOfTheDarkGroundUseTheirOwnSurfaces)
{
	const auto recipes = countries::CursedRecipes();
	const auto materials = countries::UsedSurfaces();
	EXPECT_EQ(recipes[0].bands.front().surface, countries::Surface::DarkScrub);
	EXPECT_EQ(recipes[1].bands.front().surface, countries::Surface::Charred);
	EXPECT_NE(std::ranges::find(materials, countries::Surface::DarkScrub), materials.end());
	EXPECT_NE(std::ranges::find(materials, countries::Surface::Charred), materials.end());
}

TEST(LandgenCursed, AStackOrACraterClearsTheFeaturesInItsWay)
{
	const CursedLand land;
	const auto ground = land.Ground();
	// Small rocks every 5 m over the whole land
	std::vector<features::Feature> rocks;
	for (int x = 0; x < CursedLand::k_Side * 2; ++x)
	{
		for (int z = 0; z < CursedLand::k_Side * 2; ++z)
		{
			rocks.push_back({.kind = features::Kind::Static,
			                 .type = features::k_PlainRockStatic,
			                 .x = (static_cast<float>(x) * 5.0f) + 2.5f,
			                 .z = (static_cast<float>(z) * 5.0f) + 2.5f,
			                 .angle = 0.0f,
			                 .size = 0.5f,
			                 .maxSize = 0.5f});
		}
	}
	for (uint64_t seed = 1; seed <= 6; ++seed)
	{
		auto placed = rocks;
		const auto result = cursed::Place(ground, placed, seed, {});
		for (const auto& crater : placed)
		{
			if (!IsCrater(crater))
			{
				continue;
			}
			// Only the crater is near its middle
			EXPECT_EQ(std::ranges::count_if(placed,
			                                [&crater](const auto& feature) {
				                                return std::hypot(feature.x - crater.x, feature.z - crater.z) < 20.0f;
			                                }),
			          1);
		}
		// No plain rock is left among the boulders of a stack
		for (const auto& mist : result.mists)
		{
			if ((mist.colour & 0xFFFFFFu) == 0x505050u)
			{
				continue;
			}
			EXPECT_FALSE(std::ranges::any_of(placed, [&mist](const auto& feature) {
				return feature.type == features::k_PlainRockStatic && std::hypot(feature.x - mist.x, feature.z - mist.z) < 5.0f;
			}));
		}
	}
}

TEST(LandgenCursed, TheSameSeedGivesTheSamePlaces)
{
	const CursedLand land;
	const auto ground = land.Ground();
	std::vector<features::Feature> first;
	std::vector<features::Feature> second;
	const auto a = cursed::Place(ground, first, 9, {});
	const auto b = cursed::Place(ground, second, 9, {});
	ASSERT_EQ(a.patches.size(), b.patches.size());
	ASSERT_EQ(a.mists.size(), b.mists.size());
	ASSERT_EQ(first.size(), second.size());
	for (size_t i = 0; i < first.size(); ++i)
	{
		EXPECT_EQ(first[i].type, second[i].type);
		EXPECT_EQ(first[i].x, second[i].x);
		EXPECT_EQ(first[i].z, second[i].z);
	}
}

TEST(LandgenCursed, BurntTreesStayInNoForest)
{
	std::vector<features::Feature> placed;
	placed.reserve(20);
	for (int i = 0; i < 20; ++i)
	{
		placed.push_back({.kind = features::Kind::Tree,
		                  .type = static_cast<uint8_t>(i % 2 == 0 ? features::Tree::Burnt : features::Tree::Oak),
		                  .x = 10.0f + static_cast<float>(i),
		                  .z = 10.0f,
		                  .angle = 0.0f,
		                  .size = 1.0f,
		                  .maxSize = 1.0f});
	}
	const auto forests = features::GroupForests(placed, 0.0f, 0.0f, {.square = 120.0f, .fewest = 5, .most = 4});
	ASSERT_EQ(forests.size(), 1u);
	for (const auto& feature : placed)
	{
		EXPECT_EQ(feature.forest, IsBurnt(feature) ? 0u : 1u);
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
	const auto cells = MakeCells(altitudes, light, biomes, Clear(6));
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

TEST(LandgenGenerator, NoCellHasTheSoundOfRunningWater)
{
	// As on the lands of the story, the cells by the streams have the sound of the land round them
	const auto land = Generate(SmallSettings(77));
	ASSERT_FALSE(land.rivers.empty());
	const auto running = static_cast<uint8_t>(Sound::RunningWater);
	const auto count = std::ranges::count_if(
	    land.cells.values, [running](const openblack::lnd::LNDCell& cell) { return ((cell.flags >> 2u) & 0xFu) == running; });
	EXPECT_EQ(count, 0);
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

TEST(LandgenGenerator, TheHeightFieldKeepsTheLowLandAndRisesAboveTheBytes)
{
	const auto land = Generate(SmallSettings(99));
	ASSERT_EQ(land.heightUnits.side, land.cells.side);
	bool above = false;
	for (int x = 0; x < land.cells.side; ++x)
	{
		for (int z = 0; z < land.cells.side; ++z)
		{
			const auto altitude = land.cells.At(x, z).altitude;
			const auto units = land.heightUnits.At(x, z);
			// Below the knee, the height is the altitude, as on the lands of the story. An altitude rounded down to the
			// knee can come from a height above it.
			if (static_cast<float>(altitude) + 0.5f <= land.heightCurve.knee)
			{
				EXPECT_EQ(units, altitude);
			}
			// The altitude byte and the height units come from the same height, each rounded by half a unit or less
			EXPECT_NEAR(land.heightCurve.AltitudeOf(static_cast<float>(units)), static_cast<float>(altitude), 1.0f);
			above = above || units > 255;
		}
	}
	EXPECT_TRUE(above);
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
	// A country for each biome, then the country of the paths, the two countries of the edges of the lakes, then the two
	// countries of the dark ground of the cursed places when the land has a place
	EXPECT_EQ(land.countries.size(), biomes::k_BiomeCount + 1 + countries::LakeShoreRecipes().size() +
	                                     (land.cursedPatches.empty() ? 0 : countries::CursedRecipes().size()));
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

TEST(LandgenGenerator, NoTreeOrRockStandsOnTheWetSandOfALake)
{
	int beds = 0;
	for (const uint64_t seed : {77u, 78u, 79u, 80u})
	{
		const auto land = Generate(SmallSettings(seed));
		const auto onBed = [&land](float metresX, float metresZ) {
			const int x = static_cast<int>(std::floor((metresX / 10.0f) + 0.5f));
			const int z = static_cast<int>(std::floor((metresZ / 10.0f) + 0.5f));
			return land.cells.Contains(x, z) && land.cells.At(x, z).properties.country == k_LakeBedCountry;
		};
		beds += static_cast<int>(std::ranges::count_if(
		    land.cells.values, [](const auto& cell) { return cell.properties.country == k_LakeBedCountry; }));
		for (const auto& feature : land.features)
		{
			if (feature.kind != features::Kind::BigForest)
			{
				EXPECT_FALSE(onBed(feature.x, feature.z)) << seed << " " << feature.x << " " << feature.z;
			}
		}
	}
	ASSERT_GT(beds, 0);
}
