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
#include <numeric>

#include <gtest/gtest.h>
#include <landgen/HeightCurve.h>
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

TEST(LandgenHydrology, AShallowHighBasinFillsWithWetGround)
{
	// A deep, large basin keeps its hollow for a lake at its own level (see test_landgen_lakes); a shallow one fills
	auto heights = MakeGrid(24, [](int x, int z) {
		const float dx = static_cast<float>(x - 12);
		const float dz = static_cast<float>(z - 12);
		if ((dx * dx) + (dz * dz) < 16.0f)
		{
			return 58.5f;
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

TEST(LandgenTerrain, ThermalErosionDoesNotMoveFixedCorners)
{
	auto heights = MakeGrid(16, [](int x, int /*z*/) { return x < 8 ? 100.0f : 0.0f; });
	const auto before = heights;
	const std::vector<uint8_t> fixed(heights.values.size(), 1);
	terrain::ErodeThermal(heights, {.passes = 50, .plainTalus = 10.0f, .mountainTalus = 10.0f, .rate = 0.4f}, fixed);
	EXPECT_EQ(heights.values, before.values);
}

TEST(LandgenTerrain, TerraceLevelsRiseByTheSpacing)
{
	const terrain::TerraceSettings settings;
	const auto levels = terrain::TerraceLevels(5, settings);
	ASSERT_GE(levels.size(), 2u);
	EXPECT_GE(levels.front(), settings.firstLevelLow);
	EXPECT_LE(levels.front(), settings.firstLevelHigh);
	EXPECT_LE(levels.back(), settings.highestLevel);
	for (size_t i = 1; i < levels.size(); ++i)
	{
		EXPECT_GE(levels[i] - levels[i - 1], settings.spacingLow);
		EXPECT_LE(levels[i] - levels[i - 1], settings.spacingHigh);
	}
	EXPECT_EQ(levels, terrain::TerraceLevels(5, settings));
	EXPECT_NE(levels, terrain::TerraceLevels(6, settings));
}

TEST(LandgenTerrain, TerraceStepsLiftAClosedHollowToItsRim)
{
	const terrain::TerraceSettings settings {.edgeNoise = 0.0f, .calmNoise = 0.0f};
	const std::array levels {30.0f};
	// A plateau at 60 next to the sea at x = 0, with a hollow at 20 in the middle
	const auto closed = MakeGrid(20, [](int x, int z) {
		if (x == 0)
		{
			return 0.0f;
		}
		return x >= 7 && x <= 12 && z >= 7 && z <= 12 ? 20.0f : 60.0f;
	});
	const auto steps = terrain::TerraceSteps(closed, levels, 1, settings);
	EXPECT_EQ(steps.At(9, 9), 1);
	EXPECT_EQ(steps.At(15, 15), 1);
	EXPECT_EQ(steps.At(0, 9), 0);

	// The same hollow with a valley to the sea keeps its low step
	const auto open = MakeGrid(20, [](int x, int z) {
		if (x == 0)
		{
			return 0.0f;
		}
		return (x >= 7 && x <= 12 && z >= 7 && z <= 12) || (x < 7 && z >= 8 && z <= 10) ? 20.0f : 60.0f;
	});
	const auto openSteps = terrain::TerraceSteps(open, levels, 1, settings);
	EXPECT_EQ(openSteps.At(9, 9), 0);
	EXPECT_EQ(openSteps.At(3, 9), 0);
}

TEST(LandgenTerrain, TerracesAreFlatWithSharpCliffsBetweenThem)
{
	// A ramp up from the sea at x = 0, 3 altitude units for each corner
	auto heights = MakeGrid(40, [](int x, int /*z*/) { return static_cast<float>(x) * 3.0f; });
	const auto before = heights;
	const terrain::TerraceSettings settings {
	    .firstLevelLow = 30.0f,
	    .firstLevelHigh = 30.0f,
	    .spacingLow = 40.0f,
	    .spacingHigh = 40.0f,
	    .highestLevel = 200.0f,
	    .keepLow = 0.1f,
	    .keepHigh = 0.1f,
	    .edgeNoise = 0.0f,
	    .calmNoise = 0.0f,
	    .cliffRiseLow = 30.0f,
	    .cliffRiseHigh = 30.0f,
	};
	const auto cliffs = terrain::Terrace(heights, 3, settings);
	constexpr int k_Z = 20;
	// The sea and the land below the knee of the first edge do not change
	for (int x = 0; x <= 6; ++x)
	{
		EXPECT_EQ(heights.At(x, k_Z), before.At(x, k_Z)) << x;
	}
	// The land above the knee rises slower and stays below the first edge
	for (int x = 7; x <= 8; ++x)
	{
		EXPECT_LT(heights.At(x, k_Z), before.At(x, k_Z)) << x;
		EXPECT_LT(heights.At(x, k_Z), 30.0f) << x;
	}
	// The first terrace is nearly flat, from its edge at 30
	for (int x = 10; x <= 21; ++x)
	{
		EXPECT_GE(heights.At(x, k_Z), 30.0f) << x;
		EXPECT_LE(heights.At(x, k_Z), 34.0f) << x;
		EXPECT_EQ(cliffs.At(x, k_Z), 0) << x;
	}
	// The cliff to the next edge at 70 rises in two corners
	EXPECT_EQ(cliffs.At(23, k_Z), 1);
	EXPECT_NEAR(heights.At(23, k_Z), 55.0f, 1e-3f);
	EXPECT_NEAR(heights.At(24, k_Z), 70.2f, 1e-2f);
	// The ground never goes down from the sea inland
	for (int x = 1; x < 40; ++x)
	{
		EXPECT_GE(heights.At(x, k_Z), heights.At(x - 1, k_Z)) << x;
	}
	// The same seed makes the same terraces
	auto again = before;
	static_cast<void>(terrain::Terrace(again, 3, settings));
	EXPECT_EQ(heights.values, again.values);
}

TEST(LandgenTerrain, TerraceFillsTheHollowsOfItsFloor)
{
	// A plateau at 100 with a dip to 90, next to the sea at x = 0
	auto heights = MakeGrid(20, [](int x, int z) {
		if (x == 0)
		{
			return 0.0f;
		}
		return x >= 8 && x <= 11 && z >= 8 && z <= 11 ? 90.0f : 100.0f;
	});
	const terrain::TerraceSettings settings {
	    .firstLevelLow = 50.0f,
	    .firstLevelHigh = 50.0f,
	    .spacingLow = 1000.0f,
	    .spacingHigh = 1000.0f,
	    .highestLevel = 200.0f,
	    .keepLow = 0.5f,
	    .keepHigh = 0.5f,
	    .edgeNoise = 0.0f,
	    .calmNoise = 0.0f,
	};
	static_cast<void>(terrain::Terrace(heights, 1, settings));
	// The terrace keeps a part of the ground above its edge, and the dip fills to the height of the plateau
	constexpr float k_Span = terrain::k_HighestAltitude - 50.0f;
	constexpr float k_Above = 50.0f / k_Span;
	constexpr float k_Plateau = 50.0f + (k_Span * 0.5f * k_Above / (1.0f + (0.5f * k_Above)));
	for (int x = 1; x < 20; ++x)
	{
		for (int z = 0; z < 20; ++z)
		{
			EXPECT_NEAR(heights.At(x, z), k_Plateau, 1e-3f) << x << "," << z;
		}
	}
}

TEST(LandgenTerrain, ShoreShelfMakesAFlatStripAndABank)
{
	// Land at 100 from x = 2, a low strip of land at 4 along z = 10, and the sea at x = 0 and 1
	auto heights = MakeGrid(30, [](int x, int z) {
		if (x < 2)
		{
			return 0.0f;
		}
		return z == 10 ? 4.0f : 100.0f;
	});
	const terrain::ShelfSettings settings {
	    .widthLow = 3.0f,
	    .widthHigh = 3.0f,
	    .altitude = 3.6f,
	    .rise = 0.25f,
	    .bankLow = 10.0f,
	    .bankHigh = 10.0f,
	};
	terrain::ShoreShelf(heights, 1, settings);
	constexpr int k_Z = 20;
	EXPECT_EQ(heights.At(0, k_Z), 0.0f);
	// The strip is flat for three corners from the water, then the bank rises 10 for each corner
	EXPECT_NEAR(heights.At(2, k_Z), 3.85f, 1e-4f);
	EXPECT_NEAR(heights.At(4, k_Z), 4.35f, 1e-4f);
	EXPECT_NEAR(heights.At(5, k_Z), 14.35f, 1e-4f);
	EXPECT_NEAR(heights.At(10, k_Z), 64.35f, 1e-4f);
	EXPECT_EQ(heights.At(20, k_Z), 100.0f);
	// Ground lower than the bank does not change
	EXPECT_EQ(heights.At(10, 10), 4.0f);
}

namespace
{
/// An island: a dome of land with the sea round it
Grid<float> Island(int side)
{
	const float middle = static_cast<float>(side - 1) * 0.5f;
	return MakeGrid(side, [middle](int x, int z) {
		const float dx = static_cast<float>(x) - middle;
		const float dz = static_cast<float>(z) - middle;
		return 40.0f - (std::sqrt((dx * dx) + (dz * dz)) * 40.0f / middle);
	});
}

/// The number of land corners that are lower than each of their eight neighbours
int CountPits(const Grid<float>& heights)
{
	int pits = 0;
	for (int x = 1; x < heights.side - 1; ++x)
	{
		for (int z = 1; z < heights.side - 1; ++z)
		{
			const float height = heights.At(x, z);
			if (height > terrain::k_SeaLevel && std::ranges::all_of(k_Neighbours, [&](const Step& step) {
				    return heights.At(x + step.dx, z + step.dz) > height;
			    }))
			{
				++pits;
			}
		}
	}
	return pits;
}
} // namespace

TEST(LandgenTerrain, MassifsStandInlandAndApart)
{
	const auto heights = Island(129);
	const terrain::MassifSettings settings {.countLow = 2,
	                                        .countHigh = 2,
	                                        .radiusLow = 0.1f,
	                                        .radiusHigh = 0.1f,
	                                        .sidePeaksLow = 0,
	                                        .sidePeaksHigh = 0,
	                                        .seaClearance = 1.0f};
	const auto peaks = terrain::PlaceMassifs(heights, 5, settings);
	ASSERT_EQ(peaks.size(), 2U);
	const float radius = 0.1f * 128.0f;
	for (const auto& peak : peaks)
	{
		EXPECT_NEAR(peak.radius, radius, 1e-3f);
		EXPECT_GE(peak.summit, settings.summitLow);
		EXPECT_LE(peak.summit, settings.summitHigh);
		// The sea is 64 corners from the middle
		const float fromMiddle = std::hypot(peak.x - 64.0f, peak.z - 64.0f);
		EXPECT_LE(fromMiddle, 64.0f - radius);
	}
	EXPECT_GE(std::hypot(peaks[0].x - peaks[1].x, peaks[0].z - peaks[1].z), 2.0f * radius * 0.9f);
	// The same seed gives the same peaks
	const auto again = terrain::PlaceMassifs(heights, 5, settings);
	ASSERT_EQ(again.size(), peaks.size());
	EXPECT_EQ(again[0].x, peaks[0].x);
	EXPECT_EQ(again[1].summit, peaks[1].summit);
}

TEST(LandgenTerrain, AMassifHasLowerPeaksNextToItsMainPeak)
{
	const auto heights = Island(129);
	const terrain::MassifSettings settings {.countLow = 1, .countHigh = 1, .sidePeaksLow = 3, .sidePeaksHigh = 3};
	const auto peaks = terrain::PlaceMassifs(heights, 9, settings);
	ASSERT_EQ(peaks.size(), 4U);
	for (size_t i = 1; i < peaks.size(); ++i)
	{
		EXPECT_LT(peaks[i].radius, peaks[0].radius);
		EXPECT_LE(std::hypot(peaks[i].x - peaks[0].x, peaks[i].z - peaks[0].z),
		          (settings.sideDistanceHigh * peaks[0].radius) + 1e-3f);
	}
}

TEST(LandgenTerrain, AMassifIsHighestAtItsSummitAndEndsAtItsFoot)
{
	const std::array peaks {terrain::Peak {.x = 32.0f, .z = 32.0f, .radius = 20.0f, .summit = 200.0f}};
	terrain::MassifSettings settings;
	settings.outline = 0.0f;
	settings.bandHigh = 0.0f;
	const auto massifs = terrain::MassifHeights(65, peaks, 3, settings);
	EXPECT_NEAR(massifs.At(32, 32), 200.0f, 1e-3f);
	EXPECT_EQ(massifs.At(32, 53), 0.0f);
	EXPECT_EQ(massifs.At(0, 0), 0.0f);
	// The cone is steeper near the summit than near the foot
	EXPECT_GT(massifs.At(32, 32) - massifs.At(32, 34), massifs.At(32, 48) - massifs.At(32, 50));
	EXPECT_LE(*std::ranges::max_element(massifs.values), 200.0f);
}

TEST(LandgenTerrain, MassifBandsHaveSteepCliffsAndGentleSlopes)
{
	const std::array peaks {terrain::Peak {.x = 64.0f, .z = 64.0f, .radius = 60.0f, .summit = 240.0f}};
	terrain::MassifSettings settings;
	settings.outline = 0.0f;
	settings.gullyDepth = 0.0f;
	settings.bandNoise = 0.0f;
	settings.bandLow = 60.0f;
	settings.bandHigh = 60.0f;
	settings.faceWidth = 0.1f;
	settings.faceRise = 0.6f;
	const auto massifs = terrain::MassifHeights(129, peaks, 3, settings);
	// Along a line from the summit, a few steps are cliffs and most are gentle
	int cliffs = 0;
	int gentle = 0;
	for (int z = 70; z < 110; ++z)
	{
		const float drop = massifs.At(64, z) - massifs.At(64, z + 1);
		EXPECT_GE(drop, -1e-3f);
		cliffs += drop > 15.0f ? 1 : 0;
		gentle += drop < 5.0f ? 1 : 0;
	}
	EXPECT_GE(cliffs, 2);
	EXPECT_GT(gentle, cliffs * 3);
}

TEST(LandgenTerrain, RaiseMassifsTakesTheHigherGroundAndLeavesNoPits)
{
	auto heights = Island(129);
	Grid<uint8_t> cliffs(129, 1);
	const terrain::MassifSettings settings {.countLow = 3, .countHigh = 3};
	const auto peaks = terrain::PlaceMassifs(heights, 4, settings);
	const auto massifs = terrain::MassifHeights(129, peaks, 4, settings);
	const auto before = heights;
	terrain::RaiseMassifs(heights, massifs, cliffs);
	for (size_t i = 0; i < heights.values.size(); ++i)
	{
		EXPECT_GE(heights.values[i],
		          std::max(before.values[i], before.values[i] > terrain::k_SeaLevel ? massifs.values[i] : before.values[i]) -
		              1e-3f);
		if (massifs.values[i] > before.values[i] && before.values[i] > terrain::k_SeaLevel)
		{
			EXPECT_EQ(cliffs.values[i], 0);
		}
	}
	EXPECT_EQ(CountPits(heights), 0);
}

TEST(LandgenTerrain, LowerHighGroundStaysBelowTheCeiling)
{
	auto heights = MakeGrid(4, [](int x, int z) { return static_cast<float>((x * 4) + z) * 20.0f; });
	const auto before = heights;
	terrain::LowerHighGround(heights, 60.0f, 90.0f);
	for (size_t i = 0; i < heights.values.size(); ++i)
	{
		if (before.values[i] <= 60.0f)
		{
			EXPECT_EQ(heights.values[i], before.values[i]);
		}
		else
		{
			EXPECT_LT(heights.values[i], 90.0f);
			EXPECT_GT(heights.values[i], 60.0f);
		}
		if (i > 0)
		{
			EXPECT_GT(heights.values[i], heights.values[i - 1]);
		}
	}
}

TEST(LandgenTerrain, TheHighestEdgeLimitsTheSteps)
{
	const auto heights = MakeGrid(20, [](int, int) { return 150.0f; });
	const std::array levels {30.0f, 80.0f, 130.0f};
	const terrain::TerraceSettings settings {.edgeNoise = 0.0f, .calmNoise = 0.0f};
	const auto free = terrain::TerraceSteps(heights, levels, 1, settings);
	EXPECT_EQ(free.At(10, 10), 3);
	const std::vector<float> highest(heights.values.size(), 90.0f);
	const auto limited = terrain::TerraceSteps(heights, levels, 1, settings, highest);
	EXPECT_EQ(limited.At(10, 10), 2);
}

TEST(LandgenTerrain, RollGroundMovesTheInlandGroundAndKeepsTheShore)
{
	// An island: sea round the edge, a strip of shore, and a plateau inland
	constexpr int k_Side = 96;
	auto heights = MakeGrid(k_Side, [](int x, int z) {
		const int edge = std::min({x, z, k_Side - 1 - x, k_Side - 1 - z});
		return edge < 6 ? -5.0f : (edge < 10 ? 5.0f : 40.0f);
	});
	const auto before = heights;
	terrain::RollSettings settings;
	settings.calmAmount = 20.0f;
	settings.roughAmount = 20.0f;
	settings.seaStart = 6.0f;
	settings.seaFull = 12.0f;
	settings.highStart = 200.0f;
	settings.highEnd = 220.0f;
	settings.highFall = 1.0f;
	settings.stepRise = 0.0f;
	settings.frequency = 50.0f;
	settings.ridge = 0.5f;
	settings.calmShare = 0.3f;
	terrain::RollGround(heights, 3, settings);
	int moved = 0;
	for (int x = 0; x < k_Side; ++x)
	{
		for (int z = 0; z < k_Side; ++z)
		{
			const int edge = std::min({x, z, k_Side - 1 - x, k_Side - 1 - z});
			if (edge < 8)
			{
				// The sea and the shore near the water do not change
				EXPECT_EQ(heights.At(x, z), before.At(x, z));
			}
			else if (heights.At(x, z) != before.At(x, z))
			{
				++moved;
			}
		}
	}
	EXPECT_GT(moved, (k_Side - 30) * (k_Side - 30) / 2);
	// No pit: the flood that fills the pits does not lift the ground by more than its small rise on flat ground
	const auto drainage = hydrology::Drain(heights, terrain::k_SeaLevel);
	for (size_t i = 0; i < heights.values.size(); ++i)
	{
		EXPECT_LT(drainage.filled.values[i] - heights.values[i], 0.05f);
	}
}

TEST(LandgenTerrain, RollGroundStepsMakeFacesSteeperThanTheBenches)
{
	constexpr int k_Side = 64;
	auto heights = MakeGrid(k_Side, [](int x, int /*z*/) { return x < 4 ? -5.0f : static_cast<float>(x) * 2.0f; });
	terrain::RollSettings settings;
	settings.calmAmount = 0.0f;
	settings.roughAmount = 0.0f;
	settings.seaStart = 0.0f;
	settings.seaFull = 0.5f;
	settings.highStart = 500.0f;
	settings.highEnd = 600.0f;
	settings.stepRise = 20.0f;
	settings.stepFace = 0.25f;
	settings.stepFaceRise = 0.8f;
	terrain::RollGround(heights, 5, settings);
	// The ramp rises 2 units for each corner. A step of 20 units is 10 corners wide: its face rises 16 units in 2.5
	// corners, and its bench rises 4 units in 7.5 corners.
	float steepest = 0.0f;
	float gentlest = 100.0f;
	for (int x = 10; x < k_Side - 2; ++x)
	{
		const float rise = heights.At(x + 1, 30) - heights.At(x, 30);
		EXPECT_GE(rise, 0.0f);
		steepest = std::max(steepest, rise);
		gentlest = std::min(gentlest, rise);
	}
	EXPECT_GT(steepest, 5.0f);
	EXPECT_LT(gentlest, 1.0f);
}

TEST(LandgenHeightCurve, TheLinearCurveGivesTheHeightsOfTheStory)
{
	const HeightCurve curve;
	EXPECT_TRUE(curve.IsLinear());
	for (int altitude = 0; altitude <= 255; ++altitude)
	{
		const auto a = static_cast<float>(altitude);
		EXPECT_EQ(curve.MetresOf(a), a * 0.67f);
		EXPECT_EQ(curve.UnitsOf(a), a);
		EXPECT_EQ(curve.WholeUnitsOf(a), altitude);
		EXPECT_EQ(curve.AltitudeOf(a), a);
	}
	EXPECT_EQ(HeightCurve::WithTop(60.0f, 150.0f), HeightCurve {});
	EXPECT_EQ(HeightCurve::WithTop(255.0f, 400.0f), HeightCurve {});
}

TEST(LandgenHeightCurve, AHigherCurveKeepsTheLowLandAndReachesTheTop)
{
	const auto curve = HeightCurve::WithTop(60.0f, 1100.0f);
	EXPECT_FALSE(curve.IsLinear());
	for (int altitude = 0; altitude <= 60; ++altitude)
	{
		const auto a = static_cast<float>(altitude);
		EXPECT_EQ(curve.MetresOf(a), a * 0.67f);
		EXPECT_EQ(curve.WholeUnitsOf(a), altitude);
	}
	EXPECT_NEAR(curve.HighestMetres(), 1100.0f, 0.1f);
	float before = -1.0f;
	float step = 0.0f;
	for (int altitude = 0; altitude <= 255; ++altitude)
	{
		const float metres = curve.MetresOf(static_cast<float>(altitude));
		// Each altitude is higher than the one below, by a step that does not get smaller
		EXPECT_GT(metres, before);
		if (altitude > 0)
		{
			EXPECT_GE(metres - before, step - 1e-3f);
			step = metres - before;
		}
		before = metres;
		EXPECT_NEAR(curve.AltitudeOf(curve.UnitsOf(static_cast<float>(altitude))), static_cast<float>(altitude), 1e-3f);
	}
}

TEST(LandgenHeightCurve, TheTopStopsAtTheMostUnits)
{
	const auto curve = HeightCurve::WithTop(60.0f, 10000.0f);
	EXPECT_NEAR(curve.UnitsOf(255.0f), k_MostUnits, 0.5f);
	EXPECT_EQ(curve.WholeUnitsOf(255.0f), static_cast<uint16_t>(k_MostUnits));
}
