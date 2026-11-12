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
#include <numeric>

#include <gtest/gtest.h>
#include <landgen/Hydrology.h>
#include <landgen/Noise.h>
#include <landgen/Random.h>
#include <landgen/Terrain.h>

using namespace openblack::landgen;

namespace
{
/// A grid of the side with each height from the function of x and z
template <typename Function>
Grid<float> MakeGrid(int side, Function height)
{
	Grid<float> grid(side, 0.0f);
	for (int x = 0; x < side; ++x)
	{
		for (int z = 0; z < side; ++z)
		{
			grid.At(x, z) = height(x, z);
		}
	}
	return grid;
}

float Sum(const Grid<float>& grid)
{
	return std::accumulate(grid.values.begin(), grid.values.end(), 0.0f);
}
} // namespace

TEST(LandgenRandom, SameSeedGivesSameNumbers)
{
	Random first(42);
	Random second(42);
	Random other(43);
	bool differs = false;
	for (int i = 0; i < 100; ++i)
	{
		const auto a = first.Next();
		ASSERT_EQ(a, second.Next());
		differs = differs || a != other.Next();
	}
	EXPECT_TRUE(differs);
}

TEST(LandgenRandom, NumbersStayInTheirRange)
{
	Random random(7);
	for (int i = 0; i < 10000; ++i)
	{
		const float unit = random.NextUnit();
		ASSERT_GE(unit, 0.0f);
		ASSERT_LT(unit, 1.0f);
		ASSERT_LT(random.NextBelow(5), 5u);
		const float range = random.NextRange(-2.0f, 3.0f);
		ASSERT_GE(range, -2.0f);
		ASSERT_LT(range, 3.0f);
	}
}

TEST(LandgenRandom, HashDependsOnEachInput)
{
	EXPECT_EQ(Hash(1, 2, 3), Hash(1, 2, 3));
	EXPECT_NE(Hash(1, 2, 3), Hash(1, 3, 2));
	EXPECT_NE(Hash(1, 2, 3), Hash(2, 2, 3));
}

TEST(LandgenNoise, GradientIsZeroOnTheLatticeAndBounded)
{
	EXPECT_EQ(noise::Gradient(5, 3.0f, -7.0f), 0.0f);
	for (int i = 0; i < 2000; ++i)
	{
		const float x = static_cast<float>(i) * 0.137f;
		const float z = static_cast<float>(i) * 0.291f;
		const float value = noise::Gradient(5, x, z);
		ASSERT_LE(std::fabs(value), 1.01f);
		const float ridged = noise::Ridged(5, x, z, 4);
		ASSERT_GE(ridged, 0.0f);
		ASSERT_LE(ridged, 1.0f);
		ASSERT_LE(std::fabs(noise::Fractal(5, x, z, 4)), 1.01f);
	}
}

TEST(LandgenNoise, SmoothStepIsFlatOutsideItsEdges)
{
	EXPECT_EQ(noise::SmoothStep(1.0f, 2.0f, 0.0f), 0.0f);
	EXPECT_EQ(noise::SmoothStep(1.0f, 2.0f, 3.0f), 1.0f);
	EXPECT_FLOAT_EQ(noise::SmoothStep(1.0f, 2.0f, 1.5f), 0.5f);
	// Reversed edges give the reversed step
	EXPECT_EQ(noise::SmoothStep(2.0f, 1.0f, 3.0f), 0.0f);
}

TEST(LandgenTerrain, BaseHeightsMakeAnIslandInTheSea)
{
	terrain::BaseSettings settings;
	settings.seaMargin = 8;
	settings.hollows = 0;
	const auto heights = terrain::BaseHeights(65, 3, settings);
	for (int i = 0; i < heights.side; ++i)
	{
		EXPECT_LT(heights.At(i, 0), 0.0f);
		EXPECT_LT(heights.At(0, i), 0.0f);
		EXPECT_LT(heights.At(i, heights.side - 1), 0.0f);
		EXPECT_LT(heights.At(heights.side - 1, i), 0.0f);
	}
	EXPECT_GT(heights.At(32, 32), terrain::k_SeaLevel);
	EXPECT_EQ(heights.values, terrain::BaseHeights(65, 3, settings).values);
	EXPECT_NE(heights.values, terrain::BaseHeights(65, 4, settings).values);
}

