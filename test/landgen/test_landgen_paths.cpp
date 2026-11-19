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

#include "landgen/Countries.h"
#include "landgen/Generator.h"
#include "landgen/Paths.h"

using namespace openblack::landgen;
using namespace openblack::landgen::paths;

namespace
{
constexpr int k_Side = 64;

/// Flat ground at 20 altitude units with no water and no rivers, and the settings with no wander
struct FlatLand
{
	Grid<float> heights {k_Side, 20.0f};
	HeightCurve curve {};
	Grid<uint8_t> water {k_Side, 0};
	Grid<uint8_t> rivers {k_Side, 0};
	Grid<uint8_t> laid {k_Side, 0};

	[[nodiscard]] Ground GroundOf() const { return {.heights = heights, .curve = curve, .water = water, .rivers = rivers}; }
};

paths::Settings Straight()
{
	paths::Settings settings;
	settings.wander = 0.0f;
	return settings;
}

bool Touches(const std::vector<Point>& way, const Grid<uint8_t>& mask)
{
	return std::ranges::any_of(
	    way, [&mask](const Point& point) { return mask.At(static_cast<int>(point.x), static_cast<int>(point.z)) != 0; });
}
} // namespace

TEST(LandgenPaths, EachPlaceJoinsTheNearestPlaceBeforeIt)
{
	const std::array<Place, 4> places {{{0.0f, 0.0f}, {10.0f, 0.0f}, {11.0f, 9.0f}, {-2.0f, 3.0f}}};
	const auto links = ChainLinks(places);
	ASSERT_EQ(links.size(), 3u);
	EXPECT_EQ(links[0].from, 0u);
	EXPECT_EQ(links[0].to, 1u);
	EXPECT_EQ(links[1].from, 1u);
	EXPECT_EQ(links[1].to, 2u);
	EXPECT_EQ(links[2].from, 0u);
	EXPECT_EQ(links[2].to, 3u);
}

TEST(LandgenPaths, AWayOverFlatGroundIsNearlyStraight)
{
	const FlatLand land;
	const auto way = FindWay(land.GroundOf(), land.laid, {5.0f, 10.0f}, {55.0f, 30.0f}, 1, Straight());
	ASSERT_TRUE(way.has_value());
	EXPECT_FLOAT_EQ(way->front().x, 5.0f);
	EXPECT_FLOAT_EQ(way->back().x, 55.0f);
	EXPECT_FLOAT_EQ(way->back().z, 30.0f);
	EXPECT_LT(WindingOf(*way), 1.15f);
}

TEST(LandgenPaths, AWayGoesRoundTheWaterThroughTheGap)
{
	FlatLand land;
	// A wall of water across the land, with a gap near one edge
	for (int z = 0; z < k_Side - 6; ++z)
	{
		land.water.At(32, z) = 1;
	}
	const auto way = FindWay(land.GroundOf(), land.laid, {10.0f, 10.0f}, {50.0f, 10.0f}, 1, Straight());
	ASSERT_TRUE(way.has_value());
	EXPECT_FALSE(Touches(*way, land.water));
	EXPECT_TRUE(std::ranges::any_of(*way, [](const Point& point) { return point.z >= static_cast<float>(k_Side - 6); }));
}

TEST(LandgenPaths, NoWayClimbsACliff)
{
	FlatLand land;
	// A cliff of 100 altitude units round the end
	for (int x = 0; x < k_Side; ++x)
	{
		for (int z = 0; z < k_Side; ++z)
		{
			if (x >= 40)
			{
				land.heights.At(x, z) = 120.0f;
			}
		}
	}
	EXPECT_FALSE(FindWay(land.GroundOf(), land.laid, {10.0f, 10.0f}, {50.0f, 10.0f}, 1, Straight()).has_value());
}

TEST(LandgenPaths, AWayGoesRoundAHillRatherThanOverIt)
{
	FlatLand land;
	// A round hill between the two ends, with gentle sides that a path can climb
	for (int x = 0; x < k_Side; ++x)
	{
		for (int z = 0; z < k_Side; ++z)
		{
			const float distance = std::hypot(static_cast<float>(x - 32), static_cast<float>(z - 32));
			land.heights.At(x, z) += std::max(0.0f, 60.0f - (5.0f * distance));
		}
	}
	const auto way = FindWay(land.GroundOf(), land.laid, {8.0f, 32.0f}, {56.0f, 32.0f}, 1, Straight());
	ASSERT_TRUE(way.has_value());
	const float highest = std::ranges::max(*way, {}, [&land](const Point& point) {
		                      return land.heights.At(static_cast<int>(point.x), static_cast<int>(point.z));
	                      }).x;
	EXPECT_LT(land.heights.At(static_cast<int>(highest), 32), 70.0f);
	EXPECT_TRUE(
	    std::ranges::none_of(*way, [](const Point& point) { return std::hypot(point.x - 32.0f, point.z - 32.0f) < 6.0f; }));
}

