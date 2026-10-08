/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The water queries (ECS/WaterQueries) and the creature's LandAvoid mask (3D/LandAvoid). The WaterQueries suite runs
// them on the real Land1 and needs the original game data, so it only runs with OPENBLACK_TEST_GAME_PATH set to the
// game's folder (else skipped). The WaterQueriesSynthetic suite runs the same checks on a small hand-made island.

#include <cmath>
#include <cstdlib>

#include <memory>
#include <stdexcept>
#include <vector>

#include <3D/LandAvoid.h>
#include <3D/LandIslandInterface.h>
#include <ECS/Components/Stream.h>
#include <ECS/Registry.h>
#include <ECS/SeaCells.h>
#include <ECS/WaterQueries.h>
#include <FileSystem/FileSystemInterface.h>
#include <Game.h>
#include <LNDFile.h>
#include <Locator.h>
#include <gtest/gtest.h>

#include "support/LandFakes.h"
#include "support/RestoreService.h"
#include "support/TestServices.h"

using namespace openblack;

class WaterQueries: public ::testing::Test
{
protected:
	static void SetUpTestSuite()
	{
		const char* path = std::getenv("OPENBLACK_TEST_GAME_PATH");
		if (path == nullptr)
		{
			return;
		}
		auto args = openblack::Arguments {
		    .graphicsBackend = openblack::GraphicsBackend::Noop,
		    .gamePath = path,
		    .numFramesToSimulate = 0,
		    .logFile = "stdout",
		};
		std::fill_n(args.logLevels.begin(), args.logLevels.size(), spdlog::level::warn);
		s_game = std::make_unique<openblack::Game>(std::move(args));
		if (!s_game->Initialize())
		{
			// a half-initialised Game crashes in its destructor: leak it and let the tests skip
			static_cast<void>(s_game.release());
			FAIL() << "Game::Initialize failed for " << path;
		}
		const auto script = Locator::filesystem::value().GetPath<filesystem::Path::Scripts>() / "Land1.txt";
		ASSERT_TRUE(s_game->LoadMap(script));
	}
	static void TearDownTestSuite()
	{
		// The listener has reset the services after the last test: the Game's shutdown needs them back
		if (s_game != nullptr)
		{
			openblack::test::EmplaceTestServices();
		}
		s_game.reset();
	}

	void SetUp() override
	{
		if (!s_game)
		{
			GTEST_SKIP() << "OPENBLACK_TEST_GAME_PATH is not set";
		}
	}

	static const LandIslandInterface& Island() { return Locator::terrainSystem::value(); }

	static std::unique_ptr<openblack::Game> s_game;
};

std::unique_ptr<openblack::Game> WaterQueries::s_game;

// PLAN §4.1 test points: open sea (146, 201), shallow coast water (148, 201), dry land (178, 271)
// Integration test: needs the original game data (OPENBLACK_TEST_GAME_PATH); skipped without it
TEST_F(WaterQueries, land_avoid_on_land1)
{
	// open sea, all corners at altitude 0: never walkable
	const auto sea = land_avoid::At(146, 201);
	EXPECT_TRUE(sea == land_avoid::k_Avoid || sea == land_avoid::k_Unreachable) << int(sea);
	// dry land in the middle of the island: reached land
	EXPECT_EQ(land_avoid::At(178, 271), land_avoid::k_Land);
	EXPECT_TRUE(land_avoid::IsPosValid({1788.4f, 0.0f, 2710.0f}));
	EXPECT_FALSE(land_avoid::IsPosValid({1464.0f, 0.0f, 2016.0f}));
	// off the map
	EXPECT_FALSE(land_avoid::IsPosValid({-5.0f, 0.0f, 2710.0f}));

	// every 6 is a water cell and every 0 a land cell; some of each
	size_t water = 0;
	size_t land = 0;
	for (int32_t z = 0; z < 512; ++z)
	{
		for (int32_t x = 0; x < 512; ++x)
		{
			const auto value = land_avoid::At(x, z);
			ASSERT_TRUE(value == 0 || value == 1 || value == 2 || value == 6) << x << "," << z << ": " << int(value);
			if (value == land_avoid::k_Water)
			{
				++water;
				EXPECT_TRUE(ecs::sea_cells::IsWater(Island(), {x, z}));
			}
			else if (value == land_avoid::k_Land)
			{
				++land;
				EXPECT_FALSE(ecs::sea_cells::IsWater(Island(), {x, z}));
			}
		}
	}
	EXPECT_GT(water, 0u);
	EXPECT_GT(land, 10000u);
}

