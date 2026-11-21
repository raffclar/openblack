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
#include <vector>

#include <gtest/gtest.h>
#include <landgen/Generator.h>
#include <landgen/Streams.h>

#include "3D/GeneratedLand.h"
#include "Common/IntegerSteps.h"

using namespace openblack::landgen;
using biomes::Biome;
using features::Tree;
using hydrology::River;

namespace
{
constexpr int k_Side = 100;

/// A fake map of 1 km a side: the sea in the columns below x 10, land on the rest
/// Flat ground of altitude 30, about 20 m high
Grid<float> Flat()
{
	return {k_Side, 30.0f};
}

/// The settings of the streams with no springs below 18 m, so that the streams of the flat ground start on the rivers
constexpr streams::Settings k_RiversOnly {.springHighest = 18.0f};

Grid<uint8_t> SeaOnOneSide()
{
	Grid<uint8_t> water(k_Side, 0);
	for (int x = 0; x < 10; ++x)
	{
		for (int z = 0; z < k_Side; ++z)
		{
			water.At(x, z) = 1;
		}
	}
	return water;
}

/// The water of each corner flows to the corner next to it towards the sea, which is below x 10
std::vector<int32_t> TowardsTheSea()
{
	std::vector<int32_t> receivers(static_cast<size_t>(k_Side) * k_Side, -1);
	for (int x = 10; x < k_Side; ++x)
	{
		for (int z = 0; z < k_Side; ++z)
		{
			receivers[openblack::ToIndex((x * k_Side) + z)] = ((x - 1) * k_Side) + z;
		}
	}
	return receivers;
}

/// A river along the line z of the map, from x `from` down to x `to`
River Line(int z, int from, int to)
{
	River river;
	for (int x = from; x >= to; --x)
	{
		river.corners.push_back(static_cast<uint32_t>((x * k_Side) + z));
	}
	return river;
}

struct GreenLand
{
	Grid<Biome> biomes {k_Side, Biome::Grassland};
	Grid<float> heights {k_Side, 20.0f};
	Grid<float> sea {k_Side, 40.0f};
	Grid<uint8_t> keepClear {k_Side, 0};

	[[nodiscard]] features::Ground Ground() const
	{
		return {.biomes = biomes,
		        .heights = heights,
		        .seaDistance = sea,
		        .keepClear = keepClear,
		        .clearings = {},
		        .townX = 0.0f,
		        .townZ = 0.0f};
	}
};
} // namespace

TEST(LandgenStreams, EachStreamThatStaysReachesTheWaterOrAnotherStream)
{
	const auto water = SeaOnOneSide();
	const auto receivers = TowardsTheSea();
	// Rivers to the sea, and rivers higher up the same lines that need a stream below them
	std::vector<River> rivers;
	for (int z = 10; z < 90; z += 10)
	{
		rivers.push_back(Line(z, 40, 9));
		rivers.push_back(Line(z, 95, 41));
	}
	for (uint64_t seed = 1; seed <= 20; ++seed)
	{
		const auto streams = streams::Choose(rivers, receivers, Flat(), water, 90 * 100, seed,
		                                     {.fewestPerKm2 = 30.0f, .mostPerKm2 = 30.0f, .springHighest = 18.0f});
		ASSERT_FALSE(streams.empty());
		for (const auto& stream : streams)
		{
			EXPECT_TRUE(streams::Reaches(stream, streams, water));
		}
	}
}

TEST(LandgenStreams, TheSpringsAreOnTheHills)
{
	const auto water = SeaOnOneSide();
	const auto receivers = TowardsTheSea();
	// The land rises 1 altitude each corner from the sea, so a spring at 12 m to 100 m is 18 to 149 corners in
	Grid<float> slope(k_Side, 0.0f);
	for (int x = 0; x < k_Side; ++x)
	{
		for (int z = 0; z < k_Side; ++z)
		{
			slope.At(x, z) = static_cast<float>(std::max(x - 9, 0)) * 3.0f;
		}
	}
	const auto streams =
	    streams::Choose({}, receivers, slope, water, 90 * 100, 4, {.fewestPerKm2 = 30.0f, .mostPerKm2 = 30.0f});
	ASSERT_FALSE(streams.empty());
	for (const auto& stream : streams)
	{
		const float source = slope.values[stream.corners.front()] * 0.67f;
		EXPECT_GE(source, 12.0f);
		EXPECT_LE(source, 100.0f);
		EXPECT_TRUE(streams::Reaches(stream, streams, water));
	}
}

