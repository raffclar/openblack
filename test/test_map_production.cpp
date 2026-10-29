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
#include "ECS/Components/Firefly.h"
#include "ECS/Components/MapCellResident.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/TeleportStone.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/LivingFacing.h"
#include "ECS/Map.h"
#include "ECS/MapCells.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Registry.h"
#include "ECS/ThingModel.h"
#include "Locator.h"
#include "Magic/TeleportRules.h"

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

TEST_F(MapProductionFiling, ATeleportStoneIsFiledAtItsFullSizeAndThenAtItsKeptSize)
{
	// A stone goes into the cells that its model's outline covers at once, at its full size. Then it shrinks to the size
	// it keeps, which takes it out of the cells and puts it back in them. Here the model's box has half sides of 3
	// units: at full size its circle reaches the four cells beside its own cell, and at a hundredth of that size it is
	// a circle of a unit (no side is less than a unit), which reaches only its own cell.
	auto& registry = Locator::entitiesRegistry::value();
	const glm::vec3 place {505.0f, 0.0f, 505.0f};
	Locator::entitiesMap::reset();
	Locator::entitiesMap::emplace<MapProduction>([](const Registry& from, entt::entity entity) {
		const auto& transform = from.Get<const Transform>(entity);
		return std::optional(map_cells::OutlineOfBox(transform.position, transform.rotation, transform.scale.x, glm::vec3(0.0f),
		                                             {3.0f, 3.0f, 3.0f}));
	});
	auto& map = Locator::entitiesMap::value();
	const auto stone = registry.Create();
	registry.Assign<Transform>(stone, place, glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<TeleportStone>(stone);
	map.Refile(stone);
	ASSERT_TRUE(registry.AllOf<MapCellResident>(stone));
	EXPECT_EQ(registry.Get<const MapCellResident>(stone).cells.size(), 5u);
	registry.Get<Transform>(stone).scale = glm::vec3(magic::teleport::k_StoneScale);
	map.Refile(stone);
	const auto middle = map_coords::CellOf(place);
	EXPECT_EQ(registry.Get<const MapCellResident>(stone).cells.size(), 1u);
	ASSERT_EQ(map.GetAllInCell(middle).size(), 1u);
	EXPECT_EQ(map.GetAllInCell(middle).front(), stone);
	EXPECT_TRUE(map.GetAllInCell(middle + glm::ivec2 {1, 0}).empty());
	// The map's search finds it on its land block, so the land's draw list decides it as it decides other things
	const auto block = glm::ivec2 {middle.x >> 4, middle.y >> 4};
	const auto onBlock = map.GetOnBlock(block);
	EXPECT_NE(std::ranges::find(onBlock, stone), onBlock.end());
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

TEST_F(MapProductionFiling, ALoadKeepsAPersonInACreaturesHandOutOfTheCells)
{
	// Two villagers stand in one cell. A creature takes one of them in its hand, which takes that villager out of the
	// cells. A load of the saved world must give the same cells and keep the held villager out of them.
	namespace reflection = openblack::ecs::reflection;
	auto& registry = Locator::entitiesRegistry::value();
	auto& map = Locator::entitiesMap::value();
	const glm::vec3 place {500.0f, 0.0f, 500.0f};
	std::vector<entt::entity> villagers;
	for (int i = 0; i < 2; ++i)
	{
		const auto villager = registry.Create();
		registry.Assign<Transform>(villager, place, glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<Mobile>(villager);
		registry.Assign<Villager>(villager);
		map.Sync();
		villagers.push_back(villager);
	}
	const auto held = villagers.back();
	registry.Remove<MapCellResident, MapCellMover>(held);
	const auto cell = map_coords::CellOf(place);
	const auto before = map.GetAllInCell(cell);
	ASSERT_EQ(before, std::vector<entt::entity> {villagers.front()});
	const entt::meta_ctx context;
	reflection::BinaryWriter out(context);
	map.SaveState(out);

	// The load makes each entity again, and so the map gets the held villager as a new thing
	registry.Remove<Villager>(held);
	registry.Assign<Villager>(held);
	map.FileResidents();
	// A system that looks in the cells before the map state loads must not put the held villager in them
	EXPECT_EQ(map.GetAllInCell(cell), before);
	EXPECT_FALSE((registry.AnyOf<MapCellResident, MapCellMover>(held)));
	const reflection::EntityRemap identity;
	reflection::BinaryReader in(context, out.Bytes(), identity);
	ASSERT_TRUE(map.LoadState(in));
	EXPECT_EQ(in.Remaining(), 0u);
	map.Sync();
	EXPECT_EQ(map.GetAllInCell(cell), before);
	EXPECT_FALSE((registry.AnyOf<MapCellResident, MapCellMover>(held)));
	reflection::BinaryWriter again(context);
	map.SaveState(again);
	EXPECT_EQ(again.Bytes(), out.Bytes());
}

namespace
{
/// A villager made at a place, with no walk: it stands where its metres are
entt::entity MakeVillager(Registry& registry, glm::vec3 at)
{
	const auto villager = registry.Create();
	registry.Assign<Transform>(villager, at, glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Mobile>(villager);
	registry.Assign<Villager>(villager);
	return villager;
}

std::vector<entt::entity> MobileIn(const MapInterface& map, glm::vec3 at)
{
	const auto things = map.GetMobileInGridCell(at);
	return {things.begin(), things.end()};
}
} // namespace

TEST_F(MapProductionFiling, AThingThatMovesIsInItsNewCellAtOnce)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& map = Locator::entitiesMap::value();
	const glm::vec3 here {500.0f, 0.0f, 500.0f};
	const glm::vec3 there {520.0f, 0.0f, 500.0f};
	const auto stayer = MakeVillager(registry, there);
	const auto walker = MakeVillager(registry, here);
	map.FileNew();
	ASSERT_EQ(MobileIn(map, there), std::vector {stayer});
	// It steps into the next cell: no sync is needed, and the last to come into the cell is met first
	registry.Get<Transform>(walker).position = there;
	map.Moved(walker);
	EXPECT_TRUE(MobileIn(map, here).empty());
	EXPECT_EQ(MobileIn(map, there), (std::vector {walker, stayer}));
	// A step in the same cell leaves the order of the cell as it is
	registry.Get<Transform>(stayer).position = there + glm::vec3(1.0f, 0.0f, 0.0f);
	map.Moved(stayer);
	EXPECT_EQ(MobileIn(map, there), (std::vector {walker, stayer}));
}

TEST_F(MapProductionFiling, AThingMadeBeforeAnotherMovesInIsInItsCellFirst)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& map = Locator::entitiesMap::value();
	const glm::vec3 here {500.0f, 0.0f, 500.0f};
	const glm::vec3 there {520.0f, 0.0f, 500.0f};
	const auto walker = MakeVillager(registry, here);
	map.FileNew();
	// A villager is born in the cell, then the walker steps in: the newcomer was there first, so the walker is met first
	const auto born = MakeVillager(registry, there);
	registry.Get<Transform>(walker).position = there;
	map.Moved(walker);
	EXPECT_EQ(MobileIn(map, there), (std::vector {walker, born}));
}

TEST_F(MapProductionFiling, AThingJustMadeIsInTheCells)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = MakeVillager(registry, {500.0f, 0.0f, 500.0f});
	EXPECT_TRUE(InMapCells(registry, villager));
}

TEST_F(MapProductionFiling, AThingMadeBeforeAnotherGoesIsInTheCellsBeforeTheOtherGoes)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto old = MakeVillager(registry, {500.0f, 0.0f, 500.0f});
	Locator::entitiesMap::value().FileNew();
	const auto born = MakeVillager(registry, {500.0f, 0.0f, 500.0f});
	registry.Destroy(old);
	EXPECT_TRUE(registry.AllOf<MapCellResident>(born));
}