// Integration test: needs the original game data (OPENBLACK_TEST_GAME_PATH); skipped without it
TEST_F(WaterQueries, nearest_coastal)
{
	// from the open sea next to the start beach, a coastal (land, coast line) cell within 100
	const glm::vec3 from {1464.0f, 0.0f, 2016.0f};
	const auto coast = ecs::water_queries::FindNearestCoastalTo(Island(), from, 100.0f);
	ASSERT_TRUE(coast.has_value());
	EXPECT_TRUE(ecs::sea_cells::IsCoastal(Island(), ecs::sea_cells::CellOf(*coast)));
	EXPECT_LE(ecs::water_queries::GetDistanceInMetres(from, *coast), 100.0f);
	// the spiral keeps the fraction of the start: whole cells away
	EXPECT_NEAR(std::fmod(coast->x - from.x + 1000.0f, 10.0f), 0.0f, 0.01f);
	// a coastal cell answers itself
	const auto again = ecs::water_queries::FindNearestCoastalTo(Island(), *coast, 0.0f);
	ASSERT_TRUE(again.has_value());
	EXPECT_NEAR(again->x, coast->x, 0.01f);
	EXPECT_NEAR(again->z, coast->z, 0.01f);
	// inland, far from the coast: nothing within 5
	EXPECT_FALSE(ecs::water_queries::FindNearestCoastalTo(Island(), {1788.4f, 0.0f, 2710.0f}, 5.0f).has_value());
}

// Integration test: needs the original game data (OPENBLACK_TEST_GAME_PATH); skipped without it
TEST_F(WaterQueries, drinking_water)
{
	auto& registry = Locator::entitiesRegistry::value();
	size_t streams = 0;
	glm::vec3 riverPoint {0.0f};
	registry.Each<const ecs::components::Stream>([&](const ecs::components::Stream& stream) {
		++streams;
		if (!stream.points.empty())
		{
			riverPoint = stream.points.front();
		}
	});
	EXPECT_EQ(streams, 11u); // Land1.txt: 11 CREATE_STREAM

	// next to a river point: the river is found within 20, and the drinking water is at most as far as the river
	const glm::vec3 from {riverPoint.x + 3.0f, 0.0f, riverPoint.z + 4.0f};
	const auto river = ecs::water_queries::FindNearestStreamPosTo(Island(), from, 20.0f);
	ASSERT_TRUE(river.has_value());
	// the nearest of all the river points (the rivers' points are close together: not always riverPoint)
	glm::vec3 nearest {0.0f};
	float best = 1e9f;
	registry.Each<const ecs::components::Stream>([&](const ecs::components::Stream& stream) {
		for (const auto& point : stream.points)
		{
			const float d = std::hypot(point.x - from.x, point.z - from.z);
			if (d < best)
			{
				best = d;
				nearest = point;
			}
		}
	});
	EXPECT_NEAR(river->x, nearest.x, 0.01f);
	EXPECT_NEAR(river->z, nearest.z, 0.01f);
	EXPECT_NEAR(river->y, nearest.y - Island().GetHeightAt({river->x, river->z}), 0.05f);

	glm::vec3 water {0.0f};
	ASSERT_TRUE(ecs::water_queries::FindNearestDrinkingWater(Island(), from, water, 20.0f));
	EXPECT_LE(ecs::water_queries::GetDistanceInMetres(from, water),
	          ecs::water_queries::GetDistanceInMetres(from, *river) + 0.01f);

	// the abode cache
	ecs::water_queries::DrinkingWater cache;
	EXPECT_FALSE(ecs::water_queries::GetNearestWaterPos(cache).has_value());
	EXPECT_TRUE(
	    ecs::water_queries::FindNearestDrinkingWater(Island(), cache, from, ecs::water_queries::k_AbodeDrinkingWaterRadius));
	ASSERT_TRUE(ecs::water_queries::GetNearestWaterPos(cache).has_value());

	// nothing within 1 in the middle of the open sea, and the out position does not change
	glm::vec3 unchanged {1.0f, 2.0f, 3.0f};
	EXPECT_FALSE(ecs::water_queries::FindNearestDrinkingWater(Island(), {1440.0f, 0.0f, 1990.0f}, unchanged, 1.0f));
	EXPECT_EQ(unchanged, glm::vec3(1.0f, 2.0f, 3.0f));
}

