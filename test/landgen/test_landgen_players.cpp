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
#include <limits>
#include <set>
#include <vector>

#include <gtest/gtest.h>
#include <landgen/Generator.h>
#include <landgen/Players.h>
#include <landgen/Settlement.h>

using namespace openblack::landgen;

namespace
{
constexpr int k_Side = 257;
constexpr float k_MetresPerCorner = 10.0f;
/// The distance of the near woods of a town in metres
constexpr float k_Near = 300.0f;

float Distance(float ax, float az, float bx, float bz)
{
	return std::sqrt(((ax - bx) * (ax - bx)) + ((az - bz) * (az - bz)));
}

/// A fake land: flat dry ground at 20 altitudes, 30 corners from the sea everywhere and far from rivers
struct FakeLand
{
	Grid<float> heights {k_Side, 20.0f};
	Grid<float> sea {k_Side, 30.0f};
	Grid<float> rivers {k_Side, 100.0f};
};

/// The ground of the start of player one
constexpr players::Ground k_Ground {
    .lowest = 10.0f, .highest = 70.0f, .nearestSea = 15.0f, .farthestSea = 90.0f, .riverDistance = 10.0f, .flatRadius = 9.0f};

/// Each town is 5 corners east of the middle of its start
constexpr std::array<players::Start, players::k_MostPlayers> k_Offsets {
    {{.x = 5.0f, .z = 0.0f}, {.x = 5.0f, .z = 0.0f}, {.x = 5.0f, .z = 0.0f}, {.x = 5.0f, .z = 0.0f}}};

players::Settings WithPlayers(uint32_t count)
{
	players::Settings settings;
	settings.count = count;
	return settings;
}

/// A small land for the generator, as the tests of the towns use
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
	settings.wind = climate::Wind::West;
	// The big island is the shape that the land had before the types of land
	settings.landType.type = landtypes::Type::BigIsland;
	return settings;
}
} // namespace

TEST(LandgenPlayers, ALandForOnePlayerHasNoOtherStart)
{
	const FakeLand land;
	const auto starts = players::FindStarts(land.heights, land.sea, land.rivers, {.x = 128.0f, .z = 128.0f}, k_Offsets,
	                                        k_Ground, WithPlayers(1));
	EXPECT_TRUE(starts.empty());
}

TEST(LandgenPlayers, EachTownIsAsFarFromTheOtherTownsAsOnTheSkirmishLands)
{
	const FakeLand land;
	const players::Start first {.x = 40.0f, .z = 128.0f};
	for (uint32_t count = 2; count <= players::k_MostPlayers; ++count)
	{
		const auto settings = WithPlayers(count);
		const auto starts = players::FindStarts(land.heights, land.sea, land.rivers, first, k_Offsets, k_Ground, settings);
		ASSERT_EQ(starts.size(), count - 1);
		std::vector<players::Start> towns {{.x = first.x + 5.0f, .z = first.z}};
		for (const auto& start : starts)
		{
			const players::Start town {.x = start.x + 5.0f, .z = start.z};
			float closest = std::numeric_limits<float>::max();
			for (const auto& other : towns)
			{
				closest = std::min(closest, Distance(town.x, town.z, other.x, other.z) * k_MetresPerCorner);
			}
			EXPECT_GE(closest, settings.nearestRival);
			EXPECT_LE(closest, settings.farthestRival);
			towns.push_back(town);
		}
	}
}

TEST(LandgenPlayers, AStartTakesTheGroundThatIsNearestToTheGroundOfPlayerOne)
{
	// The land is too high for a start, but for three squares. Player one is on the west square. The north square has
	// the height of player one, and the south square is 30 altitudes higher.
	FakeLand land;
	land.heights = Grid<float>(k_Side, 90.0f);
	const auto square = [&land](int middleX, int middleZ, float height) {
		for (int x = middleX - 15; x <= middleX + 15; ++x)
		{
			for (int z = middleZ - 15; z <= middleZ + 15; ++z)
			{
				land.heights.At(x, z) = height;
			}
		}
	};
	square(40, 128, 20.0f);
	square(128, 80, 20.0f);
	square(128, 176, 50.0f);
	const auto starts = players::FindStarts(land.heights, land.sea, land.rivers, {.x = 40.0f, .z = 128.0f}, k_Offsets, k_Ground,
	                                        WithPlayers(2));
	ASSERT_EQ(starts.size(), 1u);
	EXPECT_NEAR(starts[0].x, 128.0f, 15.0f);
	EXPECT_NEAR(starts[0].z, 80.0f, 15.0f);
}

