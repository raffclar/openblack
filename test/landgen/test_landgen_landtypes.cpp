/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <array>
#include <queue>
#include <set>
#include <vector>

#include <gtest/gtest.h>
#include <landgen/Generator.h>
#include <landgen/LandTypes.h>
#include <landgen/Terrain.h>

using namespace openblack::landgen;
using landtypes::Type;

namespace
{
constexpr int k_Side = 257;

/// The settings of the shape of a land of the type
struct Shape
{
	terrain::BaseSettings base;
	terrain::MassifSettings massifs;
	climate::Settings climate;
	float topHeight {1250.0f};
};

Shape ShapeOf(Type type, uint64_t seed)
{
	Shape shape;
	shape.base.seaMargin = 20;
	landtypes::Apply(type, seed, shape.base, shape.massifs, shape.climate, shape.topHeight);
	return shape;
}

/// The corners above the sea of the first heights of the shape
size_t LandCorners(const Grid<float>& heights)
{
	return static_cast<size_t>(
	    std::ranges::count_if(heights.values, [](float height) { return height > terrain::k_SeaLevel; }));
}

/// The groups of land corners that join along the axes, with this many corners or more
size_t IslandsOf(const Grid<float>& heights, size_t smallest)
{
	std::vector<uint8_t> seen(heights.values.size(), 0);
	size_t islands = 0;
	for (size_t start = 0; start < heights.values.size(); ++start)
	{
		if (seen[start] != 0 || heights.values[start] <= terrain::k_SeaLevel)
		{
			continue;
		}
		size_t corners = 0;
		std::queue<size_t> open;
		open.push(start);
		seen[start] = 1;
		while (!open.empty())
		{
			const auto index = open.front();
			open.pop();
			++corners;
			const int x = heights.XOf(index);
			const int z = heights.ZOf(index);
			for (const auto [dx, dz] : std::array<std::array<int, 2>, 4> {{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}})
			{
				if (!heights.Contains(x + dx, z + dz))
				{
					continue;
				}
				const auto next = heights.Index(x + dx, z + dz);
				if (seen[next] == 0 && heights.values[next] > terrain::k_SeaLevel)
				{
					seen[next] = 1;
					open.push(next);
				}
			}
		}
		islands += corners >= smallest ? 1 : 0;
	}
	return islands;
}
} // namespace

TEST(LandgenLandTypes, TheTypeOfTheSettingsComesFirst)
{
	for (size_t i = 0; i < landtypes::k_TypeCount; ++i)
	{
		const auto type = static_cast<Type>(i);
		EXPECT_EQ(landtypes::Pick(7, {.type = type}), type);
	}
}

TEST(LandgenLandTypes, TheSeedPicksAnArchipelagoOnlyWhenTheSettingsLetIt)
{
	std::set<Type> without;
	std::set<Type> with;
	for (uint64_t seed = 0; seed < 400; ++seed)
	{
		without.insert(landtypes::Pick(seed, {}));
		with.insert(landtypes::Pick(seed, {.archipelago = true}));
	}
	EXPECT_EQ(without.size(), landtypes::k_TypeCount - 1);
	EXPECT_FALSE(without.contains(Type::Archipelago));
	EXPECT_EQ(with.size(), landtypes::k_TypeCount);
}

TEST(LandgenLandTypes, TheIslandOfTheStorySizeIsTheMostCommonType)
{
	std::array<int, landtypes::k_TypeCount> counts {};
	for (uint64_t seed = 0; seed < 1000; ++seed)
	{
		++counts[static_cast<size_t>(landtypes::Pick(seed, {}))];
	}
	const auto story = counts[static_cast<size_t>(Type::StoryIsland)];
	for (size_t i = 0; i < counts.size(); ++i)
	{
		if (static_cast<Type>(i) != Type::StoryIsland)
		{
			EXPECT_GT(story, counts[i]);
		}
	}
}

TEST(LandgenLandTypes, ABigIslandChangesNothing)
{
	const Shape usual;
	const auto big = ShapeOf(Type::BigIsland, 7);
	EXPECT_TRUE(big.base.islands.empty());
	EXPECT_EQ(big.massifs.countLow, usual.massifs.countLow);
	EXPECT_EQ(big.massifs.radiusHigh, usual.massifs.radiusHigh);
	EXPECT_EQ(big.climate.northTemperature, usual.climate.northTemperature);
	EXPECT_EQ(big.base.plainHeight, usual.base.plainHeight);
	EXPECT_EQ(big.topHeight, usual.topHeight);
}