TEST(LandgenStreams, TheSeedPicksTheStreamsForTheArea)
{
	const auto water = SeaOnOneSide();
	const auto receivers = TowardsTheSea();
	std::vector<River> rivers;
	for (int z = 2; z < 98; z += 6)
	{
		rivers.push_back(Line(z, 40, 9));
	}
	// 0.9 square kilometres of land with 1.2 to 7.3 streams for each square kilometre, and at least 2
	for (uint64_t seed = 1; seed <= 50; ++seed)
	{
		const auto count = streams::Choose(rivers, receivers, Flat(), water, 90 * 100, seed, k_RiversOnly).size();
		EXPECT_GE(count, 2u);
		EXPECT_LE(count, 7u);
	}
	// 9 square kilometres of land: more streams than the 16 rivers, which keep 6 corners apart
	const auto many = k_RiversOnly;
	for (uint64_t seed = 1; seed <= 50; ++seed)
	{
		const auto count = streams::Choose(rivers, receivers, Flat(), water, 90 * 1000, seed, many).size();
		EXPECT_GE(count, 10u);
		EXPECT_LE(count, 16u);
	}
}

TEST(LandgenStreams, ALongRiverStartsLowerDown)
{
	const auto water = SeaOnOneSide();
	const auto receivers = TowardsTheSea();
	const std::vector<River> rivers {Line(50, 95, 9)};
	size_t shortest = rivers[0].corners.size();
	for (uint64_t seed = 1; seed <= 30; ++seed)
	{
		const auto streams = streams::Choose(rivers, receivers, Flat(), water, 90 * 100, seed, k_RiversOnly);
		ASSERT_EQ(streams.size(), 1u);
		// The longest stream of the story is 639 m long: 54 corners of 12 m
		EXPECT_LE(streams[0].corners.size(), 54u);
		EXPECT_EQ(streams[0].corners.back(), rivers[0].corners.back());
		shortest = std::min(shortest, streams[0].corners.size());
	}
	EXPECT_LT(shortest, 30u);
}

TEST(LandgenStreams, ThePointsAreOneCornerApartAndTheLastPointIsOnItsCorner)
{
	const auto water = SeaOnOneSide();
	const auto receivers = TowardsTheSea();
	const std::vector<River> rivers {Line(50, 40, 9)};
	const auto streams = streams::Choose(rivers, receivers, Flat(), water, 90 * 100, 3, k_RiversOnly);
	ASSERT_EQ(streams.size(), 1u);
	const auto& points = streams[0].points;
	ASSERT_EQ(points.size(), streams[0].corners.size());
	for (size_t i = 0; i + 1 < points.size(); ++i)
	{
		const float step = std::hypot(points[i + 1].x - points[i].x, points[i + 1].z - points[i].z);
		EXPECT_GE(step, 0.5f);
		EXPECT_LE(step, 1.5f);
	}
	EXPECT_EQ(points.back().x, 9.0f);
	EXPECT_EQ(points.back().z, 50.0f);
}

TEST(LandgenStreams, AStreamThatEndsOnDryGroundDoesNotReach)
{
	const auto water = SeaOnOneSide();
	const std::vector<River> rivers {Line(50, 60, 40), Line(60, 40, 9)};
	EXPECT_FALSE(streams::Reaches(rivers[0], rivers, water));
	EXPECT_TRUE(streams::Reaches(rivers[1], rivers, water));
}

TEST(LandgenStreams, TheWarmBanksHaveCedarFirst)
{
	const auto warm = streams::BankTreesOf(Biome::DenseJungle);
	ASSERT_FALSE(warm.empty());
	EXPECT_EQ(warm.front().tree, Tree::Cedar);
	EXPECT_EQ(streams::BankTreesOf(Biome::Grassland).front().tree, Tree::Bush);
	EXPECT_TRUE(streams::BankTreesOf(Biome::DeadWoods).empty());
	EXPECT_TRUE(streams::BankTreesOf(Biome::Sea).empty());
}

