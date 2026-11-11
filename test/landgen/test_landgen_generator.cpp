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
} // namespace

TEST(LandgenFeatures, NothingStandsInTheWaterOrInAClearing)
{
	Grid<Biome> biomes(64, Biome::DenseJungle);
	Grid<float> heights(64, 30.0f);
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
	const auto placed = features::Place(biomes, heights, keepClear, clearings, 5, {});
	ASSERT_FALSE(placed.empty());
	for (const auto& feature : placed)
	{
		const float x = feature.x / 10.0f;
		const float z = feature.z / 10.0f;
		EXPECT_GE(x, 15.5f);
		EXPECT_GE(std::hypot(x - 50.0f, z - 50.0f), 6.0f);
		EXPECT_NE(static_cast<int>(x + 0.5f), 40);
	}
	EXPECT_EQ(placed.size(), features::Place(biomes, heights, keepClear, clearings, 5, {}).size());
}

TEST(LandgenFeatures, TheJungleIsDenserThanTheGrassland)
{
	const Grid<float> heights(64, 30.0f);
	const auto jungle = features::Place(Grid<Biome>(64, Biome::DenseJungle), heights, Clear(64), {}, 3, {});
	const auto grassland = features::Place(Grid<Biome>(64, Biome::Grassland), heights, Clear(64), {}, 3, {});
	EXPECT_GT(jungle.size(), grassland.size() * 5);
	// The jungle has palms and bushes; the dead woods have burnt trees
	for (const auto& feature : jungle)
	{
		if (feature.kind == features::Kind::Tree)
		{
			const auto tree = static_cast<features::Tree>(feature.type);
			EXPECT_TRUE((tree >= features::Tree::Palm && tree <= features::Tree::PalmC) ||
			            (tree >= features::Tree::Bush && tree <= features::Tree::BushB));
		}
	}
	const auto dead = features::Place(Grid<Biome>(64, Biome::DeadWoods), heights, Clear(64), {}, 3, {});
	EXPECT_TRUE(std::ranges::any_of(dead, [](const auto& feature) {
		return feature.kind == features::Kind::Tree && feature.type == static_cast<uint8_t>(features::Tree::Burnt);
	}));
}

TEST(LandgenFeatures, TheMostKeepsAnEvenShare)
{
	const Grid<float> heights(64, 30.0f);
	features::Settings settings;
	settings.most = 50;
	const auto placed = features::Place(Grid<Biome>(64, Biome::DenseJungle), heights, Clear(64), {}, 3, settings);
	EXPECT_EQ(placed.size(), 50u);
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
