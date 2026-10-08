/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The foresters (docs/bw1-notes/villagers.md): the pure rules (the tree-near code, IsTouching),
// VillagerGotoForest's raw 47, GotWoodDecideWhatToDo, 50 / 52 / 53 and ExitForesting. The fixture of
// test_villager_build.cpp (a fake state table, scripted turn and draws) with the builders' mock building side (no building
// site: CheckNeededForBuilding finds none).

#define LOCATOR_IMPLEMENTATIONS

#include <algorithm>
#include <memory>
#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "Common/EventManager.h"
#include "Common/GameRandomTesting.h"
#include "Common/RandomNumberManager.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Components/WallHugMoveState.h"
#include "ECS/Events/CreatureMimicEvents.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/VillagerBuildingSites.h"
#include "ECS/Systems/Implementations/VillagerChildFactory.h"
#include "ECS/Systems/Implementations/VillagerDiscipleJobs.h"
#include "ECS/Systems/Implementations/VillagerRules.h"
#include "ECS/Systems/Implementations/VillagerStores.h"
#include "ECS/Systems/Implementations/VillagerWorldQueries.h"
#include "ECS/Systems/Implementations/VillagerWorshipCheck.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Trees.h"
#include "ECS/Villager/VillagerBuild.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerForester.h"
#include "ECS/Villager/VillagerResources.h"
#include "Enums.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/RestoreService.h"
#include "support/TestServices.h"
#include "support/VillagerFakes.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace villager = openblack::ecs::villager;
using openblack::map_coords::MapCoords;

namespace
{
class TestRng final: public RandomNumberManagerInterface
{
	std::mt19937 _generator {1};
	std::mt19937& Generator() override { return _generator; }
	std::optional<std::reference_wrapper<std::mutex>> LockAccess() override { return std::nullopt; }
};

class FakeStateTable final: public ecs::systems::LivingActionSystemInterface
{
public:
	void Update() override {}
	[[nodiscard]] VillagerStates VillagerGetState(const LivingAction& action, LivingAction::Index index) const override
	{
		return static_cast<VillagerStates>(action.states.at(static_cast<size_t>(index)));
	}
	void VillagerSetState(LivingAction&, LivingAction::Index, VillagerStates, bool) const override {}
	uint32_t VillagerCallState(LivingAction&, LivingAction::Index) const override { return 0; }
	uint32_t VillagerCallEntry(LivingAction&, VillagerStates, VillagerStates, VillagerStates) const override { return 1; }
	uint32_t VillagerCallExit(LivingAction&, VillagerStates, VillagerStates) const override { return 1; }
	int VillagerCallOutOfAnimation(LivingAction&, LivingAction::Index) const override { return -1; }
	bool VillagerCallValidate(LivingAction&, LivingAction::Index) const override { return false; }
};

constexpr auto S(uint32_t n)
{
	return static_cast<VillagerStates>(n);
}

class VillagerForesterTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::EmplaceMapAndVillagerDefaults();
		auto info = std::make_unique<InfoConstants>();
		auto& v = info->villager.at(0);
		v.hungryForFood = 0.5f;
		v.pauseForASecondChance = 0.01f;
		v.grownUpAge = 13;
		v.life = 1.0f;
		v.moveState = LivingStates::LivingMoveToPos;
		v.speedGroup.speedDefault = static_cast<SpeedState>(100);
		v.maxFoodCarried = 150;
		v.maxWoodCarried = 250;
		// the speed table of info.dat (every Celtic record): the speed's town term (0.85 + clamp(needs / 2, 0, 0.5)),
		// the load terms and the wounded thresholds. The land balance speed is land_balance's default 1
		v.baseForTownNeedsSpeedMod = 0.85f;
		v.divisorForTownNeedsSpeedMod = 2.0f;
		v.speedModWhenFullLoadOfWood = 0.75f;
		v.speedModWhenFullLoadOfFood = 0.85f;
		v.lifeWhenWalksWounded = 0.3f;
		v.lifeWhenCrawlsWounded = 0.15f;
		for (auto& row : info->villagerStateTable)
		{
			row.animation = 385;
			row.field0x4 = -1;
			row.field0x8 = 1.0f;
			row.field0xf0 = 1;
		}
		for (const uint32_t s : {31u, 40u, 49u, 50u, 52u, 53u, 163u})
		{
			info->villagerStateTable.at(s).isFinalState = 1;
		}
		info->villagerStateTable.at(1).field0x14 = 1;
		info->town.maxDistanceForTownForest = 250.0f;
		// every tree row of info.dat is a FOREST_TREE (map_cells::TypeOf reads the row's type)
		for (auto& tree : info->tree)
		{
			tree.type = ObjectType::ForestTree;
		}
		Locator::infoConstants::reset(info.release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		Locator::rng::emplace<TestRng>();
		Locator::livingActionSystem::emplace<FakeStateTable>();
		ecs::map_cells::Clear();
		ecs::object_index::OnLoadMap();
		game_clock::SetTurn(1000);
		// The villager services the code under test reaches: the game's, unless faked here
		Locator::villagerRules::emplace<ecs::systems::VillagerRules>();
		Locator::villagerWorldQueries::emplace<ecs::systems::VillagerWorldQueries>();
		Locator::villagerWorshipCheck::emplace<ecs::systems::VillagerWorshipCheck>();
		Locator::villagerChildFactory::emplace<ecs::systems::VillagerChildFactory>();
		Locator::villagerDiscipleJobs::emplace<ecs::systems::VillagerDiscipleJobs>();
		game_random::testing::SetGameRand([](uint32_t) { return 0u; }, [](float) { return 0.0f; });
		test::FakeVillagerStores stores(std::make_shared<ecs::systems::VillagerStores>());
		stores.temporaryStore = [](entt::entity, const MapCoords& from, ResourceType) {
			return ecs::town_stores::TemporaryStore {entt::null, from};
		};
		Locator::villagerStores::emplace<test::FakeVillagerStores>(stores);
		// The builders' building side: no site anywhere
		test::FakeVillagerBuildingSites ops(std::make_shared<ecs::systems::VillagerBuildingSites>());
		ops.isBuildingHappening = [](entt::entity) { return false; };
		ops.isBuildingSiteValid = [](entt::entity, entt::entity) { return false; };
		Locator::villagerBuildingSites::emplace<test::FakeVillagerBuildingSites>(ops);
	}