TEST(LandgenTerrain, HollowsGoBelowTheSea)
{
	terrain::BaseSettings settings;
	settings.seaMargin = 8;
	settings.mountainHeight = 0.0f;
	settings.hillHeight = 0.0f;
	settings.plainHeight = 10.0f;
	settings.hollowGround = 255.0f;
	settings.hollows = 1;
	settings.hollowRadius = 6.0f;
	const auto heights = terrain::BaseHeights(65, 9, settings);
	settings.hollows = 0;
	const auto without = terrain::BaseHeights(65, 9, settings);
	// The hollow lowers the land in one place only, down to below the sea
	int lowered = 0;
	bool belowSea = false;
	for (size_t i = 0; i < heights.values.size(); ++i)
	{
		EXPECT_LE(heights.values[i], without.values[i]);
		if (heights.values[i] < without.values[i])
		{
			++lowered;
			belowSea = belowSea || (without.values[i] > terrain::k_SeaLevel && heights.values[i] < 0.0f);
		}
	}
	EXPECT_GT(lowered, 0);
	EXPECT_LT(lowered, 400);
	EXPECT_TRUE(belowSea);
}

TEST(LandgenTerrain, HydraulicErosionCutsASlope)
{
	// A slope from 60 down to the sea along x
	auto heights = MakeGrid(48, [](int x, int /*z*/) { return 60.0f - (static_cast<float>(x) * 1.5f); });
	const auto before = heights;
	terrain::HydraulicSettings settings;
	settings.drops = 3000;
	terrain::ErodeHydraulic(heights, 11, settings);
	EXPECT_NE(heights.values, before.values);
	EXPECT_LT(Sum(heights), Sum(before));
	for (const auto height : heights.values)
	{
		ASSERT_TRUE(std::isfinite(height));
	}
	// The same seed wears the land the same way
	auto again = before;
	terrain::ErodeHydraulic(again, 11, settings);
	EXPECT_EQ(heights.values, again.values);
}

TEST(LandgenTerrain, ThermalErosionLowersSteepSlopesAndKeepsTheGround)
{
	// A cliff of 100 between two flat steps
	auto heights = MakeGrid(16, [](int x, int /*z*/) { return x < 8 ? 100.0f : 0.0f; });
	const float before = Sum(heights);
	terrain::ErodeThermal(heights, {.passes = 2000, .plainTalus = 10.0f, .mountainTalus = 10.0f, .rate = 0.4f});
	EXPECT_NEAR(Sum(heights), before, 0.01f * before);
	const auto slopes = terrain::Slopes(heights);
	EXPECT_LT(*std::ranges::max_element(slopes.values), 12.0f);
}

TEST(LandgenTerrain, SlopesAreTheLargestStepAlongAnAxis)
{
	const auto heights = MakeGrid(3, [](int x, int z) { return static_cast<float>((x * 2) + (z * 5)); });
	const auto slopes = terrain::Slopes(heights);
	EXPECT_EQ(slopes.At(1, 1), 5.0f);
}

TEST(LandgenHydrology, DrainFillsAPitToItsSpill)
{
	// A bowl in a plateau at 20, open to the sea at the low x edge through a sill at 12
	auto heights = MakeGrid(16, [](int x, int z) {
		if (x == 0)
		{
			return 0.0f;
		}
		const float dx = static_cast<float>(x - 8);
		const float dz = static_cast<float>(z - 8);
		if ((dx * dx) + (dz * dz) < 9.0f)
		{
			return 5.0f;
		}
		return x < 4 ? 12.0f : 20.0f;
	});
	const auto drainage = hydrology::Drain(heights, terrain::k_SeaLevel);
	// The bowl fills to the height of the plateau round it
	EXPECT_GE(drainage.filled.At(8, 8), 20.0f);
	EXPECT_LT(drainage.filled.At(8, 8), 20.1f);
	// The sea stays as it is and has no way down
	EXPECT_EQ(drainage.filled.At(0, 5), 0.0f);
	EXPECT_EQ(drainage.receivers[heights.Index(0, 5)], -1);
	// Each corner of the land reaches the sea or the edge of the map
	for (size_t start = 0; start < heights.values.size(); ++start)
	{
		auto index = static_cast<int32_t>(start);
		int steps = 0;
		while (drainage.receivers[static_cast<size_t>(index)] >= 0 && steps < 1000)
		{
			index = drainage.receivers[static_cast<size_t>(index)];
			++steps;
		}
		const int x = heights.XOf(static_cast<size_t>(index));
		const int z = heights.ZOf(static_cast<size_t>(index));
		const bool edge = x == 0 || z == 0 || x == heights.side - 1 || z == heights.side - 1;
		ASSERT_TRUE(edge || drainage.filled.values[static_cast<size_t>(index)] <= terrain::k_SeaLevel);
	}
}