TEST_F(MapProductionFiling, AMoveOutsideTheTurnWaitsForTheSync)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& map = Locator::entitiesMap::value();
	const glm::vec3 here {500.0f, 0.0f, 500.0f};
	const glm::vec3 there {520.0f, 0.0f, 500.0f};
	const auto villager = MakeVillager(registry, here);
	map.FileNew();
	registry.Get<Transform>(villager).position = there;
	EXPECT_EQ(MobileIn(map, here), std::vector {villager});
	map.Sync();
	EXPECT_EQ(MobileIn(map, there), std::vector {villager});
}

TEST_F(MapProductionFiling, AThingThatTurnsGoesToTheFrontOfItsCell)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& map = Locator::entitiesMap::value();
	const glm::vec3 here {500.0f, 0.0f, 500.0f};
	const auto first = MakeVillager(registry, here);
	map.FileNew();
	const auto second = MakeVillager(registry, here);
	map.FileNew();
	ASSERT_EQ(MobileIn(map, here), (std::vector {second, first}));
	// The first villager turns where it stands: it comes out of its cell and goes in again, at the front
	map.Reinsert(first);
	EXPECT_EQ(MobileIn(map, here), (std::vector {first, second}));
}

TEST_F(MapProductionFiling, AThingNotInTheCellsStaysOutOfThemWhenItTurns)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& map = Locator::entitiesMap::value();
	const glm::vec3 here {500.0f, 0.0f, 500.0f};
	const auto villager = MakeVillager(registry, here);
	map.FileNew();
	// A creature holds it, so it is not in the cells
	registry.Remove<MapCellResident>(villager);
	map.Reinsert(villager);
	EXPECT_FALSE(registry.AllOf<MapCellResident>(villager));
	EXPECT_TRUE(MobileIn(map, here).empty());
}