TEST(LandgenPaths, TheWanderNoiseMakesAWayWind)
{
	const FlatLand land;
	paths::Settings settings;
	settings.wander = 4.0f;
	settings.wanderScale = 6.0f;
	float most = 0.0f;
	for (uint64_t seed = 1; seed <= 4; ++seed)
	{
		const auto way = FindWay(land.GroundOf(), land.laid, {4.0f, 32.0f}, {60.0f, 32.0f}, seed, settings);
		ASSERT_TRUE(way.has_value());
		most = std::max(most, WindingOf(*way));
	}
	EXPECT_GT(most, 1.05f);
}

TEST(LandgenPaths, ASecondWayFollowsTheFirstWhereItCan)
{
	const FlatLand land;
	const paths::Settings settings = Straight();
	const std::array<Place, 3> places {{{8.0f, 32.0f}, {56.0f, 32.0f}, {56.0f, 36.0f}}};
	const std::array<Link, 2> links {{{0, 1}, {0, 2}}};
	const auto paths = Lay(land.GroundOf(), places, links, 1, settings);
	ASSERT_EQ(paths.size(), 2u);
	// The second path shares most of the way of the first, which is cheaper to walk
	const auto drawn = Draw(std::span(paths).first(1), land.water, {});
	const auto shared = std::ranges::count_if(paths[1].points, [&drawn](const Point& point) {
		return drawn.At(static_cast<int>(std::floor(point.x + 0.5f)), static_cast<int>(std::floor(point.z + 0.5f))) != 0;
	});
	EXPECT_GT(shared * 2, static_cast<long>(paths[1].points.size()));
}

TEST(LandgenPaths, SmoothingKeepsTheEnds)
{
	const std::array<Point, 5> points {{{0.0f, 0.0f}, {1.0f, 1.0f}, {2.0f, 0.0f}, {3.0f, 1.0f}, {4.0f, 0.0f}}};
	const auto smooth = Smooth(points, 3);
	ASSERT_EQ(smooth.size(), points.size());
	EXPECT_FLOAT_EQ(smooth.front().x, 0.0f);
	EXPECT_FLOAT_EQ(smooth.back().x, 4.0f);
	// The bends are less sharp
	EXPECT_LT(std::abs(smooth[2].z - smooth[1].z), 1.0f);
}

TEST(LandgenPaths, TheDrawnLineIsJoinedAlongTheAxesAndKeepsOffTheClearCircles)
{
	FlatLand land;
	land.water.At(30, 30) = 1;
	Path path {.link = {0, 1}, .points = {{5.0f, 5.0f}, {45.0f, 45.0f}}};
	const std::array<Clear, 1> clear {{{10.0f, 10.0f, 2.0f}}};
	const auto drawn = Draw(std::span(&path, 1), land.water, clear);
	EXPECT_EQ(drawn.At(30, 30), 0);
	EXPECT_EQ(drawn.At(10, 10), 0);
	EXPECT_EQ(drawn.At(20, 20), 1);
	// Each drawn corner of the diagonal has a drawn corner next to it along an axis
	for (int i = 14; i < 28; ++i)
	{
		ASSERT_EQ(drawn.At(i, i), 1);
		EXPECT_TRUE(drawn.At(i + 1, i) != 0 || drawn.At(i, i + 1) != 0);
	}
}