TEST(LandgenHydrology, AccumulateAddsTheRainOfTheCornersAbove)
{
	// A slope along x down to the sea at x = 0: each row carries the rain of the corners above it
	const auto heights = MakeGrid(8, [](int x, int /*z*/) { return static_cast<float>(x) * 2.0f; });
	const auto drainage = hydrology::Drain(heights, terrain::k_SeaLevel);
	const auto flow = hydrology::Accumulate(drainage, Grid<float>(8, 1.0f));
	EXPECT_FLOAT_EQ(Sum(flow), [&] {
		// Each corner counts its own rain and the rain of each corner that flows through it
		float total = 0.0f;
		for (size_t i = 0; i < heights.values.size(); ++i)
		{
			auto index = static_cast<int32_t>(i);
			total += 1.0f;
			while (drainage.receivers[static_cast<size_t>(index)] >= 0)
			{
				index = drainage.receivers[static_cast<size_t>(index)];
				total += 1.0f;
			}
		}
		return total;
	}());
	EXPECT_GT(flow.At(2, 4), flow.At(6, 4));
}

TEST(LandgenHydrology, ALowBasinBecomesALakeAtTheSeaLevel)
{
	auto heights = MakeGrid(24, [](int x, int z) {
		const float dx = static_cast<float>(x - 12);
		const float dz = static_cast<float>(z - 12);
		if ((dx * dx) + (dz * dz) < 16.0f)
		{
			return -2.0f;
		}
		return std::min(static_cast<float>(x) * 1.5f, 8.0f);
	});
	const auto drainage = hydrology::Drain(heights, terrain::k_SeaLevel);
	const auto lakes = hydrology::FormLakes(heights, drainage, {});
	ASSERT_EQ(lakes.seaLakes.size(), 1u);
	EXPECT_LE(heights.At(12, 12), 0.0f);
	// A channel joins the lake to the sea: the drainage of the new land reaches the sea from the lake at the sea level
	const auto after = hydrology::Drain(heights, terrain::k_SeaLevel);
	EXPECT_EQ(after.filled.At(12, 12), heights.At(12, 12));
	EXPECT_EQ(std::ranges::count(lakes.wet.values, uint8_t {1}), 0);
}

TEST(LandgenHydrology, AHighBasinFillsWithWetGround)
{
	auto heights = MakeGrid(24, [](int x, int z) {
		const float dx = static_cast<float>(x - 12);
		const float dz = static_cast<float>(z - 12);
		if ((dx * dx) + (dz * dz) < 16.0f)
		{
			return 40.0f;
		}
		return x == 0 ? 0.0f : 60.0f;
	});
	const auto drainage = hydrology::Drain(heights, terrain::k_SeaLevel);
	const auto lakes = hydrology::FormLakes(heights, drainage, {});
	EXPECT_TRUE(lakes.seaLakes.empty());
	EXPECT_GE(heights.At(12, 12), 60.0f);
	EXPECT_EQ(lakes.wet.At(12, 12), 1);
	EXPECT_EQ(lakes.wet.At(2, 2), 0);
}

TEST(LandgenHydrology, RiversRunFromTheirSourceToTheSeaAndCutTheirBed)
{
	// A wide slope that narrows into a valley in the middle of z, down to the sea at x = 0
	auto heights = MakeGrid(40, [](int x, int z) {
		return x == 0 ? 0.0f : (static_cast<float>(x) * 1.0f) + (std::fabs(static_cast<float>(z - 20)) * 0.6f) + 4.0f;
	});
	const auto drainage = hydrology::Drain(heights, terrain::k_SeaLevel);
	const auto flow = hydrology::Accumulate(drainage, Grid<float>(40, 1.0f));
	hydrology::RiverSettings settings;
	settings.sourceFlow = 60.0f;
	settings.minCorners = 4;
	const auto rivers = hydrology::TraceRivers(drainage, flow, settings);
	ASSERT_FALSE(rivers.empty());
	const auto& main = *std::ranges::max_element(rivers, {}, [](const auto& river) { return river.corners.size(); });
	EXPECT_LE(heights.values[main.corners.back()], terrain::k_SeaLevel);
	EXPECT_GE(main.points.size(), 2u);
	EXPECT_EQ(main.points.front().x, static_cast<float>(heights.XOf(main.corners.front())));

	const auto before = heights;
	hydrology::CarveRivers(heights, rivers, flow, settings);
	float previous = heights.values[main.corners.front()];
	for (const auto corner : main.corners)
	{
		// The bed is lower than the land was, and never rises downstream
		EXPECT_LE(heights.values[corner], before.values[corner]);
		EXPECT_LE(heights.values[corner], previous + 1e-4f);
		previous = heights.values[corner];
	}
}
