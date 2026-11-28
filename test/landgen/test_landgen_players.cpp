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
#include <span>
#include <vector>

#include <gtest/gtest.h>
#include <landgen/Generator.h>
#include <landgen/Paths.h>
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

/// The settings for a number of players, with ranges of fairness that each place meets
players::Settings WithoutRanges(uint32_t count)
{
	auto settings = WithPlayers(count);
	settings.landShare = {.least = 0.0f, .most = 1.0f};
	settings.flatShare = {.least = 0.0f, .most = 1.0f};
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

TEST(LandgenPlayers, AStartIsOnThePartOfTheLandOfPlayerOne)
{
	// Player one is on the west square. The north square has the ground of player one, but a ring of cliffs divides it from
	// the land round it. The south square is higher, but a way goes to it.
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
	// The parts of the land: the ways cross level ground everywhere, but for the ring of cliffs round the north square
	const HeightCurve curve {};
	const Grid<uint8_t> none(k_Side, 0);
	Grid<float> walkable(k_Side, 20.0f);
	for (int x = 128 - 17; x <= 128 + 17; ++x)
	{
		for (int z = 80 - 17; z <= 80 + 17; ++z)
		{
			if (std::abs(x - 128) > 15 || std::abs(z - 80) > 15)
			{
				walkable.At(x, z) = 200.0f;
			}
		}
	}
	const auto parts = paths::Parts({.heights = walkable, .curve = curve, .water = none, .rivers = none}, paths::Settings {});
	ASSERT_NE(parts.At(128, 80), parts.At(40, 128));
	ASSERT_EQ(parts.At(128, 176), parts.At(40, 128));

	auto ground = k_Ground;
	ground.parts = &parts;
	const auto starts =
	    players::FindStarts(land.heights, land.sea, land.rivers, {.x = 40.0f, .z = 128.0f}, k_Offsets, ground, WithPlayers(2));
	ASSERT_EQ(starts.size(), 1u);
	EXPECT_NEAR(starts[0].x, 128.0f, 15.0f);
	EXPECT_NEAR(starts[0].z, 176.0f, 15.0f);
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

TEST(LandgenPlayers, TheGroundOfAStartHasNoLakeCorners)
{
	// All the land is flat and dry, and a lake covers the east half
	const FakeLand land;
	Grid<uint8_t> lakes(k_Side, 0);
	for (int x = 128; x < k_Side; ++x)
	{
		for (int z = 0; z < k_Side; ++z)
		{
			lakes.At(x, z) = 1;
		}
	}
	const auto rises = players::Rises(land.heights);
	const players::Settings settings;
	const auto dry = players::GroundAt(land.heights, rises, 128.0f, 128.0f, settings);
	const auto withLake = players::GroundAt(land.heights, rises, 128.0f, 128.0f, settings, &lakes);
	EXPECT_EQ(dry.flat, dry.land);
	EXPECT_NEAR(withLake.land, dry.land / 2.0f, dry.land * 0.05f);
	EXPECT_EQ(withLake.flat, withLake.land);
}

TEST(LandgenPlayers, AStartSeesTheLakesNearIt)
{
	// All the land is flat and dry, with a lake of radius 40 corners near player one. Without the lakes, the
	// search puts the second start next to the lake. With the lakes, the ground of the second start is as dry as the
	// ground of player one.
	const FakeLand land;
	Grid<uint8_t> lakes(k_Side, 0);
	for (int x = 0; x < k_Side; ++x)
	{
		for (int z = 0; z < k_Side; ++z)
		{
			if (Distance(static_cast<float>(x), static_cast<float>(z), 128.0f, 80.0f) <= 40.0f)
			{
				lakes.At(x, z) = 1;
			}
		}
	}
	const players::Start first {.x = 60.0f, .z = 128.0f};
	const auto rises = players::Rises(land.heights);
	const players::Settings settings;
	const auto firstLand = players::GroundAt(land.heights, rises, first.x, first.z, settings, &lakes).land;

	const auto blind = players::FindStarts(land.heights, land.sea, land.rivers, first, k_Offsets, k_Ground, WithoutRanges(2));
	ASSERT_EQ(blind.size(), 1u);
	EXPECT_LT(players::GroundAt(land.heights, rises, blind[0].x, blind[0].z, settings, &lakes).land, firstLand * 0.9f);

	auto ground = k_Ground;
	ground.lakes = &lakes;
	const auto starts = players::FindStarts(land.heights, land.sea, land.rivers, first, k_Offsets, ground, WithoutRanges(2));
	ASSERT_EQ(starts.size(), 1u);
	EXPECT_GE(players::GroundAt(land.heights, rises, starts[0].x, starts[0].z, settings, &lakes).land, firstLand * 0.98f);
}

TEST(LandgenPlayers, TheFlatGroundOfAStartIsFlatInTheUnitsThatTheGameDraws)
{
	// The heights are flat everywhere. The curve of the heights makes the ground on one side of player one steep in the
	// units that the game draws.
	const FakeLand land;
	Grid<float> units(k_Side, 20.0f);
	for (int x = 80; x < k_Side; ++x)
	{
		for (int z = 100; z < k_Side; ++z)
		{
			units.At(x, z) = 20.0f + (static_cast<float>(z % 8) * 3.0f);
		}
	}
	const players::Start first {.x = 60.0f, .z = 128.0f};
	const auto drawnRises = players::Rises(units);
	const players::Settings settings;
	const auto firstFlat = players::GroundAt(land.heights, drawnRises, first.x, first.z, settings).flat;

	const auto blind = players::FindStarts(land.heights, land.sea, land.rivers, first, k_Offsets, k_Ground, WithoutRanges(2));
	ASSERT_EQ(blind.size(), 1u);
	EXPECT_LT(players::GroundAt(land.heights, drawnRises, blind[0].x, blind[0].z, settings).flat, firstFlat * 0.8f);

	auto ground = k_Ground;
	ground.drawnUnits = &units;
	const auto starts = players::FindStarts(land.heights, land.sea, land.rivers, first, k_Offsets, ground, WithoutRanges(2));
	ASSERT_EQ(starts.size(), 1u);
	EXPECT_GE(players::GroundAt(land.heights, drawnRises, starts[0].x, starts[0].z, settings).flat, firstFlat * 0.9f);
}

TEST(LandgenPlayers, TheFairnessIsTheLeastValueOverTheMostValue)
{
	EXPECT_FLOAT_EQ(players::Fairness(std::array {80.0f, 100.0f, 90.0f}), 0.8f);
	EXPECT_FLOAT_EQ(players::Fairness(std::array {0.0f, 0.0f}), 0.0f);
	EXPECT_FLOAT_EQ(players::Fairness(std::span<const float> {}), 0.0f);
}

TEST(LandgenPlayers, TheDistanceToTheRangesIsZeroInTheTwoRanges)
{
	const players::Settings settings;
	// Land 0.90 and flat ground 0.80 are in the ranges
	const std::array inside {players::StartGround {.land = 100.0f, .flat = 100.0f},
	                         players::StartGround {.land = 90.0f, .flat = 80.0f}};
	EXPECT_FLOAT_EQ(players::DistanceToRanges(inside, settings), 0.0f);
	// Land 0.50 is 0.34 below its range, and flat ground 0.50 is 0.28 below its range
	const std::array below {players::StartGround {.land = 100.0f, .flat = 100.0f},
	                        players::StartGround {.land = 50.0f, .flat = 50.0f}};
	EXPECT_NEAR(players::DistanceToRanges(below, settings), 0.62f, 1e-5f);
	// The same ground at each start is 0.02 above each range
	const std::array same {players::StartGround {.land = 100.0f, .flat = 100.0f},
	                       players::StartGround {.land = 100.0f, .flat = 100.0f}};
	EXPECT_NEAR(players::DistanceToRanges(same, settings), 0.04f, 1e-5f);
}

TEST(LandgenPlayers, AStartKeepsTheGroundInTheRangesOfTheSkirmishLands)
{
	// Three round islands in the sea. Player one is on the west island, and a sixth of its ground is steep. Lakes cover
	// a fifth of the north island, and all its dry ground is flat. Lakes cover less of the south island, and a fifth of
	// its ground is steep. The north island costs less, but its dry land is out of the range. The ground of the south
	// island is in the two ranges.
	FakeLand land;
	land.heights = Grid<float>(k_Side, 0.0f);
	Grid<uint8_t> lakes(k_Side, 0);
	Grid<float> units(k_Side, 20.0f);
	const players::Start first {.x = 60.0f, .z = 128.0f};
	const players::Start north {.x = 130.0f, .z = 50.0f};
	const players::Start south {.x = 130.0f, .z = 206.0f};
	constexpr float k_Island = 52.0f;
	const auto near = [](int x, int z, const players::Start& middle) {
		return Distance(static_cast<float>(x), static_cast<float>(z), middle.x, middle.z) <= k_Island;
	};
	for (int x = 0; x < k_Side; ++x)
	{
		for (int z = 0; z < k_Side; ++z)
		{
			// The ground goes up 6 units and down again in each wave. The four rows at the two edges of a wave are steep.
			int wave = 0;
			uint32_t lakeShare = 0;
			if (near(x, z, first))
			{
				wave = 24;
			}
			else if (near(x, z, north))
			{
				lakeShare = 20;
			}
			else if (near(x, z, south))
			{
				wave = 20;
				lakeShare = 14;
			}
			else
			{
				continue;
			}
			land.heights.At(x, z) = 20.0f;
			if (wave > 0 && (z % wave) < wave / 2)
			{
				units.At(x, z) = 26.0f;
			}
			const auto hash = (static_cast<uint32_t>(x) * 73856093u) ^ (static_cast<uint32_t>(z) * 19349663u);
			lakes.At(x, z) = (hash % 100u) < lakeShare ? 1 : 0;
		}
	}
	auto ground = k_Ground;
	ground.lakes = &lakes;
	ground.drawnUnits = &units;
	const auto rises = players::Rises(units);
	const players::Settings settings;
	const auto groundOf = [&](const players::Start& start) {
		return players::GroundAt(land.heights, rises, start.x, start.z, settings, &lakes);
	};

	const auto blind = players::FindStarts(land.heights, land.sea, land.rivers, first, k_Offsets, ground, WithoutRanges(2));
	ASSERT_EQ(blind.size(), 1u);
	EXPECT_LT(blind[0].z, 128.0f);
	EXPECT_GT(players::DistanceToRanges(std::array {groundOf(first), groundOf(blind[0])}, settings), 0.0f);

	const auto starts = players::FindStarts(land.heights, land.sea, land.rivers, first, k_Offsets, ground, WithPlayers(2));
	ASSERT_EQ(starts.size(), 1u);
	EXPECT_GT(starts[0].z, 128.0f);
	EXPECT_FLOAT_EQ(players::DistanceToRanges(std::array {groundOf(first), groundOf(starts[0])}, settings), 0.0f);
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
	// A path joins the town of the other player to the land, and a path joins its temple to its town
	const auto endsAt = [&](float x, float z) {
		return std::ranges::any_of(land.paths, [&](const paths::Path& path) {
			const auto& end = path.points.back();
			return !path.points.empty() && Distance(end.x, end.z, x / k_MetresPerCorner, z / k_MetresPerCorner) <= 1.0f;
		});
	};
	const auto& other = land.otherPlayers.front();
	EXPECT_TRUE(endsAt(other.town.x, other.town.z));
	EXPECT_TRUE(endsAt(other.citadelX, other.citadelZ));
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