TEST(LandgenStreams, TheStandsAndScalesHaveTheSpreadOfTheStory)
{
	EXPECT_EQ(streams::BankStandSize(0.0f), 1u);
	EXPECT_EQ(streams::BankStandSize(0.5f), 3u);
	EXPECT_EQ(streams::BankStandSize(0.9f), 12u);
	EXPECT_NEAR(streams::BankTreeScale(0.5f), 1.119f, 1e-4f);
	EXPECT_NEAR(streams::BankTreeScale(0.0f), 0.354f, 1e-4f);
	EXPECT_NEAR(streams::BankTreeScale(1.0f), 3.07f, 1e-4f);
}

TEST(LandgenStreams, TheBankTreesStandNearTheStreamOnAPartOfItsLength)
{
	const GreenLand land;
	River river = Line(50, 90, 10);
	for (const auto corner : river.corners)
	{
		river.points.push_back(
		    {.x = static_cast<float>(openblack::WholeQuotient(corner, k_Side)), .z = static_cast<float>(corner % k_Side)});
	}
	const std::vector<River> rivers {river};
	size_t total = 0;
	for (uint64_t seed = 1; seed <= 20; ++seed)
	{
		const auto trees = streams::PlaceBankTrees(land.Ground(), rivers, seed, {});
		total += trees.size();
		for (const auto& tree : trees)
		{
			EXPECT_EQ(tree.kind, features::Kind::Tree);
			const float side = std::abs(tree.z - 500.0f);
			EXPECT_GE(side, 2.0f - 1e-3f);
			EXPECT_LE(side, 9.0f + 1e-3f);
		}
	}
	// 800 m of stream: a tree each 5 m on 30% of the length gives about 48 trees for each seed, less those that do not
	// suit the ground
	EXPECT_GT(total, 20u * 15u);
	EXPECT_LT(total, 20u * 120u);
}

TEST(LandgenStreams, NoBankTreeStandsInAClearing)
{
	GreenLand land;
	for (int x = 0; x < k_Side; ++x)
	{
		for (int z = 40; z < 60; ++z)
		{
			land.keepClear.At(x, z) = 1;
		}
	}
	River river = Line(50, 90, 10);
	for (const auto corner : river.corners)
	{
		river.points.push_back(
		    {.x = static_cast<float>(openblack::WholeQuotient(corner, k_Side)), .z = static_cast<float>(corner % k_Side)});
	}
	const std::vector<River> rivers {river};
	EXPECT_TRUE(streams::PlaceBankTrees(land.Ground(), rivers, 5, {}).empty());
}

TEST(LandgenStreams, EachLandOfTheMeasureSeedsHasTheStreamsOfTheStory)
{
	// The seeds that the land measure compares with the lands of the story. The land of the story with the fewest
	// streams has 2 streams, and 1.17 streams for each square kilometre.
	constexpr std::array<uint64_t, 8> k_Seeds {1, 7, 42, 3, 11, 23, 64, 99};
	constexpr uint8_t k_SeaLevelAltitude = 3;
	for (const auto seed : k_Seeds)
	{
		const auto land = Generate(openblack::generated_land::SettingsOf({.seed = seed}));
		Grid<uint8_t> water(land.cells.side, 0);
		size_t corners = 0;
		for (size_t i = 0; i < land.cells.values.size(); ++i)
		{
			water.values[i] = land.cells.values[i].altitude <= k_SeaLevelAltitude ? 1 : 0;
			corners += water.values[i] == 0 ? 1 : 0;
		}
		const float area = static_cast<float>(corners) * 1.0e-4f;
		EXPECT_GE(land.rivers.size(), 2u) << "seed " << seed;
		EXPECT_GE(static_cast<float>(land.rivers.size()) / area, 1.17f) << "seed " << seed;
		for (const auto& stream : land.rivers)
		{
			EXPECT_GE(stream.points.size(), 2u) << "seed " << seed;
			EXPECT_TRUE(streams::Reaches(stream, land.rivers, water)) << "seed " << seed;
		}
	}
}