TEST_F(MapProductionFiling, AThingMadeBeforeAnotherTurnsIsInItsCellFirst)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& map = Locator::entitiesMap::value();
	const glm::vec3 here {500.0f, 0.0f, 500.0f};
	const auto turner = MakeVillager(registry, here);
	map.FileNew();
	// A villager is born in the cell, then the other turns: the newcomer went in first, so the one that turned is met first
	const auto born = MakeVillager(registry, here);
	map.Reinsert(turner);
	EXPECT_EQ(MobileIn(map, here), (std::vector {turner, born}));
}

TEST_F(MapProductionFiling, ABuildingThatChangesItsSizeCoversTheCellsOfItsNewOutline)
{
	// The outline grows with the scale of the building: at scale 1 it is inside its cell, at scale 3 it covers the four
	// cells beside it too
	auto& registry = Locator::entitiesRegistry::value();
	const glm::vec3 place {505.0f, 0.0f, 505.0f};
	Locator::entitiesMap::reset();
	Locator::entitiesMap::emplace<MapProduction>([](const Registry& from, entt::entity entity) {
		const auto& transform = from.Get<const Transform>(entity);
		return std::optional(map_cells::OutlineOfBox(transform.position, glm::mat3(1.0f), transform.scale.x, glm::vec3(0.0f),
		                                             {2.0f, 1.0f, 2.0f}));
	});
	auto& map = Locator::entitiesMap::value();
	const auto field = registry.Create();
	registry.Assign<Transform>(field, place, glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Abode>(field, AbodeNumber::Field, 0u, 0u, 0u);
	registry.Assign<Field>(field);
	map.FileNew();
	const auto middle = map_coords::CellOf(place);
	ASSERT_EQ(registry.Get<const MapCellResident>(field).cells.size(), 1u);
	EXPECT_TRUE(map.GetAllInCell(middle + glm::ivec2 {1, 0}).empty());
	registry.Get<Transform>(field).scale = glm::vec3(3.0f);
	map.Reinsert(field);
	EXPECT_EQ(registry.Get<const MapCellResident>(field).cells.size(), 5u);
	EXPECT_EQ(map.GetAllInCell(middle + glm::ivec2 {1, 0}), std::vector {field});
}

TEST_F(MapProductionFiling, AWalkerGoesToTheFrontOfItsCellOnlyWhenItsAngleChanges)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& map = Locator::entitiesMap::value();
	const glm::vec3 here {500.0f, 0.0f, 500.0f};
	const auto walker = MakeVillager(registry, here);
	registry.Assign<WallHug>(walker, WallHug {.goalPlace = {0, 0}, .yAngle = 0.0f, .speed = 1.0f});
	map.FileNew();
	const auto other = MakeVillager(registry, here);
	map.FileNew();
	ASSERT_EQ(MobileIn(map, here), (std::vector {other, walker}));
	// Facing the way it faces already, it stays where it is in the list of its cell
	living_facing::FaceAngle(registry, walker, 0.0f);
	EXPECT_EQ(MobileIn(map, here), (std::vector {other, walker}));
	// A new angle takes it to the front, and the walk takes the game angle of the new way
	living_facing::FaceGameAngle(registry, walker, 512);
	EXPECT_EQ(MobileIn(map, here), (std::vector {walker, other}));
	EXPECT_EQ(registry.Get<const WallHug>(walker).gameAngle, 512);
}

