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
#include "ECS/Components/Abode.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/MapCellResident.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/MapCells.h"
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

// A creature holds a pot. The pot is out of the map's cells. A load makes the pot again with what the save keeps, and the
// pot stays out of the cells until the creature lets it go.
TEST_F(MapProductionFiling, AThingThatACreatureHoldsStaysOutOfTheCellsAfterALoad)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto creature = registry.Create();
	const auto pot = registry.Create();
	registry.Assign<Transform>(pot, glm::vec3(500.0f, 0.0f, 500.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Pot>(pot, Pot {.amount = 300, .maxAmount = 600, .type = PotInfo::WheatInHand});
	registry.Assign<HeldByCreature>(pot, HeldByCreature {.creature = creature});
	Locator::entitiesMap::value().Sync();
	EXPECT_FALSE(registry.AllOf<MapCellResident>(pot));
	const auto inCell = Locator::entitiesMap::value().GetAllInCell(map_coords::CellOf(glm::vec3(500.0f, 0.0f, 500.0f)));
	EXPECT_EQ(std::ranges::count(inCell, pot), 0);
}

TEST_F(MapProductionFiling, AVillagerThatAScriptMakesIsOnTheMapBeforeTheScriptReleasesIt)
{
	// A script makes a villager, gives it to a town and releases it in the same turn. Only a villager in the map can go
	// back to its tasks when a script releases it, so the villager goes into its cell when it is made, and only once.
	auto& registry = Locator::entitiesRegistry::value();
	auto& map = Locator::entitiesMap::value();
	const auto villager = registry.Create();
	registry.Assign<Transform>(villager, glm::vec3(500.0f, 0.0f, 500.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Mobile>(villager);
	registry.Assign<Villager>(villager);
	EXPECT_FALSE(registry.AllOf<MapCellResident>(villager));
	map.Refile(villager);
	ASSERT_TRUE(registry.AllOf<MapCellResident>(villager));
	map.Sync();
	const auto inCell = map.GetAllInCell(map_coords::CellOf(glm::vec3(500.0f, 0.0f, 500.0f)));
	EXPECT_EQ(std::ranges::count(inCell, villager), 1);
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

TEST_F(MapProductionFiling, AFieldIsInEachCellItsOutlineCoversAndLeavesThemWhenGone)
{
	// A field is a building: it goes into the front of the fixed things of each cell that its model's outline covers,
	// and so a search of any of those cells finds it. Here the outline is a circle of 4.88 units in the middle of a cell,
	// as a field of wheat has: the circle of each of the four cells beside it touches it, the cells at the corners not.
	auto& registry = Locator::entitiesRegistry::value();
	const glm::vec3 place {505.0f, 0.0f, 505.0f};
	Locator::entitiesMap::reset();
	Locator::entitiesMap::emplace<MapProduction>([place](const Registry& /*registry*/, entt::entity /*entity*/) {
		return std::optional(map_cells::OutlineOfBox(place, glm::mat3(1.0f), 1.0f, glm::vec3(0.0f), {4.88f, 1.0f, 4.88f}));
	});
	auto& map = Locator::entitiesMap::value();
	const auto field = registry.Create();
	registry.Assign<Transform>(field, place, glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Abode>(field, AbodeNumber::Field, 0u, 0u, 0u);
	registry.Assign<Field>(field);
	map.Sync();
	ASSERT_TRUE(registry.AllOf<MapCellResident>(field));
	const auto middle = map_coords::CellOf(place);
	for (const auto offset : {glm::ivec2 {0, 0}, glm::ivec2 {-1, 0}, glm::ivec2 {1, 0}, glm::ivec2 {0, -1}, glm::ivec2 {0, 1}})
	{
		const auto cell = middle + offset;
		const auto inCell = map.GetAllInCell(cell);
		ASSERT_FALSE(inCell.empty());
		EXPECT_EQ(inCell.front(), field);
	}
	for (const auto offset : {glm::ivec2 {-1, -1}, glm::ivec2 {1, 1}, glm::ivec2 {-1, 1}, glm::ivec2 {1, -1}})
	{
		const auto cell = middle + offset;
		EXPECT_TRUE(map.GetAllInCell(cell).empty());
	}
	registry.Destroy(field);
	EXPECT_TRUE(map.GetAllInCell(middle).empty());
	EXPECT_TRUE(map.GetAllInCell(middle + glm::ivec2 {1, 0}).empty());
}

TEST_F(MapProductionFiling, AThingIsInTheMapUntilItLeavesTheCells)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& map = Locator::entitiesMap::value();
	// A new villager goes into its cell when the map is asked
	const auto villager = registry.Create();
	registry.Assign<Transform>(villager, glm::vec3(500.0f, 0.0f, 500.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Mobile>(villager);
	registry.Assign<Villager>(villager);
	EXPECT_TRUE(map.InMap(villager));
	// A creature that holds it takes it out of the cells
	registry.Remove<MapCellResident>(villager);
	EXPECT_FALSE(map.InMap(villager));
	// A thing of a kind that the map does not keep in its cells counts as in the map. A thing that has gone does not.
	const auto other = registry.Create();
	registry.Assign<Transform>(other, glm::vec3(500.0f, 0.0f, 500.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	EXPECT_TRUE(map.InMap(other));
	registry.Destroy(other);
	EXPECT_FALSE(map.InMap(other));
}
