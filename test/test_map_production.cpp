/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <vector>

#include <entt/meta/context.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/MapCellResident.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/Transform.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Registry.h"
#include "Locator.h"

#define LOCATOR_IMPLEMENTATIONS
#include "ECS/MapProduction.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

class MapProductionFiling: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<Registry>();
		Locator::entitiesMap::emplace<MapProduction>();
	}
	void TearDown() override
	{
		Locator::entitiesMap::reset();
		Locator::entitiesRegistry::reset();
	}
};

TEST_F(MapProductionFiling, AMadeFootballIsPutOnTheMapAtOnce)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto ball = registry.Create();
	registry.Assign<Transform>(ball, glm::vec3(500.0f, 0.0f, 500.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Mobile>(ball);
	registry.Assign<Ball>(ball);
	Locator::entitiesMap::value().Sync();
	EXPECT_TRUE(registry.AllOf<MapCellResident>(ball));
}

TEST_F(MapProductionFiling, AHandfulRefiledAsItIsMadeIsOnTheMapAtOnceAndOnlyOnce)
{
	// A creature picks up a handful from a pile in the same turn that it makes the handful. Thus the handful must be in
	// the map before a query of the map.
	auto& registry = Locator::entitiesRegistry::value();
	auto& map = Locator::entitiesMap::value();
	const auto handful = registry.Create();
	registry.Assign<Transform>(handful, glm::vec3(500.0f, 0.0f, 500.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Pot>(handful, Pot {.amount = 300, .maxAmount = 600, .type = PotInfo::WheatInHand});
	EXPECT_FALSE(registry.AllOf<MapCellResident>(handful));
	map.Refile(handful);
	ASSERT_TRUE(registry.AllOf<MapCellResident>(handful));
	const auto cell = map_coords::CellOf(glm::vec3(500.0f, 0.0f, 500.0f));
	const auto inCell = map.GetAllInCell(cell);
	EXPECT_EQ(std::ranges::count(inCell, handful), 1);
}

TEST_F(MapProductionFiling, AStoneHandOfAVillageCentreIsNeverPutOnTheMap)
{
	// A village centre's stone hand is not put in the land's cells. Thus no search of the cells finds it, and no miracle,
	// fire or heal that looks in the cells reaches it.
	auto& registry = Locator::entitiesRegistry::value();
	auto& map = Locator::entitiesMap::value();
	const auto hand = registry.Create();
	registry.Assign<Transform>(hand, glm::vec3(500.0f, 0.0f, 500.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<TownCentreIcon>(hand, TownCentreIcon {.hand = 0, .seed = SpellSeedType::Heal});
	map.Sync();
	map.Refile(hand);
	EXPECT_FALSE(registry.AllOf<MapCellResident>(hand));
	const auto inCell = map.GetAllInCell(map_coords::CellOf(glm::vec3(500.0f, 0.0f, 500.0f)));
	EXPECT_EQ(std::ranges::count(inCell, hand), 0);
}

TEST_F(MapProductionFiling, ASavedGameKeepsTheOrderOfTheThingsInACell)
{
	namespace reflection = openblack::ecs::reflection;
	auto& registry = Locator::entitiesRegistry::value();
	auto& map = Locator::entitiesMap::value();
	std::vector<entt::entity> balls;
	for (int i = 0; i < 3; ++i)
	{
		const auto ball = registry.Create();
		registry.Assign<Transform>(ball, glm::vec3(500.0f, 0.0f, 500.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<Mobile>(ball);
		registry.Assign<Ball>(ball);
		map.Sync();
		balls.push_back(ball);
	}
	// The first ball moves, and so goes to the front of its cell again
	map.Refile(balls.front());
	const auto cell = map_coords::CellOf(glm::vec3(500.0f, 0.0f, 500.0f));
	const auto before = map.GetAllInCell(cell);
	ASSERT_EQ(before.size(), 3u);
	const entt::meta_ctx context;
	reflection::BinaryWriter out(context);
	map.SaveState(out);

	// A load files the things again in the order of the registry, and then puts back the order of the save
	map.FileResidents();
	const reflection::EntityRemap identity;
	reflection::BinaryReader in(context, out.Bytes(), identity);
	ASSERT_TRUE(map.LoadState(in));
	EXPECT_EQ(in.Remaining(), 0u);
	EXPECT_EQ(map.GetAllInCell(cell), before);
	reflection::BinaryWriter again(context);
	map.SaveState(again);
	EXPECT_EQ(again.Bytes(), out.Bytes());
}