TEST(LandgenPlayers, ALandWithNoRoomGetsFewerStarts)
{
	// Only a strip of 400 m of the land meets the rules of a start, so the land has no room for a second town
	FakeLand land;
	for (int x = 0; x < k_Side; ++x)
	{
		for (int z = 0; z < k_Side; ++z)
		{
			if (x < 100 || x > 140)
			{
				land.heights.At(x, z) = 90.0f;
			}
			if (z < 100 || z > 140)
			{
				land.heights.At(x, z) = 90.0f;
			}
		}
	}
	const auto starts = players::FindStarts(land.heights, land.sea, land.rivers, {.x = 120.0f, .z = 120.0f}, k_Offsets,
	                                        k_Ground, WithPlayers(3));
	EXPECT_TRUE(starts.empty());
}

TEST(LandgenPlayers, TheGroundCountsTheDryLandAndTheFlatLand)
{
	// The west half is sea. In the east half, the north half is steep and the south half is flat.
	FakeLand land;
	for (int x = 0; x < k_Side; ++x)
	{
		for (int z = 0; z < k_Side; ++z)
		{
			if (x < 128)
			{
				land.heights.At(x, z) = 0.0f;
			}
			else if (z < 128)
			{
				land.heights.At(x, z) = 20.0f + (static_cast<float>(z % 8) * 3.0f);
			}
		}
	}
	const auto rises = players::Rises(land.heights);
	const players::Settings settings;
	const auto ground = players::GroundAt(land.heights, rises, 128.0f, 128.0f, settings);
	// The circle has a radius of 50 corners: about half of it is dry, and about a half of the dry land is flat
	constexpr float k_Circle = 3.14159265f * 50.0f * 50.0f;
	EXPECT_NEAR(ground.land, k_Circle / 2.0f, k_Circle * 0.05f);
	EXPECT_NEAR(ground.flat, ground.land / 2.0f, k_Circle * 0.05f);
}

TEST(LandgenPlayers, EachPlayerHasItsOwnSeed)
{
	std::set<uint64_t> seeds {7};
	for (uint32_t player = 1; player < players::k_MostPlayers; ++player)
	{
		EXPECT_TRUE(seeds.insert(players::SeedOfPlayer(7, player)).second);
	}
	EXPECT_EQ(players::SeedOfPlayer(7, 2), players::SeedOfPlayer(7, 2));
}

TEST(LandgenPlayers, TheNeutralTownsComeInAdditionToTheTownsOfThePlayers)
{
	const FakeLand land;
	const settlement::SiteSettings settings;
	const settlement::Site first {.x = 60.0f, .z = 128.0f};
	const std::array firsts {first, settlement::Site {.x = 200.0f, .z = 128.0f}};
	for (uint64_t seed = 1; seed <= 10; ++seed)
	{
		const auto one = settlement::FindSites(land.heights, HeightCurve {}, land.sea, land.rivers, first, {}, seed, settings);
		const auto two = settlement::FindSites(land.heights, HeightCurve {}, land.sea, land.rivers, firsts, {}, seed, settings);
		EXPECT_EQ(two.size(), one.size());
		for (const auto& site : two)
		{
			for (const auto& town : firsts)
			{
				EXPECT_GE(Distance(site.x, site.z, town.x, town.z) * k_MetresPerCorner, settings.nearestTown);
			}
		}
	}
}