	void TearDown() override
	{
		ecs::ClearForests();
		ecs::map_cells::Clear();
		game_random::testing::SetGameRand({}, {});
		Locator::villagerDiscipleJobs::reset();
		Locator::villagerChildFactory::reset();
		Locator::villagerWorshipCheck::reset();
		Locator::villagerWorldQueries::reset();
		Locator::villagerRules::reset();
		Locator::livingActionSystem::reset();
		Locator::rng::reset();
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
		test::ResetMapAndVillagerDefaults();
		Locator::infoConstants::reset();
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	static entt::entity MakeVillager(glm::vec2 at = {100.0f, 130.0f}, uint32_t top = 163)
	{
		auto& registry = Reg();
		const auto e = registry.Create();
		auto& v = registry.Assign<Villager>(e);
		v.life = 1.0f;
		v.town = entt::null;
		v.abode = entt::null;
		v.food = 1.0f;
		v.lastCheckTurn = 1000;
		registry.Assign<LivingAction>(e, S(top), static_cast<uint16_t>(0));
		registry.Assign<Transform>(e, glm::vec3(at.x, 0.0f, at.y), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<WallHug>(e, glm::vec2(), glm::vec2(), 0.0f, 0.1f);
		registry.Assign<Mobile>(e);
		return e;
	}

	static entt::entity MakeTown(uint32_t id = 1)
	{
		auto& registry = Reg();
		const auto town = registry.Create();
		registry.Assign<Town>(town, id);
		registry.Assign<Tribe>(town, Tribe::CELTIC);
		registry.Assign<Transform>(town, glm::vec3(50.0f, 0.0f, 50.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Context().towns[id] = town;
		return town;
	}

	/// A forest tree in the map cells (FIND_TYPE 6 of its cell)
	static entt::entity AddTree(glm::vec2 at, uint32_t forestId)
	{
		auto& registry = Reg();
		const auto e = registry.Create();
		ecs::object_index::Assign(e);
		registry.Assign<Transform>(e, glm::vec3(at.x, 0.0f, at.y), glm::mat3(1.0f), glm::vec3(1.0f));
		auto& tree = registry.Assign<Tree>(e);
		tree.type = TreeInfo::Pine;
		tree.maxSize = 1.0f;
		tree.forestId = forestId;
		ecs::map_cells::InsertMapObject(e);
		return e;
	}

	static LivingAction& Action(entt::entity e) { return Reg().Get<LivingAction>(e); }
	static Villager& V(entt::entity e) { return Reg().Get<Villager>(e); }
	static uint32_t Top(entt::entity e) { return Action(e).states.at(0); }
	static uint32_t Final(entt::entity e) { return Action(e).states.at(1); }
};
} // namespace

TEST(VillagerForesterPure, Rules)
{
	// the tree-near code: 0 with no tree, 1 with one, 10 when also touching it
	EXPECT_EQ(villager::TreeSearchResult(false, false), 0u);
	EXPECT_EQ(villager::TreeSearchResult(true, false), 1u);
	EXPECT_EQ(villager::TreeSearchResult(true, true), 10u);
	// IsTouching: the speed above the distance (equal is not touching)
	EXPECT_TRUE(villager::TouchingRule(0.05f, 0.1f));
	EXPECT_FALSE(villager::TouchingRule(0.1f, 0.1f));
	EXPECT_FALSE(villager::TouchingRule(0.2f, 0.1f));
}

TEST_F(VillagerForesterTest, VillagerGotoForestRaw47)
{
	const auto v = MakeVillager();
	const auto forest = ecs::CreateForest(0, glm::vec3(120.0f, 0.0f, 130.0f));
	Action(v).turnsSinceStateChange = 7;
	// no tree in it: the forest's centre; SetupMoveToWithHug(p, 49) then the raw TOP 47 (turns since the state
	// change = 0), FINAL 49 kept
	EXPECT_EQ(villager::VillagerGotoForest(v, forest, S(49)), 1u);
	EXPECT_EQ(Top(v), 47u);
	EXPECT_EQ(Final(v), 49u);
	EXPECT_EQ(Action(v).turnsSinceStateChange, 0u);
	EXPECT_EQ(Reg().Get<WallHug>(v).goal, glm::vec2(120.0f, 130.0f));
	// 47's look-ahead does not run (the wall-hug move byte is 0 in the game): 1, nothing else
	EXPECT_EQ(villager::ForesterMoveToForest(Action(v), 7), 1u);
	EXPECT_EQ(Top(v), 47u);
	// ExitForesting: the target thing is cleared
	V(v).targetThing = v;
	EXPECT_EQ(villager::ExitForesting(Action(v), S(163)), 1u);
	EXPECT_EQ(V(v).targetThing, entt::entity(entt::null));
}

TEST_F(VillagerForesterTest, GotWoodDecideWhatToDo)
{
	const auto town = MakeTown();
	const auto v = MakeVillager();
	V(v).town = town;
	// no wood: 163
	EXPECT_EQ(villager::GotWoodDecideWhatToDo(v), 1u);
	EXPECT_EQ(Top(v), 163u);
	// wood and no building site: 31 (the storage pit)
	V(v).resourceHeld.at(1) = 120;
	EXPECT_EQ(villager::GotWoodDecideWhatToDo(v), 1u);
	EXPECT_EQ(Top(v), 31u);
	// 52: the same through ForesterFinishedForestering; no wood -> 163
	Action(v).states.at(0) = 52;
	EXPECT_EQ(villager::ForesterFinishedForestering(Action(v)), 1u);
	EXPECT_EQ(Top(v), 31u);
	V(v).resourceHeld.at(1) = 0;
	Action(v).states.at(0) = 52;
	EXPECT_EQ(villager::ForesterFinishedForestering(Action(v)), 1u);
	EXPECT_EQ(Top(v), 163u);
}

TEST_F(VillagerForesterTest, GotWoodDecideWhatToDoBuildingSite)
{
	// the building-site branch, through the builders' building side (a fake): a valid building site that needs builders ->
	// GotoBuildingSite (the site kept, the ring walk with FINAL 40), 1; one that does not -> CheckNeededForBuilding
	// (no building happening: 0) -> 31
	const auto town = MakeTown();
	const auto v = MakeVillager();
	const auto site = Reg().Create();
	bool needsBuilders = true;
	test::FakeVillagerBuildingSites ops(std::make_shared<ecs::systems::VillagerBuildingSites>());
	ops.isBuildingHappening = [](entt::entity) { return false; };
	ops.isBuildingSiteValid = [site](entt::entity, entt::entity s) { return s == site; };
	ops.needsBuilders = [&needsBuilders](entt::entity) { return needsBuilders; };
	ops.isBuilder = [](entt::entity, entt::entity) { return false; };
	ops.getBuilding = [](entt::entity) { return entt::entity {entt::null}; };
	ops.getRandomBuildPos = [](entt::entity, entt::entity, int32_t& index) {
		index = 3;
		const auto fixed = ecs::town_queries::ToMapCoords(glm::vec2(105.0f, 130.0f));
		return MapCoords {fixed.x, fixed.y, 0.0f};
	};
	Locator::villagerBuildingSites::emplace<test::FakeVillagerBuildingSites>(ops);
	V(v).town = town;
	V(v).buildingSite = site;
	V(v).resourceHeld.at(1) = 120;
	EXPECT_EQ(villager::GotWoodDecideWhatToDo(v), 1u);
	EXPECT_EQ(V(v).buildingSite, site);
	EXPECT_EQ(V(v).buildPosIndex, 3);
	EXPECT_EQ(Final(v), 40u);
	// the site no longer needs builders (and the villager is no BUILDER disciple): the storage pit
	needsBuilders = false;
	Action(v).states.at(0) = 52;
	EXPECT_EQ(villager::GotWoodDecideWhatToDo(v), 1u);
	EXPECT_EQ(Top(v), 31u);
}

TEST_F(VillagerForesterTest, ChopsTreeLanded)
{
	const auto v = MakeVillager({100.0f, 130.0f}, 50);
	// the felled log is no longer in the physics: the target thing is cleared, 52
	const auto log = Reg().Create();
	Reg().Assign<Transform>(log, glm::vec3(102.0f, 0.0f, 130.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	V(v).targetThing = log;
	EXPECT_EQ(villager::ForesterChopsTree(Action(v)), 1u);
	EXPECT_EQ(V(v).targetThing, entt::entity(entt::null));
	EXPECT_EQ(Top(v), 52u);
}

TEST_F(VillagerForesterTest, ArrivesAtBigForestWithoutTown)
{
	const auto v = MakeVillager({100.0f, 130.0f}, 53);
	EXPECT_EQ(villager::ArrivesAtBigForest(Action(v)), 1u);
	EXPECT_EQ(Top(v), 163u);
	// the trivial rows
	EXPECT_EQ(villager::ArrivesAtBigForestForBuilding(Action(v)), 1u);
	EXPECT_EQ(villager::TakeWoodFromPot(Action(v)), 1u);
	Action(v).states.at(0) = 51;
	EXPECT_EQ(villager::ForesterChopsTreeForBuilding(Action(v)), 1u);
	EXPECT_EQ(Top(v), 163u);
}

// ---- the forester states -------------------------------------------------------------------------------------------

TEST_F(VillagerForesterTest, DecideHowToGetWood)
{
	const auto town = MakeTown();
	const auto v = MakeVillager();
	V(v).town = town;
	// the store is the villager's own point (the mock temporary store), so its GDM is 1. No forest: 0
	EXPECT_EQ(villager::DecideHowToGetWood(v, false).how, 0u);
	// a forest 20 m away with one tree (FindForest: the town list is empty)
	const auto forest = ecs::CreateForest(0, glm::vec3(120.0f, 0.0f, 130.0f));
	AddTree({125.0f, 135.0f}, forest);
	// wood 0: frac 0 -> store 0, forest 1 x GDM(20, 250) -> 3 with the forest
	auto source = villager::DecideHowToGetWood(v, false);
	EXPECT_EQ(source.how, 3u);
	ASSERT_TRUE(source.forest.has_value());
	EXPECT_EQ(*source.forest, forest);
	EXPECT_EQ(source.store, 0.0f);
	EXPECT_GT(source.forestScore, 0.0f);
	// wood 250 (capacity 0): frac 1 -> the store wins (store > forest) -> 1
	V(v).resourceHeld.at(1) = 250;
	EXPECT_EQ(villager::DecideHowToGetWood(v, false).how, 1u);
	// builder mode (1): the store 1 only above the capacity (stock 0 here: 0), the forest 0.5 -> 3
	V(v).resourceHeld.at(1) = 0;
	EXPECT_EQ(villager::DecideHowToGetWood(v, true).how, 3u);
	// the forest's big forest set -> 2 with it, no forest out
	const auto big = Reg().Create();
	ecs::SetForestBigForest(forest, big);
	source = villager::DecideHowToGetWood(v, false);
	EXPECT_EQ(source.how, 2u);
	EXPECT_EQ(source.bigForest, big);
	EXPECT_FALSE(source.forest.has_value());
}

TEST_F(VillagerForesterTest, ArrivesAtForestTouchingWalkingAndEmpty)
{
	// 49 with a tree in the 9 cells, not touching: SetupMoveToWithHug(its working point, 49)
	const auto v = MakeVillager({100.0f, 130.0f}, 49);
	const auto tree = AddTree({107.0f, 130.0f}, 0);
	const auto working = ecs::TreeWorkingPos(tree, v);
	EXPECT_EQ(villager::ForesterArrivesAtForest(v), 1u);
	EXPECT_EQ(Final(v), 49u);
	EXPECT_EQ(Reg().Get<WallHug>(v).goal, glm::vec2(working.x, working.z));
	// standing on the working point (the speed above the distance): touching -> the target thing is cleared, the
	// turn and PlayAnimThenSetState(50): TOP 23 WAIT_FOR_ANIMATION, FINAL 50
	Reg().Get<Transform>(v).position = glm::vec3(working.x, 0.0f, working.z);
	V(v).targetThing = tree;
	Action(v).states.at(0) = 49;
	EXPECT_EQ(villager::ForesterArrivesAtForest(v), 1u);
	EXPECT_EQ(V(v).targetThing, entt::entity(entt::null));
	EXPECT_EQ(Top(v), 23u);
	EXPECT_EQ(Final(v), 50u);
	// no tree near and an empty forest within 50 m: the forest is deleted, then 52 -> no wood -> 163
	ecs::map_cells::RemoveMapObject(tree);
	const auto empty = ecs::CreateForest(0, glm::vec3(130.0f, 0.0f, 130.0f));
	const auto far = ecs::CreateForest(0, glm::vec3(300.0f, 0.0f, 300.0f));
	Action(v).states.at(0) = 49;
	EXPECT_EQ(villager::ForesterArrivesAtForest(v), 1u);
	EXPECT_FALSE(ecs::IsInForest(empty));
	EXPECT_TRUE(ecs::IsInForest(far));
	EXPECT_EQ(Top(v), 163u);
}

TEST_F(VillagerForesterTest, MoveToForestLookAhead)
{
	// the wall-hug move byte == 2 (never in the game) and MoveToPos 7: three probes 10 m out, at the heading and a
	// step either side of it; the nearest forest tree of the probe cells to its probe -> SetupMoveToWithHug(its
	// working point, 49)
	const auto v = MakeVillager({105.0f, 135.0f}, 47);
	const auto forest = ecs::CreateForest(0, glm::vec3(105.0f, 0.0f, 135.0f));
	std::vector<entt::entity> trees;
	for (const auto at :
	     {glm::vec2(115.0f, 135.0f), glm::vec2(95.0f, 135.0f), glm::vec2(105.0f, 145.0f), glm::vec2(105.0f, 125.0f)})
	{
		trees.push_back(AddTree(at, forest));
	}
	Reg().AssignOrReplace<WallHugMoveState>(v, uint8_t {2});
	EXPECT_EQ(villager::ForesterMoveToForest(Action(v), 7), 1u);
	Reg().Remove<WallHugMoveState>(v);
	EXPECT_EQ(Final(v), 49u);
	const auto goal = Reg().Get<WallHug>(v).goal;
	EXPECT_TRUE(std::any_of(trees.begin(), trees.end(), [goal, v](entt::entity tree) {
		const auto working = ecs::TreeWorkingPos(tree, v);
		return goal == glm::vec2(working.x, working.z);
	}));
	// any other move result: nothing
	Action(v).states.at(1) = 0;
	Reg().AssignOrReplace<WallHugMoveState>(v, uint8_t {2});
	EXPECT_EQ(villager::ForesterMoveToForest(Action(v), 1), 1u);
	Reg().Remove<WallHugMoveState>(v);
	EXPECT_EQ(Final(v), 0u);
}

TEST_F(VillagerForesterTest, ChopsTreeNoTree)
{
	// 50 with no target thing and no tree near: nothing felled; the target thing stays cleared, 52
	const auto v = MakeVillager({100.0f, 130.0f}, 50);
	EXPECT_EQ(villager::ForesterChopsTree(Action(v)), 1u);
	EXPECT_EQ(V(v).targetThing, entt::entity(entt::null));
	EXPECT_EQ(Top(v), 52u);
}

TEST_F(VillagerForesterTest, ArrivesAtBigForest)
{
	const auto town = MakeTown();
	const auto forest = ecs::CreateForest(0, glm::vec3(110.0f, 0.0f, 130.0f));
	const auto big = Reg().Create();
	Reg().Assign<Transform>(big, glm::vec3(110.0f, 0.0f, 130.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	auto& b = Reg().Assign<BigForest>(big);
	b.wood = 1000.0f;
	b.woodValue = 1000.0f;
	b.forestId = forest;
	ecs::SetForestBigForest(forest, big);
	ecs::AssignForestsToTown(1, glm::vec3(50.0f, 0.0f, 50.0f));
	ASSERT_EQ(ecs::TownForests(1).size(), 1u);
	// 53 away from the arrive point: the walk there with the FINAL kept (53)
	const auto v = MakeVillager({100.0f, 130.0f}, 53);
	V(v).town = town;
	EXPECT_EQ(villager::ArrivesAtBigForest(Action(v)), 1u);
	EXPECT_EQ(Final(v), 53u);
	const auto arrive = ecs::BigForestArrivePos(big, v);
	EXPECT_EQ(Reg().Get<WallHug>(v).goal, glm::vec2(arrive.x, arrive.z));
	EXPECT_EQ(V(v).resourceHeld.at(1), 0);
	// there: RemoveResource(WOOD, capacity 250) from the BigForest, PickupWood, GotWoodDecideWhatToDo (no site: 31),
	// then TOP 163 all the same
	Reg().Get<Transform>(v).position = glm::vec3(arrive.x, 0.0f, arrive.z);
	Action(v).states.at(0) = 53;
	EXPECT_EQ(villager::ArrivesAtBigForest(Action(v)), 1u);
	EXPECT_EQ(V(v).resourceHeld.at(1), 250);
	EXPECT_FLOAT_EQ(Reg().Get<const BigForest>(big).wood, 750.0f);
	EXPECT_EQ(Top(v), 163u);
}

TEST_F(VillagerForesterTest, TakingWoodFromATreeSharesTheTownsNeedForWoodWithItsPlayer)
{
	const test::RestoreService<Locator::events> restoreEvents;
	Locator::events::emplace<EventManager>();
	std::vector<ecs::events::CreatureEmpathyWithTownDesire> needs;
	Locator::events::value().AddHandler<ecs::events::CreatureEmpathyWithTownDesire>(
	    [&needs](const ecs::events::CreatureEmpathyWithTownDesire& event) { needs.push_back(event); });
	const auto town = MakeTown();
	Reg().Get<Town>(town).owner = PlayerNames::PLAYER_ONE;
	const auto v = MakeVillager({100.0f, 130.0f}, 49);
	V(v).town = town;
	const auto at = Reg().Get<const Transform>(v).position;
	// no tree near: back to deciding, nothing shared
	EXPECT_EQ(villager::TakeWoodFromTree(Action(v)), 1u);
	EXPECT_EQ(Top(v), 163u);
	EXPECT_TRUE(needs.empty());
	// a tree near: the need for wood, 0.5, where the villager is, once, as it goes to the tree
	AddTree({107.0f, 130.0f}, 0);
	villager::TakeWoodFromTree(Action(v));
	ASSERT_EQ(needs.size(), 1u);
	EXPECT_EQ(needs.front().player, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(needs.front().desire, TownDesireInfo::ForWood);
	EXPECT_FLOAT_EQ(needs.front().weight, 0.5f);
	EXPECT_EQ(needs.front().point, at);
	// a villager with no town: no player, nothing shared
	const auto loner = MakeVillager({100.0f, 130.0f}, 49);
	villager::TakeWoodFromTree(Action(loner));
	EXPECT_EQ(needs.size(), 1u);
}