// Integration test: needs the original game data (OPENBLACK_TEST_GAME_PATH); skipped without it. Its checks need no
// data at all: WaterQueriesSynthetic.distance_in_metresSynthetic repeats them without the game
TEST_F(WaterQueries, distance_in_metres)
{
	// the table inverse square root of GUtils is within about 0.1 %
	const float d = ecs::water_queries::GetDistanceInMetres({100.0f, 0.0f, 100.0f}, {130.0f, 0.0f, 140.0f});
	EXPECT_NEAR(d, 50.0f, 0.1f);
	EXPECT_EQ(ecs::water_queries::GetDistanceInMetres({100.0f, 0.0f, 100.0f}, {100.0f, 0.0f, 100.0f}), 0.0f);
}

using openblack::test::MakeWaterCell;
using openblack::test::WaterCellIsland;

/// A 16 x 16 cell island, each column of cells (fixed x) the same from z = 0 to 15:
///   x 0..7   sea, altitude 0 with the water bit
///   x 8      the beach: land at altitude 10 on the coast line
///   x 9..12  land at altitude 10
///   x 13..15 sea, altitude 0 with the water bit
/// What the LandAvoid mask makes of it: a cell is walkable when its four corners are not all at altitude 0 and differ
/// by at most 10 x 0.67, so x 7..12 are walkable (x 7 and 12 touch the land at one side) and the rest is deep sea.
/// The flood starts at the last walkable cell that a deep sea cell ends in the last row, (12, 15), and reaches x 7..12;
/// the deep sea next to it, x 6 and 13, stays to avoid and the rest is unreachable; x 7 is reached water.
/// Two rivers: one with the points (100, 15, 100) and (100, 15, 110), one with (120, 20, 40).
class WaterQueriesSynthetic: public ::testing::Test
{
protected:
	void SetUp() override
	{
		for (uint16_t x = 0; x < 16; ++x)
		{
			for (uint16_t z = 0; z < 16; ++z)
			{
				const bool land = x >= 8 && x <= 12;
				_island.Cell({x, z}) = land ? MakeWaterCell(10, false, x == 8) : MakeWaterCell(0, true, false);
			}
		}
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		AddStream(1, {{100.0f, 15.0f, 100.0f}, {100.0f, 15.0f, 110.0f}});
		AddStream(2, {{120.0f, 20.0f, 40.0f}});
	}
	void TearDown() override
	{
		land_avoid::Clear();
		Locator::entitiesRegistry::reset();
	}

	static void AddStream(ecs::components::Stream::Id id, std::vector<glm::vec3> points)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto entity = registry.Create();
		auto& stream = registry.Assign<ecs::components::Stream>(entity);
		stream.id = id;
		stream.points = std::move(points);
	}

	[[nodiscard]] const LandIslandInterface& Island() const { return _island; }

	// whatever the registry held before (a Game's, for one) comes back after the test
	const openblack::test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	WaterCellIsland _island {1};
};