TEST(LandgenLandTypes, ASmallerIslandHasFewerAndSmallerMassifs)
{
	const Shape usual;
	for (uint64_t seed = 1; seed <= 8; ++seed)
	{
		const auto story = ShapeOf(Type::StoryIsland, seed);
		ASSERT_EQ(story.base.islands.size(), 1u);
		EXPECT_LT(story.base.islands[0].size, 0.6f);
		EXPECT_LT(story.massifs.radiusHigh, usual.massifs.radiusHigh);
		EXPECT_GE(story.massifs.countLow, 1);
		EXPECT_LT(story.massifs.countHigh, usual.massifs.countLow);
		// The mountains are lower, but not lower than the mountains of the story
		EXPECT_LT(story.topHeight, usual.topHeight);
		EXPECT_GE(story.topHeight, landtypes::k_StoryTopHeight);
		EXPECT_LE(story.massifs.countLow, story.massifs.countHigh);
		// The land keeps a change of temperature from north to south, in the range of the map
		EXPECT_LT(story.climate.northTemperature, story.climate.southTemperature);
		const float middle = (story.climate.northTemperature + story.climate.southTemperature) * 0.5f;
		EXPECT_GT(middle, usual.climate.northTemperature);
		EXPECT_LT(middle, usual.climate.southTemperature);
	}
}

TEST(LandgenLandTypes, TheHighlandsAndTheValleysChangeTheShape)
{
	const Shape usual;
	const auto highlands = ShapeOf(Type::Highlands, 3);
	EXPECT_GT(highlands.base.plainHeight, usual.base.plainHeight);
	EXPECT_GT(highlands.massifs.lowlandCeiling, usual.massifs.lowlandCeiling);
	const auto valleys = ShapeOf(Type::Valleys, 3);
	EXPECT_GE(valleys.base.ranges, usual.base.ranges + 2);
}

TEST(LandgenLandTypes, TheIslandsOfAnArchipelagoStandApart)
{
	for (uint64_t seed = 1; seed <= 20; ++seed)
	{
		const auto shape = ShapeOf(Type::Archipelago, seed);
		const auto& islands = shape.base.islands;
		ASSERT_GE(islands.size(), static_cast<size_t>(landtypes::k_FewestIslands));
		ASSERT_LE(islands.size(), static_cast<size_t>(landtypes::k_MostIslands));
		for (size_t i = 0; i < islands.size(); ++i)
		{
			EXPECT_GT(islands[i].centreU - (islands[i].size * 0.41f), 0.05f);
			EXPECT_LT(islands[i].centreU + (islands[i].size * 0.41f), 0.95f);
			for (size_t j = i + 1; j < islands.size(); ++j)
			{
				const float du = islands[i].centreU - islands[j].centreU;
				const float dv = islands[i].centreV - islands[j].centreV;
				EXPECT_GT(std::sqrt((du * du) + (dv * dv)), (islands[i].size + islands[j].size) * 0.41f);
			}
		}
	}
}

TEST(LandgenLandTypes, TheIslandOfTheStorySizeHasLessLand)
{
	for (uint64_t seed = 1; seed <= 4; ++seed)
	{
		const auto big = ShapeOf(Type::BigIsland, seed);
		const auto story = ShapeOf(Type::StoryIsland, seed);
		const auto bigLand = LandCorners(terrain::BaseHeights(k_Side, seed, big.base));
		const auto storyLand = LandCorners(terrain::BaseHeights(k_Side, seed, story.base));
		const float size = story.base.islands[0].size;
		EXPECT_GT(storyLand, static_cast<size_t>(static_cast<float>(bigLand) * size * size * 0.5f)) << seed;
		EXPECT_LT(storyLand, static_cast<size_t>(static_cast<float>(bigLand) * size * size * 1.6f)) << seed;
		EXPECT_EQ(IslandsOf(terrain::BaseHeights(k_Side, seed, story.base), 200), 1u) << seed;
	}
}

TEST(LandgenLandTypes, AnArchipelagoHasManyIslands)
{
	for (uint64_t seed = 1; seed <= 4; ++seed)
	{
		const auto shape = ShapeOf(Type::Archipelago, seed);
		EXPECT_GE(IslandsOf(terrain::BaseHeights(k_Side, seed, shape.base), 100), 3u) << seed;
	}
}

TEST(LandgenLandTypes, TheGeneratorKeepsTheTypeOfTheLand)
{
	Settings settings;
	settings.seed = 5;
	settings.side = 129;
	settings.base.seaMargin = 12;
	settings.base.hollows = 2;
	settings.base.hollowRadius = 6.0f;
	settings.hydraulic.drops = 4000;
	settings.thermal.passes = 4;
	settings.rivers.sourceFlow = 150.0f;
	settings.landType.type = Type::Highlands;
	const auto land = Generate(settings);
	EXPECT_EQ(land.type, Type::Highlands);
}
