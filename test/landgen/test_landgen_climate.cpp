/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <array>
#include <set>

#include <gtest/gtest.h>
#include <landgen/Biomes.h>
#include <landgen/Climate.h>
#include <landgen/Countries.h>
#include <landgen/Lighting.h>

using namespace openblack::landgen;
using biomes::Biome;
using countries::Surface;

namespace
{
template <typename T, typename Function>
Grid<T> MakeGrid(int side, Function value)
{
	Grid<T> grid(side, T {});
	for (int x = 0; x < side; ++x)
	{
		for (int z = 0; z < side; ++z)
		{
			grid.At(x, z) = value(x, z);
		}
	}
	return grid;
}

/// Temperate land far from the sea, which meets no rule but the last
biomes::Conditions Temperate()
{
	return {.altitude = 40.0f,
	        .temperature = 0.5f,
	        .moisture = 0.4f,
	        .slope = 2.0f,
	        .seaDistance = 50.0f,
	        .beachWidth = 6.0f,
	        .wet = false,
	        .blight = 0.0f};
}
} // namespace

TEST(LandgenClimate, TheSouthIsWarmAndTheMountainsAreCold)
{
	climate::Settings settings;
	settings.variation = 0.0f;
	const auto flat = MakeGrid<float>(33, [](int /*x*/, int /*z*/) { return 20.0f; });
	const auto temperature = climate::Temperature(flat, 1, settings);
	EXPECT_LT(temperature.At(16, 0), temperature.At(16, 32));
	const auto peak = MakeGrid<float>(33, [](int x, int /*z*/) { return x == 16 ? 250.0f : 20.0f; });
	const auto cooled = climate::Temperature(peak, 1, settings);
	EXPECT_LT(cooled.At(16, 16), cooled.At(10, 16));
}

TEST(LandgenClimate, DistanceToWaterCountsAlongTheAxesAndTheDiagonals)
{
	const auto water = MakeGrid<uint8_t>(9, [](int x, int z) { return static_cast<uint8_t>(x == 0 && z == 0 ? 1 : 0); });
	const auto distance = climate::DistanceToWater(water);
	EXPECT_EQ(distance.At(0, 0), 0.0f);
	EXPECT_FLOAT_EQ(distance.At(4, 0), 4.0f);
	EXPECT_NEAR(distance.At(3, 3), 3.0f * 1.41421356f, 1e-4f);
}

TEST(LandgenClimate, TheLandBehindTheMountainsIsDry)
{
	// The sea at the west edge, a ridge across the middle of x, and the wind from the west
	const auto heights = MakeGrid<float>(64, [](int x, int /*z*/) {
		if (x < 4)
		{
			return 0.0f;
		}
		return x >= 28 && x <= 34 ? 200.0f : 20.0f;
	});
	const auto water = MakeGrid<uint8_t>(64, [](int x, int /*z*/) { return static_cast<uint8_t>(x < 4 ? 1 : 0); });
	climate::Settings settings;
	settings.wind = climate::Wind::West;
	settings.variation = 0.0f;
	const auto moisture = climate::Moisture(heights, water, 1, settings);
	EXPECT_GT(moisture.At(26, 32), moisture.At(50, 32));
	// The same wind from the east puts the rain shadow on the other side
	settings.wind = climate::Wind::East;
	const auto eastWater = MakeGrid<uint8_t>(64, [](int x, int /*z*/) { return static_cast<uint8_t>(x > 59 ? 1 : 0); });
	const auto eastHeights = MakeGrid<float>(64, [&heights](int x, int z) { return heights.At(63 - x, z); });
	const auto eastMoisture = climate::Moisture(eastHeights, eastWater, 1, settings);
	EXPECT_GT(eastMoisture.At(37, 32), eastMoisture.At(13, 32));
}

TEST(LandgenBiomes, EachRuleGivesItsBiome)
{
	const biomes::Thresholds thresholds;
	EXPECT_EQ(biomes::Classify(Temperate(), thresholds), Biome::Grassland);

	auto c = Temperate();
	c.altitude = 2.0f;
	EXPECT_EQ(biomes::Classify(c, thresholds), Biome::Sea);
	c = Temperate();
	c.altitude = 200.0f;
	EXPECT_EQ(biomes::Classify(c, thresholds), Biome::Alpine);
	c = Temperate();
	c.temperature = 0.1f;
	EXPECT_EQ(biomes::Classify(c, thresholds), Biome::NordicWastes);
	c = Temperate();
	c.altitude = 8.0f;
	c.seaDistance = 3.0f;
	EXPECT_EQ(biomes::Classify(c, thresholds), Biome::Beach);
	c = Temperate();
	c.wet = true;
	EXPECT_EQ(biomes::Classify(c, thresholds), Biome::Swamp);
	c = Temperate();
	c.temperature = 0.9f;
	c.moisture = 0.7f;
	EXPECT_EQ(biomes::Classify(c, thresholds), Biome::DenseJungle);
	c = Temperate();
	c.temperature = 0.9f;
	c.moisture = 0.1f;
	EXPECT_EQ(biomes::Classify(c, thresholds), Biome::Desert);
	c = Temperate();
	c.blight = 0.8f;
	EXPECT_EQ(biomes::Classify(c, thresholds), Biome::DeadWoods);
	c = Temperate();
	c.moisture = 0.6f;
	EXPECT_EQ(biomes::Classify(c, thresholds), Biome::TemperateForest);
}