TEST_F(WaterQueriesSynthetic, land_avoid_on_land1Synthetic)
{
	land_avoid::Validate(Island());
	// deep sea: avoided next to the reached land, unreachable away from it
	EXPECT_EQ(land_avoid::At(6, 5), land_avoid::k_Avoid);
	EXPECT_EQ(land_avoid::At(13, 5), land_avoid::k_Avoid);
	EXPECT_EQ(land_avoid::At(3, 5), land_avoid::k_Unreachable);
	EXPECT_EQ(land_avoid::At(14, 5), land_avoid::k_Unreachable);
	// the reached land, and the reached sea cell next to the beach
	EXPECT_EQ(land_avoid::At(10, 5), land_avoid::k_Land);
	EXPECT_EQ(land_avoid::At(7, 5), land_avoid::k_Water);
	// off the map
	EXPECT_EQ(land_avoid::At(16, 5), land_avoid::k_Unreachable);
	EXPECT_EQ(land_avoid::At(-1, 5), land_avoid::k_Unreachable);

	// the middle of the land; the open sea; off the map ((int)(-15 x 0.1) is cell -1)
	EXPECT_TRUE(land_avoid::IsPosValid({105.0f, 0.0f, 55.0f}));
	EXPECT_FALSE(land_avoid::IsPosValid({35.0f, 0.0f, 55.0f}));
	EXPECT_FALSE(land_avoid::IsPosValid({-15.0f, 0.0f, 55.0f}));
	// the last land column: 10 from the centre of the avoided cell (135, 55) is valid, 7 is closer than 7.1
	EXPECT_TRUE(land_avoid::IsPosValid({125.0f, 0.0f, 55.0f}));
	EXPECT_FALSE(land_avoid::IsPosValid({128.0f, 0.0f, 55.0f}));

	// every 6 is a water cell and every 0 a land cell: the column x 7 and the five land columns
	size_t water = 0;
	size_t land = 0;
	for (int32_t z = 0; z < 16; ++z)
	{
		for (int32_t x = 0; x < 16; ++x)
		{
			const auto value = land_avoid::At(x, z);
			ASSERT_TRUE(value == 0 || value == 1 || value == 2 || value == 6) << x << "," << z << ": " << int(value);
			if (value == land_avoid::k_Water)
			{
				++water;
				EXPECT_TRUE(ecs::sea_cells::IsWater(Island(), {x, z}));
			}
			else if (value == land_avoid::k_Land)
			{
				++land;
				EXPECT_FALSE(ecs::sea_cells::IsWater(Island(), {x, z}));
			}
		}
	}
	EXPECT_EQ(water, 16u);
	EXPECT_EQ(land, 5u * 16u);
}

TEST_F(WaterQueriesSynthetic, nearest_coastalSynthetic)
{
	// from the open sea at cell (3, 5): the beach column x 8 is 5 cells away, within 100
	const glm::vec3 from {35.0f, 0.0f, 55.0f};
	const auto coast = ecs::water_queries::FindNearestCoastalTo(Island(), from, 100.0f);
	ASSERT_TRUE(coast.has_value());
	EXPECT_TRUE(ecs::sea_cells::IsCoastal(Island(), ecs::sea_cells::CellOf(*coast)));
	EXPECT_EQ(ecs::sea_cells::CellOf(*coast).x, 8);
	EXPECT_LE(ecs::water_queries::GetDistanceInMetres(from, *coast), 100.0f);
	// the spiral keeps the fraction of the start: whole cells away
	EXPECT_NEAR(std::fmod(coast->x - from.x + 1000.0f, 10.0f), 0.0f, 0.01f);
	// the beach is at least 50 away: nothing within 40
	EXPECT_FALSE(ecs::water_queries::FindNearestCoastalTo(Island(), from, 40.0f).has_value());
	// a coastal cell answers itself
	const auto again = ecs::water_queries::FindNearestCoastalTo(Island(), *coast, 0.0f);
	ASSERT_TRUE(again.has_value());
	EXPECT_NEAR(again->x, coast->x, 0.01f);
	EXPECT_NEAR(again->z, coast->z, 0.01f);
	// inland, 4 cells from the beach: nothing within 5
	EXPECT_FALSE(ecs::water_queries::FindNearestCoastalTo(Island(), {125.0f, 0.0f, 55.0f}, 5.0f).has_value());
}