TEST(LandgenPlayers, AGeneratedLandForTwoPlayersHasAStartForEachPlayer)
{
	auto settings = SmallSettings(7);
	settings.players.count = 2;
	// The small land is 1.3 km across, so the starts can be nearer than on the skirmish lands
	settings.players.nearestRival = 300.0f;
	const auto land = Generate(settings);
	ASSERT_EQ(land.otherStarts.size(), 1u);
	ASSERT_EQ(land.otherPlayers.size(), 1u);
	for (const auto& plan : land.otherPlayers)
	{
		EXPECT_FALSE(plan.town.buildings.empty());
		EXPECT_TRUE(plan.neutral.empty());
		EXPECT_GE(Distance(plan.town.x, plan.town.z, land.settlement.town.x, land.settlement.town.z),
		          settings.players.nearestRival);
	}
	// No tree stands on the flat ground of a start
	for (const auto& start : land.otherStarts)
	{
		for (const auto& feature : land.features)
		{
			EXPECT_GE(Distance(feature.x / k_MetresPerCorner, feature.z / k_MetresPerCorner, start.x, start.z),
			          StartSettings {}.blendRadius);
		}
	}
}

TEST(LandgenPlayers, ALandForOnePlayerIsTheLandOfBefore)
{
	auto one = SmallSettings(11);
	one.players.count = 1;
	const auto land = Generate(one);
	EXPECT_TRUE(land.otherStarts.empty());
	EXPECT_TRUE(land.otherPlayers.empty());
	const auto again = Generate(SmallSettings(11));
	EXPECT_EQ(land.settlement.town.x, again.settlement.town.x);
	EXPECT_EQ(land.settlement.neutral.size(), again.settlement.neutral.size());
	EXPECT_EQ(land.features.size(), again.features.size());
}

TEST(LandgenPlayers, TheStartWithFewTreesGetsWoodsLikeTheWoodsOfTheRichestStart)
{
	const FakeLand land;
	const Grid<biomes::Biome> biomeGrid(k_Side, biomes::Biome::Grassland);
	const Grid<uint8_t> keepClear(k_Side, 0);
	const std::array towns {players::Start {.x = 60.0f, .z = 128.0f}, players::Start {.x = 190.0f, .z = 128.0f}};
	const std::array others {features::Clearing {.x = towns[1].x, .z = towns[1].z, .radius = 0.0f}};
	const features::Ground ground {.biomes = biomeGrid,
	                               .heights = land.heights,
	                               .seaDistance = land.sea,
	                               .keepClear = keepClear,
	                               .clearings = {},
	                               .townX = towns[0].x,
	                               .townZ = towns[0].z,
	                               .towns = others};
	// Player one has a ring of oaks 100 to 200 m from its town. Player two has no tree.
	std::vector<features::Feature> trees;
	for (int i = 0; i < 80; ++i)
	{
		const float angle = static_cast<float>(i) * 0.3f;
		const float distance = 100.0f + static_cast<float>(i);
		trees.push_back({.kind = features::Kind::Tree,
		                 .type = static_cast<uint8_t>(features::Tree::Oak),
		                 .x = (towns[0].x * k_MetresPerCorner) + (distance * std::cos(angle)),
		                 .z = (towns[0].z * k_MetresPerCorner) + (distance * std::sin(angle)),
		                 .angle = 0.0f,
		                 .size = 1.0f,
		                 .maxSize = 1.0f});
	}
	const players::Settings settings;
	ASSERT_EQ(players::TreesNear(trees, towns[0].x * k_MetresPerCorner, towns[0].z * k_MetresPerCorner, k_Near), 80u);
	players::EvenTheWoods(trees, ground, towns, settings);
	EXPECT_EQ(players::TreesNear(trees, towns[0].x * k_MetresPerCorner, towns[0].z * k_MetresPerCorner, k_Near), 80u);
	// Three quarters of the trees of player one, at the same places from the town
	EXPECT_EQ(players::TreesNear(trees, towns[1].x * k_MetresPerCorner, towns[1].z * k_MetresPerCorner, k_Near), 60u);
	EXPECT_EQ(trees.size(), 140u);
	const float shift = (towns[1].x - towns[0].x) * k_MetresPerCorner;
	EXPECT_FLOAT_EQ(trees[80].x, trees[0].x + shift);
	EXPECT_FLOAT_EQ(trees[80].z, trees[0].z);
}