TEST(LandgenBiomes, SmoothRemovesASingleCornerButKeepsTheSeaAndTheMountains)
{
	Grid<Biome> grid(5, Biome::Grassland);
	grid.At(2, 2) = Biome::Desert;
	grid.At(0, 0) = Biome::Sea;
	grid.At(4, 4) = Biome::Alpine;
	const auto smooth = biomes::Smooth(grid, 1);
	EXPECT_EQ(smooth.At(2, 2), Biome::Grassland);
	EXPECT_EQ(smooth.At(0, 0), Biome::Sea);
	EXPECT_EQ(smooth.At(4, 4), Biome::Alpine);
}

TEST(LandgenCountries, TheBlendRampsAsTheLandsOfTheStoryDo)
{
	// A blend of 30 altitudes that starts at 116, as one country of Land 1 has: 9, 17, 26, 34 and so on, then 255
	const std::array<uint32_t, 5> first {9, 17, 26, 34, 43};
	for (size_t i = 0; i < first.size(); ++i)
	{
		EXPECT_EQ(countries::BlendCoefficient(116 + static_cast<int>(i), 116, 30), first.at(i));
	}
	EXPECT_EQ(countries::BlendCoefficient(145, 116, 30), 255u);
}

TEST(LandgenCountries, ACountryTakesEachBandAtItsAltitude)
{
	constexpr std::array<countries::Band, 2> k_Bands {{{0, Surface::Grass}, {100, Surface::Snow}}};
	const std::array materials {Surface::Snow, Surface::Grass};
	const auto country = countries::MakeCountry({.bands = k_Bands, .blend = 10}, materials);
	// Grass below the blend, snow from the band's altitude up
	EXPECT_EQ(country.materials.at(50).indices[0], 1u);
	EXPECT_EQ(country.materials.at(50).indices[1], 1u);
	EXPECT_EQ(country.materials.at(100).indices[0], 0u);
	EXPECT_EQ(country.materials.at(100).indices[1], 0u);
	EXPECT_EQ(country.materials.at(255).indices[0], 0u);
	// In the blend the snow is the first material
	const auto& blend = country.materials.at(95);
	EXPECT_EQ(blend.indices[0], 0u);
	EXPECT_EQ(blend.indices[1], 1u);
	EXPECT_EQ(blend.coefficient, countries::BlendCoefficient(95, 90, 10));
	EXPECT_EQ(country.materials.at(99).coefficient, 255u);
}

TEST(LandgenCountries, TheMountainsHaveSnowAtTheTop)
{
	const auto materials = countries::UsedSurfaces();
	const auto country = countries::MakeCountry(countries::RecipeOf(Biome::Alpine), materials);
	const auto top = materials.at(country.materials.at(250).indices[0]);
	EXPECT_TRUE(top == Surface::Snow || top == Surface::SnowyTops);
	EXPECT_EQ(materials.at(country.materials.at(140).indices[0]), Surface::Rock);
	// Each surface comes once, and each band of each biome finds its material
	EXPECT_EQ(std::set(materials.begin(), materials.end()).size(), materials.size());
	for (size_t biome = 0; biome < biomes::k_BiomeCount; ++biome)
	{
		for (const auto& band : countries::RecipeOf(static_cast<Biome>(biome)).bands)
		{
			EXPECT_NE(std::ranges::find(materials, band.surface), materials.end());
		}
	}
}

TEST(LandgenLighting, TheSunLightsTheSlopesThatFaceIt)
{
	const lighting::Settings settings;
	const auto flat = MakeGrid<uint8_t>(16, [](int /*x*/, int /*z*/) { return uint8_t {40}; });
	const auto flatLight = lighting::Luminosity(flat, settings);
	EXPECT_EQ(flatLight.At(8, 8), static_cast<uint8_t>(std::floor(settings.base + (settings.gain * lighting::k_Sun.y) + 0.5f)));
	// The sun is towards the low x: a slope that climbs to the high x faces it and is lit, one that falls to it is dark
	const auto rising = MakeGrid<uint8_t>(16, [](int x, int /*z*/) { return static_cast<uint8_t>(40 + (x * 6)); });
	const auto falling = MakeGrid<uint8_t>(16, [](int x, int /*z*/) { return static_cast<uint8_t>(130 - (x * 6)); });
	EXPECT_GT(lighting::Luminosity(rising, settings).At(8, 8), flatLight.At(8, 8));
	EXPECT_LT(lighting::Luminosity(falling, settings).At(8, 8), flatLight.At(8, 8));
}

TEST(LandgenLighting, AWallCastsAShadow)
{
	const lighting::Settings settings;
	// A high wall at x = 6 on flat land: the corners just past it, away from the sun, are in its shadow
	const auto walled = MakeGrid<uint8_t>(16, [](int x, int /*z*/) { return static_cast<uint8_t>(x == 6 ? 250 : 20); });
	const auto light = lighting::Luminosity(walled, settings);
	EXPECT_LT(light.At(8, 10), light.At(3, 10));
	EXPECT_GE(light.At(8, 10), settings.darkest);
}