TEST_F(WaterQueriesSynthetic, drinking_waterSynthetic)
{
	auto& registry = Locator::entitiesRegistry::value();
	size_t streams = 0;
	registry.Each<const ecs::components::Stream>([&](const ecs::components::Stream&) { ++streams; });
	EXPECT_EQ(streams, 2u);

	// 5 from (100, 15, 100), about 6.7 from (100, 15, 110): the first is the nearest river point within 20
	const glm::vec3 from {103.0f, 0.0f, 104.0f};
	const auto river = ecs::water_queries::FindNearestStreamPosTo(Island(), from, 20.0f);
	ASSERT_TRUE(river.has_value());
	EXPECT_NEAR(river->x, 100.0f, 0.01f);
	EXPECT_NEAR(river->z, 100.0f, 0.01f);
	// y: the point's altitude minus the ground's there
	EXPECT_NEAR(river->y, 15.0f - Island().GetHeightAt({river->x, river->z}), 0.05f);
	// no river point within 4.5
	EXPECT_FALSE(ecs::water_queries::FindNearestStreamPosTo(Island(), from, 4.5f).has_value());

	// no coast within the 5 of the river: the drinking water is the river point
	glm::vec3 water {0.0f};
	ASSERT_TRUE(ecs::water_queries::FindNearestDrinkingWater(Island(), from, water, 20.0f));
	EXPECT_LE(ecs::water_queries::GetDistanceInMetres(from, water),
	          ecs::water_queries::GetDistanceInMetres(from, *river) + 0.01f);
	EXPECT_NEAR(water.x, 100.0f, 0.01f);
	EXPECT_NEAR(water.z, 100.0f, 0.01f);

	// on the beach, the river about 15.5 away: the beach itself is nearer, so it is the drinking water
	const glm::vec3 beach {85.0f, 0.0f, 104.0f};
	glm::vec3 beachWater {0.0f};
	ASSERT_TRUE(ecs::water_queries::FindNearestDrinkingWater(Island(), beach, beachWater, 20.0f));
	EXPECT_NEAR(beachWater.x, 85.0f, 0.01f);
	EXPECT_NEAR(beachWater.z, 104.0f, 0.01f);

	// the abode cache
	ecs::water_queries::DrinkingWater cache;
	EXPECT_FALSE(ecs::water_queries::GetNearestWaterPos(cache).has_value());
	EXPECT_TRUE(
	    ecs::water_queries::FindNearestDrinkingWater(Island(), cache, from, ecs::water_queries::k_AbodeDrinkingWaterRadius));
	ASSERT_TRUE(ecs::water_queries::GetNearestWaterPos(cache).has_value());

	// nothing within 1 in the open sea, and the out position does not change
	glm::vec3 unchanged {1.0f, 2.0f, 3.0f};
	EXPECT_FALSE(ecs::water_queries::FindNearestDrinkingWater(Island(), {25.0f, 0.0f, 25.0f}, unchanged, 1.0f));
	EXPECT_EQ(unchanged, glm::vec3(1.0f, 2.0f, 3.0f));
}

TEST_F(WaterQueriesSynthetic, distance_in_metresSynthetic)
{
	// the table inverse square root of GUtils is within about 0.1 %
	const float d = ecs::water_queries::GetDistanceInMetres({100.0f, 0.0f, 100.0f}, {130.0f, 0.0f, 140.0f});
	EXPECT_NEAR(d, 50.0f, 0.1f);
	EXPECT_EQ(ecs::water_queries::GetDistanceInMetres({100.0f, 0.0f, 100.0f}, {100.0f, 0.0f, 100.0f}), 0.0f);
}