TEST(LandgenPaths, LanternsStandBesideTheLitPathsAtTheSpacingOfTheStory)
{
	FlatLand land;
	const paths::Settings settings;
	std::vector<Path> paths;
	paths.push_back({.link = {0, 1}, .points = {{2.0f, 32.0f}, {62.0f, 32.0f}}, .lit = true});
	paths.push_back({.link = {0, 1}, .points = {{2.0f, 10.0f}, {62.0f, 10.0f}}, .lit = false});
	const std::array<Place, 1> places {{{2.0f, 32.0f}}};
	const auto lanterns = PlaceLanterns(paths, places, land.water, {}, 7, settings);
	ASSERT_GE(lanterns.size(), 5u);
	for (const auto& lantern : lanterns)
	{
		// Beside the lit path, in metres
		EXPECT_NEAR(lantern.z, 320.0f, settings.farthestSide + 0.01f);
		EXPECT_GE(std::abs(lantern.z - 320.0f), settings.nearestSide - 0.01f);
		const float fromPlace = std::hypot(lantern.x - 20.0f, lantern.z - 320.0f);
		EXPECT_EQ(lantern.kind, fromPlace <= settings.streetReach ? LanternKind::Street : LanternKind::Country);
	}
	for (size_t i = 1; i < lanterns.size(); ++i)
	{
		const float gap = std::abs(lanterns[i].x - lanterns[i - 1].x);
		EXPECT_GE(gap, settings.streetSpacingLow - settings.farthestSide);
		EXPECT_LE(gap, settings.countrySpacingHigh * 2.0f);
	}
	EXPECT_TRUE(std::ranges::any_of(lanterns, [](const Lantern& l) { return l.kind == LanternKind::Country; }));
}

TEST(LandgenPaths, NoLanternStandsInTheWaterAndTheLandKeepsTheMostOfTheStory)
{
	FlatLand land;
	for (int x = 0; x < k_Side; ++x)
	{
		for (int z = 33; z < k_Side; ++z)
		{
			land.water.At(x, z) = 1;
		}
	}
	paths::Settings settings;
	settings.mostLanterns = 3;
	settings.streetSpacingLow = 5.0f;
	settings.streetSpacingHigh = 6.0f;
	std::vector<Path> paths {{.link = {0, 1}, .points = {{2.0f, 32.0f}, {62.0f, 32.0f}}, .lit = true}};
	const std::array<Place, 1> places {{{32.0f, 32.0f}}};
	const auto lanterns = PlaceLanterns(paths, places, land.water, {}, 3, settings);
	EXPECT_LE(lanterns.size(), 3u);
	for (const auto& lantern : lanterns)
	{
		EXPECT_LT(std::floor((lantern.z / 10.0f) + 0.5f), 33.0f);
	}
}

TEST(LandgenPaths, TheSameSeedLaysTheSamePaths)
{
	const FlatLand land;
	const paths::Settings settings;
	const std::array<Place, 3> places {{{8.0f, 8.0f}, {50.0f, 20.0f}, {20.0f, 55.0f}}};
	const auto links = ChainLinks(places);
	const auto first = Lay(land.GroundOf(), places, links, 42, settings);
	const auto second = Lay(land.GroundOf(), places, links, 42, settings);
	ASSERT_EQ(first.size(), second.size());
	for (size_t i = 0; i < first.size(); ++i)
	{
		ASSERT_EQ(first[i].points.size(), second[i].points.size());
		EXPECT_EQ(first[i].lit, second[i].lit);
		for (size_t p = 0; p < first[i].points.size(); ++p)
		{
			EXPECT_EQ(first[i].points[p].x, second[i].points[p].x);
			EXPECT_EQ(first[i].points[p].z, second[i].points[p].z);
		}
	}
}

TEST(LandgenPaths, AGeneratedLandHasPathsDrawnBetweenItsTownsAndTheTemple)
{
	openblack::landgen::Settings settings;
	settings.seed = 7;
	const auto land = Generate(settings);
	const size_t places = 2 + land.settlement.neutral.size();
	EXPECT_EQ(land.paths.size(), places - 1);
	EXPECT_LE(land.lanterns.size(), settings.paths.mostLanterns);
	const auto drawn =
	    std::ranges::count_if(land.cells.values, [](const auto& cell) { return cell.properties.country == k_PathCountry; });
	EXPECT_GT(drawn, 50);
	// The country of the paths comes after the biomes, then the two countries of the edges of the lakes, then the two
	// countries of the dark ground of the cursed places when the land has a place
	ASSERT_EQ(land.countries.size(), static_cast<size_t>(k_PathCountry) + 1 + countries::LakeShoreRecipes().size() +
	                                     (land.cursedPatches.empty() ? 0 : countries::CursedRecipes().size()));
	for (const auto& cell : land.cells.values)
	{
		if (cell.properties.country == k_PathCountry)
		{
			EXPECT_EQ(cell.properties.hasWater, 0);
		}
	}
	// No tree or rock stands on a drawn corner of a path
	for (const auto& feature : land.features)
	{
		const int x = static_cast<int>(std::floor((feature.x / 10.0f) + 0.5f));
		const int z = static_cast<int>(std::floor((feature.z / 10.0f) + 0.5f));
		if (land.cells.Contains(x, z))
		{
			EXPECT_NE(land.cells.At(x, z).properties.country, k_PathCountry);
		}
	}
}