TEST_F(MapProductionFiling, ATotemCoversItsOutlineAndGoesToTheFrontOfItsCellsWhenItChanges)
{
	// A town centre's totem stands in the cells as a building does, at the front of the fixed things of each cell. Its
	// outline comes from the icon on its plinth.
	auto& registry = Locator::entitiesRegistry::value();
	const glm::vec3 place {505.0f, 0.0f, 505.0f};
	Locator::entitiesMap::reset();
	Locator::entitiesMap::emplace<MapProduction>([place](const Registry& /*registry*/, entt::entity /*entity*/) {
		return std::optional(map_cells::OutlineOfBox(place, glm::mat3(1.0f), 1.0f, glm::vec3(0.0f), {1.0f, 1.0f, 1.0f}));
	});
	auto& map = Locator::entitiesMap::value();
	constexpr entt::id_type k_Icon = 7;
	constexpr entt::id_type k_Plinth = 8;
	const auto icon = registry.Create();
	registry.Assign<Mesh>(icon, k_Icon, static_cast<int8_t>(0), static_cast<int8_t>(0));
	const auto totem = registry.Create();
	registry.Assign<Transform>(totem, place, glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Mesh>(totem, k_Plinth, static_cast<int8_t>(0), static_cast<int8_t>(0));
	registry.Assign<VillageTotem>(totem, VillageTotem {.icon = icon});
	EXPECT_EQ(thing_model::OutlineModelOf(registry, totem), std::optional(k_Icon));
	map.FileNew();
	const auto* resident = registry.TryGet<const MapCellResident>(totem);
	ASSERT_NE(resident, nullptr);
	EXPECT_EQ(resident->placement, map_cells::Placement::FixedFront);
	EXPECT_TRUE(resident->coversOutline);
	// A building filed after it goes in front of it; the totem goes to the front again when it is put in again
	const auto building = registry.Create();
	registry.Assign<Transform>(building, place, glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Abode>(building, AbodeNumber::Field, 0u, 0u, 0u);
	map.FileNew();
	const auto middle = map_coords::CellOf(place);
	ASSERT_FALSE(map.GetAllInCell(middle).empty());
	EXPECT_EQ(map.GetAllInCell(middle).front(), building);
	map.Reinsert(totem);
	EXPECT_EQ(map.GetAllInCell(middle).front(), totem);
	// It goes out of the cells with its town centre
	registry.Destroy(totem);
	EXPECT_EQ(map.GetAllInCell(middle), std::vector {building});
}

TEST_F(MapProductionFiling, AFireflyIsAtTheFrontOfTheThingsThatMoveInItsCellAndMovesWithIt)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& map = Locator::entitiesMap::value();
	const glm::vec3 here {500.0f, 0.0f, 500.0f};
	const glm::vec3 there {520.0f, 0.0f, 500.0f};
	const auto villager = MakeVillager(registry, here);
	map.FileNew();
	// It goes into the cell of its map position, in front of the things that move, and not in the fixed things
	const auto firefly = registry.Create();
	auto& fly = registry.Assign<Firefly>(firefly);
	fly.at = map_coords::FromMetres({here.x, here.z});
	registry.Assign<Transform>(firefly, here, glm::mat3(1.0f), glm::vec3(1.0f));
	map.FileNew();
	EXPECT_EQ(MobileIn(map, here), (std::vector {firefly, villager}));
	EXPECT_TRUE(map.GetFixedInGridCell(here).empty());
	const auto* resident = registry.TryGet<const MapCellResident>(firefly);
	ASSERT_NE(resident, nullptr);
	EXPECT_EQ(resident->placement, map_cells::Placement::MobileFront);
	EXPECT_FALSE(resident->coversOutline);
	// Its map position, not its metres, gives its cell: it flies into the next cell
	registry.Get<Firefly>(firefly).at = map_coords::FromMetres({there.x, there.z});
	map.Moved(firefly);
	EXPECT_EQ(MobileIn(map, here), std::vector {villager});
	EXPECT_EQ(MobileIn(map, there), std::vector {firefly});
	// It leaves the cells when it is caught
	registry.Destroy(firefly);
	EXPECT_TRUE(MobileIn(map, there).empty());
}

TEST_F(MapProductionFiling, ATownsFlagIsAtTheBackOfTheFixedThingsInItsCellAndGoesThereAgainWhenItMoves)
{
	auto& registry = Locator::entitiesRegistry::value();
	const glm::vec3 place {505.0f, 0.0f, 505.0f};
	Locator::entitiesMap::reset();
	Locator::entitiesMap::emplace<MapProduction>([place](const Registry& /*registry*/, entt::entity /*entity*/) {
		return std::optional(map_cells::OutlineOfBox(place, glm::mat3(1.0f), 1.0f, glm::vec3(0.0f), {1.0f, 1.0f, 1.0f}));
	});
	auto& map = Locator::entitiesMap::value();
	const auto town = registry.Create();
	registry.Assign<Town>(town, 0u);
	const auto flag = registry.Create();
	registry.Assign<TownDesireFlag>(flag, TownDesireFlag {.town = town, .desire = TownDesireInfo::ForFood, .point = 12});
	registry.Assign<Transform>(flag, place, glm::mat3(1.0f), glm::vec3(1.0f));
	map.FileNew();
	const auto* resident = registry.TryGet<const MapCellResident>(flag);
	ASSERT_NE(resident, nullptr);
	EXPECT_EQ(resident->placement, map_cells::Placement::FixedBack);
	// It stands in one cell only, and it does not cover an outline
	EXPECT_FALSE(resident->coversOutline);
	EXPECT_EQ(resident->cells.size(), 1u);
	// A building filed after it goes in front of it
	const auto building = registry.Create();
	registry.Assign<Transform>(building, place, glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Abode>(building, AbodeNumber::Field, 0u, 0u, 0u);
	map.FileNew();
	const auto fixed = map.GetFixedInGridCell(place);
	ASSERT_EQ(fixed.size(), 2u);
	EXPECT_EQ(fixed.front(), building);
	EXPECT_EQ(fixed.back(), flag);
	// A new store takes it to its place in the same cell: it goes to the back of the cell's fixed things again
	map.Refile(flag);
	EXPECT_EQ(map.GetFixedInGridCell(place).back(), flag);
}
