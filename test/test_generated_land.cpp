/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>

#include <gtest/gtest.h>

#include "3D/GeneratedLand.h"
#include "Audio/SoundMap.h"

using namespace openblack;
using landgen::countries::Surface;

namespace
{
/// The parts of the story with a plain material for each surface, whose type is the number of the surface
generated_land::StoryParts FakeParts()
{
	generated_land::StoryParts parts;
	for (size_t surface = 0; surface < landgen::countries::k_SurfaceCount; ++surface)
	{
		auto& material = parts.surfaces.emplace_back();
		material.type = static_cast<uint16_t>(surface);
	}
	parts.noise.assign(static_cast<size_t>(lnd::LNDBumpMap::k_Width) * lnd::LNDBumpMap::k_Height, 3);
	parts.bump.assign(static_cast<size_t>(lnd::LNDBumpMap::k_Width) * lnd::LNDBumpMap::k_Height, 0x80);
	return parts;
}
} // namespace

TEST(GeneratedLand, ThePathKeepsTheSeed)
{
	EXPECT_EQ(generated_land::PathOf(42), "landgen:42");
	EXPECT_EQ(generated_land::SeedOf(generated_land::PathOf(18446744073709551615ull)), 18446744073709551615ull);
	EXPECT_FALSE(generated_land::SeedOf("testbed").has_value());
	EXPECT_FALSE(generated_land::SeedOf("landgen:").has_value());
	EXPECT_FALSE(generated_land::SeedOf("landgen:12x").has_value());
	EXPECT_FALSE(generated_land::SeedOf("Data/Landscape/Land1.lnd").has_value());
}

TEST(GeneratedLand, EachSurfaceTakesAMaterialOfTheStory)
{
	for (const auto& source : generated_land::MaterialSources())
	{
		EXPECT_TRUE(source.landscape.ends_with(".lnd"));
	}
}

TEST(GeneratedLand, OnlyTheBlocksWithLandAreKept)
{
	landgen::GeneratedLand land;
	land.cells = landgen::Grid<lnd::LNDCell>(landgen::k_MapCorners, lnd::LNDCell {});
	// Land in the middle of block (10, 12) only
	land.cells.At((10 * 16) + 8, (12 * 16) + 8).altitude = 40;
	land.countries.resize(landgen::biomes::k_BiomeCount);
	land.materials = {Surface::Grass, Surface::Snow};
	const auto parts = FakeParts();

	const auto data = generated_land::Build(land, parts);
	ASSERT_EQ(data.blocks.size(), 1u);
	const auto& block = data.blocks.front();
	EXPECT_EQ(block.blockX, 10u);
	EXPECT_EQ(block.blockZ, 12u);
	EXPECT_EQ(block.cells.at((8 * 17) + 8).altitude, 40);
	EXPECT_EQ(data.blockIndexLookup.at((10 * 32) + 12), 1);
	EXPECT_EQ(std::ranges::count(data.blockIndexLookup, uint16_t {0}), 1023);
	ASSERT_EQ(data.materials.size(), 2u);
	EXPECT_EQ(data.materials[0].type, static_cast<uint16_t>(Surface::Grass));
	EXPECT_EQ(data.materials[1].type, static_cast<uint16_t>(Surface::Snow));
	EXPECT_EQ(data.countries.size(), landgen::biomes::k_BiomeCount);
	EXPECT_EQ(data.noise, parts.noise);
	EXPECT_EQ(data.bump, parts.bump);
}

TEST(GeneratedLand, TheSoundsHaveTheNumbersOfTheGame)
{
	EXPECT_EQ(static_cast<int>(landgen::Sound::Sea), static_cast<int>(audio::AtmosType::Sea));
	EXPECT_EQ(static_cast<int>(landgen::Sound::Coastal), static_cast<int>(audio::AtmosType::Coastal));
	EXPECT_EQ(static_cast<int>(landgen::Sound::Jungle), static_cast<int>(audio::AtmosType::Jungle));
	EXPECT_EQ(static_cast<int>(landgen::Sound::Countryside), static_cast<int>(audio::AtmosType::Countryside));
	EXPECT_EQ(static_cast<int>(landgen::Sound::RunningWater), static_cast<int>(audio::AtmosType::RunningWater));
}
