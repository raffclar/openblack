/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Carrying resources and the storage pit (docs/bw1-notes/villagers.md; values worked out in float32 and the
// original's 24-bit float chains): the pick-ups and drops with the town's carried
// totals, the capacities, GetResourceHeld, AddResourceToVillager, SetStateCarriedObject, the load and town-needs speed
// factors, AtStructureAddResource, 31 / 32, GetResourceDropoffPos, 34 at the temporary pot, CheckSatisfyFoodDesire,
// CreateDroppedResource and the drops of VillagerDead. The fixture of test_villager_food.cpp (a fake state table in the
// Locator, scripted draws) and a mock of GetTemporaryResourceStorePotOrPos (a fake villagerStores service).

#define LOCATOR_IMPLEMENTATIONS

#include <cstring>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "Common/GameRandomTesting.h"
#include "Common/RandomNumberManager.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Life.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Life.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/ObjectResources.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/VillagerChildFactory.h"
#include "ECS/Systems/Implementations/VillagerDiscipleJobs.h"
#include "ECS/Systems/Implementations/VillagerRules.h"
#include "ECS/Systems/Implementations/VillagerWorldQueries.h"
#include "ECS/Systems/Implementations/VillagerWorshipCheck.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Town/TownDesire.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerDecide.h"
#include "ECS/Villager/VillagerFood.h"
#include "ECS/Villager/VillagerResources.h"
#include "ECS/Villager/VillagerSatisfy.h"
#include "Enums.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/TestServices.h"
#include "support/VillagerFakes.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace villager = openblack::ecs::villager;
namespace town_desire = openblack::ecs::town_desire;
namespace tq = openblack::ecs::town_queries;

namespace
{
class TestRng final: public RandomNumberManagerInterface
{
	std::mt19937 _generator {1};
	std::mt19937& Generator() override { return _generator; }
	std::optional<std::reference_wrapper<std::mutex>> LockAccess() override { return std::nullopt; }
};

/// A state table where each row returns 1 (no functions): SetTopState sets the state as asked
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

uint32_t Bits(float value)
{
	uint32_t bits = 0;
	std::memcpy(&bits, &value, sizeof(bits));
	return bits;
}

class VillagerResourcesTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::EmplaceMapAndVillagerDefaults();
		auto info = std::make_unique<InfoConstants>();
		auto& v = info->villager.at(0);
		v.hungryForFood = 0.5f;
		v.starvingForFood = 0.25f;
		v.processChecksEvery = 8;
		v.foodReqiredForDinner = 85;
		v.foodNurishmentMultiplier = 1.2f;
		v.pauseForASecondChance = 0.01f;
		v.grownUpAge = 13;
		v.oldAge = 60;
		v.retirementAge = 100;
		v.sex = SexType::Male;
		v.life = 1.0f;
		v.moveState = LivingStates::LivingMoveToPos;
		v.speedGroup.speedDefault = static_cast<SpeedState>(100);
		// GVillagerInfo of info.dat (the same in all 63 records)
		v.maxFoodCarried = 150;
		v.maxWoodCarried = 250;
		v.minWoodToShowGraphic = 50;
		v.minFoodToShowGraphic = 100;
		v.maxTraderFoodCarried = 500;
		v.maxTraderWoodCarried = 500;
		v.baseForTownNeedsSpeedMod = 0.85f;
		v.divisorForTownNeedsSpeedMod = 2.0f;
		v.speedModWhenFullLoadOfWood = 0.75f;
		v.speedModWhenFullLoadOfFood = 0.85f;
		v.lifeWhenWalksWounded = 0.3f;
		v.lifeWhenCrawlsWounded = 0.15f;
		// (not verified) Pine's woodValue
		info->tree.at(static_cast<size_t>(TreeInfo::Pine)).woodValue = 350;
		for (auto& row : info->villagerStateTable)
		{
			row.animation = 385;
			row.field0x4 = -1;
			row.field0x8 = 1.0f;
			row.field0xf0 = 1; // no out-of clip; no row pauses (no draw from SetTopState)
		}
		auto& table = info->villagerStateTable;
		for (const uint32_t s : {31u, 32u, 33u, 34u, 36u, 40u, 117u, 118u, 163u})
		{
			table.at(s).isFinalState = 1;
		}
		table.at(1).field0x14 = 1;
		// the pit's food pile and the temporary food pot: no cap (nextPotForResource 19, AddToPotDirect),
		// potType Pot (SetSize without a mesh, no pile sound)
		for (const auto pile : {PotInfo::StoragePitFoodPile, PotInfo::MagicFood})
		{
			info->pot.at(static_cast<size_t>(pile)).nextPotForResource = static_cast<PotInfo>(19);
		}
		auto& abode = info->abode.at(0);
		abode.abodeNumber = AbodeNumber::A;
		abode.tribeType = Tribe::CELTIC;
		abode.maxVillagersInAbode = 2;
		abode.maxChildrenInAbode = 1;
		abode.thresholdForStopBeingFunctional = 0.75f;
		Locator::infoConstants::reset(info.release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		Locator::rng::emplace<TestRng>();
		Locator::livingActionSystem::emplace<FakeStateTable>();
		game_clock::SetTurn(1000);
		// The villager services the code under test reaches: the game's, unless faked here
		Locator::villagerRules::emplace<ecs::systems::VillagerRules>();
		Locator::villagerWorldQueries::emplace<ecs::systems::VillagerWorldQueries>();
		Locator::villagerWorshipCheck::emplace<ecs::systems::VillagerWorshipCheck>();
		Locator::villagerChildFactory::emplace<ecs::systems::VillagerChildFactory>();
		Locator::villagerDiscipleJobs::emplace<ecs::systems::VillagerDiscipleJobs>();
		game_random::testing::SetGameRand([](uint32_t) { return 0u; }, [](float) { return 0.0f; });
		// no temporary pot unless a test makes one (GetTemporaryResourceStorePotOrPos's "no pile": the point)
		test::FakeVillagerStores stores;
		stores.temporaryStore = [](entt::entity, const map_coords::MapCoords& from, ResourceType) {
			return ecs::town_stores::TemporaryStore {entt::null, from};
		};
		// the dropped logs are recorded, not made (no DeadTree / physics in the tests)
		stores.makeDroppedLog = [this](glm::vec3 position, uint32_t mesh, float multiplier, glm::vec3 velocity, glm::vec3,
		                               std::optional<glm::vec3>) {
			droppedLogs.push_back({position, mesh, multiplier, velocity});
		};
		Locator::villagerStores::emplace<test::FakeVillagerStores>(stores);
	}

	struct DroppedLogRecord
	{
		glm::vec3 position;
		uint32_t mesh;
		float multiplier;
		glm::vec3 velocity;
	};
	std::vector<DroppedLogRecord> droppedLogs;

	void TearDown() override
	{
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
	static GVillagerInfo& Info() { return const_cast<GVillagerInfo&>(Locator::infoConstants::value().villager.at(0)); }

	static entt::entity MakeVillager(glm::vec2 at = {100.0f, 130.0f}, uint32_t top = 163, uint32_t final = 0)
	{
		auto& registry = Reg();
		const auto e = registry.Create();
		auto& v = registry.Assign<Villager>(e);
		v.life = 1.0f;
		v.town = entt::null;
		v.abode = entt::null;
		v.food = 1.0f;
		v.lastCheckTurn = 1000;
		auto& action = registry.Assign<LivingAction>(e, S(top), static_cast<uint16_t>(0));
		action.states.at(1) = static_cast<uint8_t>(final);
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

	static entt::entity MakeAbode(glm::vec2 at, uint32_t food, uint32_t townId = 1, float life = 1.0f)
	{
		auto& registry = Reg();
		const auto abode = registry.Create();
		ecs::object_index::Assign(abode);
		registry.Assign<Abode>(abode, AbodeNumber::A, townId, food, 0u);
		registry.Assign<Transform>(abode, glm::vec3(at.x, 0.0f, at.y), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<Life>(abode, life);
		return abode;
	}

	/// A storage pit (StoragePit + its food pile, a Pot) that is the town's storage pit, at (70, 70)
	static entt::entity MakePit(entt::entity town, uint16_t food)
	{
		auto& registry = Reg();
		const auto pit = MakeAbode({70.0f, 70.0f}, food, registry.Get<Town>(town).id);
		const auto pile = registry.Create();
		auto& pot = registry.Assign<Pot>(pile);
		pot.type = PotInfo::StoragePitFoodPile;
		pot.amount = food;
		registry.Assign<Transform>(pile, glm::vec3(70.0f, 0.0f, 70.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		auto& store = registry.Assign<StoragePit>(pit);
		store.woodPiles.fill(entt::null);
		store.foodPile = pile;
		registry.Get<Town>(town).storagePit = pit;
		return pit;
	}

	/// A stand-in for a temporary pot that keeps what it is given: an object with an Abode component (what
	/// object_resources::AddResource takes today; the pot dispatch belongs to the buildings code)
	static entt::entity MakeMockStore(glm::vec2 at)
	{
		auto& registry = Reg();
		const auto store = registry.Create();
		registry.Assign<Abode>(store, AbodeNumber::A, 99u, 0u, 0u);
		registry.Assign<Transform>(store, glm::vec3(at.x, 0.0f, at.y), glm::mat3(1.0f), glm::vec3(1.0f));
		return store;
	}

	/// The mock GetTemporaryResourceStorePotOrPos: always `pot`, its point `pos`
	static void UseTemporaryStore(entt::entity pot, glm::vec2 pos)
	{
		const auto fixed = tq::ToMapCoords(pos);
		// only the temporary store changes: the fixture's dropped log recorder stays
		auto& stores = static_cast<test::FakeVillagerStores&>(Locator::villagerStores::value());
		stores.temporaryStore = [pot, fixed](entt::entity, const map_coords::MapCoords&, ResourceType) {
			return ecs::town_stores::TemporaryStore {pot, {fixed.x, fixed.y, 0.0f}};
		};
	}

	static LivingAction& Action(entt::entity e) { return Reg().Get<LivingAction>(e); }
	static Villager& V(entt::entity e) { return Reg().Get<Villager>(e); }
	static Town& T(entt::entity town) { return Reg().Get<Town>(town); }
	static uint32_t Top(entt::entity e) { return Action(e).states.at(0); }
	static uint32_t Final(entt::entity e) { return Action(e).states.at(1); }
	static glm::vec2 Goal(entt::entity e) { return Reg().Get<WallHug>(e).goal; }
	static uint32_t TreeBits(entt::entity e)
	{
		return static_cast<uint32_t>((V(e).flags & Villager::k_FlagTreeTypeMask) >> Villager::k_TreeTypeShift);
	}
};
} // namespace

TEST_F(VillagerResourcesTest, PickupDropAndTotals)
{
	const auto town = MakeTown();
	auto a = MakeVillager();
	V(a).town = town;
	// FOOD 120 with a town: held and the town's carried total
	EXPECT_EQ(villager::PickupResource(a, ResourceType::Food, 120, 0), 120);
	EXPECT_EQ(V(a).resourceHeld.at(0), 120);
	EXPECT_FLOAT_EQ(T(town).stats.foodCarried, 120.0f);
	// without a town: held only
	auto b = MakeVillager();
	villager::PickupResource(b, ResourceType::Food, 120, 0);
	EXPECT_EQ(V(b).resourceHeld.at(0), 120);
	// PickupWood(60, 2): the wood held, the town's carried wood, the tree bits
	villager::PickupWood(a, 60, 2);
	EXPECT_EQ(V(a).resourceHeld.at(1), 60);
	EXPECT_FLOAT_EQ(T(town).stats.woodCarried, 60.0f);
	EXPECT_EQ(TreeBits(a), 2u);
	// DropWood(0): all of it; the tree bits stay (literal)
	EXPECT_EQ(villager::DropWood(a, 0), 60u);
	EXPECT_EQ(V(a).resourceHeld.at(1), 0);
	EXPECT_FLOAT_EQ(T(town).stats.woodCarried, 0.0f);
	EXPECT_EQ(TreeBits(a), 2u);
	// PickupWood(10, 0) overwrites the bits with 0; DropWood(500) (above what it carries) -> all
	villager::PickupWood(a, 10, 0);
	EXPECT_EQ(TreeBits(a), 0u);
	EXPECT_EQ(villager::DropWood(a, 500), 10u);
	EXPECT_EQ(V(a).resourceHeld.at(1), 0);
	// DropResource of another type: 0, nothing dropped
	EXPECT_EQ(villager::DropResource(a, static_cast<ResourceType>(2), 5), 0u);
	EXPECT_EQ(villager::DropResource(a, ResourceType::Any, 5), 0u);
	EXPECT_EQ(V(a).resourceHeld.at(0), 120);
	// type -2 picks up wood (anything but FOOD is wood)
	villager::PickupResource(a, ResourceType::Any, 7, 1);
	EXPECT_EQ(V(a).resourceHeld.at(1), 7);
	EXPECT_EQ(TreeBits(a), 1u);
	// DropResource(FOOD, 20)
	EXPECT_EQ(villager::DropResource(a, ResourceType::Food, 20), 20u);
	EXPECT_EQ(V(a).resourceHeld.at(0), 100);
	EXPECT_FLOAT_EQ(T(town).stats.foodCarried, 100.0f);
}

TEST_F(VillagerResourcesTest, Capacities)
{
	auto a = MakeVillager();
	V(a).resourceHeld.at(0) = 74;
	EXPECT_EQ(villager::GetFoodCapacity(a), 76);
	EXPECT_EQ(villager::GetWoodCapacity(a), 250);
	// the double pick-up can leave 170: -20 (16-bit)
	V(a).resourceHeld.at(0) = 170;
	EXPECT_EQ(villager::GetFoodCapacity(a), -20);
}

TEST_F(VillagerResourcesTest, GetResourceHeld)
{
	const auto check = [](int16_t food, int16_t wood, ResourceType type, uint16_t amount) {
		const auto held = villager::HeldLarger(food, wood);
		EXPECT_EQ(held.type, type) << food << " " << wood;
		EXPECT_EQ(held.amount, amount) << food << " " << wood;
	};
	check(0, 0, ResourceType::None, 0);
	check(10, 0, ResourceType::Food, 10);
	check(10, 10, ResourceType::Wood, 10);
	check(11, 10, ResourceType::Food, 11);
	check(0, 5, ResourceType::Wood, 5);
	auto a = MakeVillager();
	V(a).resourceHeld = {120, 60};
	ResourceType type = ResourceType::None;
	EXPECT_EQ(villager::GetResourceHeld(a, type), 120u);
	EXPECT_EQ(type, ResourceType::Food);
}

TEST_F(VillagerResourcesTest, AddResourceToVillager)
{
	auto a = MakeVillager();
	EXPECT_EQ(villager::AddResourceToVillager(a, ResourceType::Food, 30, true), 0u);
	EXPECT_EQ(V(a).resourceHeld.at(0), 30);
	EXPECT_TRUE(ecs::life::IsPoisoned(a));
	auto b = MakeVillager();
	V(b).flags = static_cast<uint16_t>(3u << Villager::k_TreeTypeShift);
	EXPECT_EQ(villager::AddResourceToVillager(b, ResourceType::Wood, 30, false), 0u);
	EXPECT_EQ(V(b).resourceHeld.at(1), 30);
	EXPECT_EQ(TreeBits(b), 0u);
	EXPECT_FALSE(ecs::life::IsPoisoned(b));
	auto c = MakeVillager();
	EXPECT_EQ(villager::AddResourceToVillager(c, static_cast<ResourceType>(2), 30, false), 0u);
	EXPECT_EQ(V(c).resourceHeld.at(0), 0);
	EXPECT_EQ(V(c).resourceHeld.at(1), 0);
}

TEST_F(VillagerResourcesTest, CarriedObjectFor)
{
	villager::CarriedInput in;
	in.finalState = S(163);
	in.topState = S(163);
	in.wood = 51;
	EXPECT_EQ(villager::CarriedObjectFor(in), 12); // WOOD
	for (uint32_t tree = 1; tree <= 3; ++tree)
	{
		in.flags = static_cast<uint16_t>(tree << Villager::k_TreeTypeShift);
		EXPECT_EQ(villager::CarriedObjectFor(in), static_cast<int32_t>(12 + tree)); // TREE_1..3
	}
	in.flags = 0;
	// wood 50 (not above 50), food 101: BAG
	in.wood = 50;
	in.food = 101;
	EXPECT_EQ(villager::CarriedObjectFor(in), 6);
	// food 101 with the final 40 ARRIVES_AT_BUILDING_SITE (exit ExitBuilding): NONE
	EXPECT_TRUE(villager::IsBuildingExitState(S(40)));
	EXPECT_TRUE(villager::IsBuildingExitState(S(189)));
	EXPECT_FALSE(villager::IsBuildingExitState(S(32)));
	in.finalState = S(40);
	in.finalIsBuilding = villager::IsBuildingExitState(in.finalState);
	EXPECT_EQ(villager::CarriedObjectFor(in), 1);
	in.finalState = S(163);
	in.finalIsBuilding = false;
	in.food = 0;
	// life 0.15 (not above LifeWhenCrawlsWounded) with wood 200: NONE; 0.1500001: TREE_1
	in.wood = 200;
	in.flags = static_cast<uint16_t>(1u << Villager::k_TreeTypeShift);
	in.life = 0.15f;
	EXPECT_EQ(villager::CarriedObjectFor(in), 1);
	in.life = 0.1500001f;
	EXPECT_EQ(villager::CarriedObjectFor(in), 13);
	// final 4 IN_SCRIPT or TOP 200 SCRIPT_PLAY_ANIM: the previous object stays
	in.previous = 9;
	in.finalState = S(4);
	EXPECT_EQ(villager::CarriedObjectFor(in), 9);
	in.finalState = S(163);
	in.topState = S(200);
	EXPECT_EQ(villager::CarriedObjectFor(in), 9);
	in.topState = S(163);
	// the final state's row forces 2; the TOP's row (9) wins over it
	in.rowCarriedFinal = 2;
	EXPECT_EQ(villager::CarriedObjectFor(in), 2);
	in.rowCarriedTop = 9;
	EXPECT_EQ(villager::CarriedObjectFor(in), 9);
	// the entity layer: a villager in 163 with wood 120 of tree 0 shows the WOOD log
	auto a = MakeVillager();
	V(a).resourceHeld.at(1) = 120;
	EXPECT_EQ(villager::SetStateCarriedObject(a, 1), 12);
}

TEST_F(VillagerResourcesTest, LoadFactors)
{
	const auto& info = Info();
	EXPECT_EQ(villager::LoadFactors(0, 0, info, false).wood, 1.0f);
	EXPECT_EQ(villager::LoadFactors(187, 0, info, false).wood, 1.0f);
	EXPECT_EQ(Bits(villager::LoadFactors(188, 0, info, false).wood), 0x3F7F7CEEu); // 0.998000026
	EXPECT_EQ(Bits(villager::LoadFactors(200, 0, info, false).wood), 0x3F733333u); // 0.949999988
	EXPECT_EQ(villager::LoadFactors(250, 0, info, false).wood, 0.75f);
	EXPECT_EQ(villager::LoadFactors(300, 0, info, false).wood, 0.75f);
	// foodF in float steps (24-bit float precision)
	EXPECT_EQ(villager::LoadFactors(0, 127, info, false).food, 1.0f);
	EXPECT_EQ(Bits(villager::LoadFactors(0, 128, info, false).food), 0x3F7F258Cu); // 0.99666667
	EXPECT_EQ(Bits(villager::LoadFactors(0, 140, info, false).food), 0x3F6AAAABu); // 0.916666687
	EXPECT_EQ(Bits(villager::LoadFactors(0, 150, info, false).food), 0x3F59999Au); // 0.850000024
	EXPECT_EQ(villager::LoadFactors(0, 170, info, false).food, 0.75f);             // the 0.75 clamp
	// the trader (disciple 9): 500 / 500, so wood 250 gives 1.75 - 0.5 -> 1
	EXPECT_EQ(villager::LoadFactors(250, 0, info, true).wood, 1.0f);
}

TEST_F(VillagerResourcesTest, TownNeeds)
{
	const auto& info = Info();
	TownDesire desire;
	// float steps (24-bit float precision)
	EXPECT_EQ(villager::TownNeedsFactor(town_desire::TownNeedsSum(desire), info), 0.85f);
	desire.raw.at(0) = 1.0f;                                                                          // Food
	desire.raw.at(1) = 1.0f;                                                                          // Wood
	EXPECT_EQ(Bits(town_desire::TownNeedsSum(desire)), 0x3ECCCCCDu);                                  // 0.400000006
	EXPECT_EQ(Bits(villager::TownNeedsFactor(town_desire::TownNeedsSum(desire), info)), 0x3F866667u); // 1.05000007
	// ForPlaytime 2, ForChildren 8, ForRain 10, ForSun 11, Wonder 14, Relaxation 15, Sleep 16 are not summed
	TownDesire others;
	for (const size_t d : {2u, 8u, 10u, 11u, 14u, 15u, 16u})
	{
		others.raw.at(d) = 9.0f;
	}
	EXPECT_EQ(villager::TownNeedsFactor(town_desire::TownNeedsSum(others), info), 0.85f);
	// a sum of 10 (S clamped to 1, the term to 0.5): 1.35; a negative one: 0.85
	TownDesire high;
	high.raw.at(5) = 10.0f;
	EXPECT_EQ(Bits(villager::TownNeedsFactor(town_desire::TownNeedsSum(high), info)), 0x3FACCCCDu); // 1.35000002
	TownDesire low;
	low.raw.at(3) = -3.0f;
	EXPECT_EQ(villager::TownNeedsFactor(town_desire::TownNeedsSum(low), info), 0.85f);
}

TEST_F(VillagerResourcesTest, AtStructureAddResource)
{
	const auto town = MakeTown();
	const auto pit = MakePit(town, 0);
	// farther than the speed: walk to the edge (the arrive point) with the final state, amount 0, 0x24
	auto a = MakeVillager({100.0f, 130.0f}, 32);
	V(a).town = town;
	villager::PickupFood(a, 120);
	uint32_t amount = 120;
	EXPECT_EQ(villager::AtStructureAddResource(a, pit, ResourceType::Food, amount), 0x24u);
	EXPECT_EQ(amount, 0u);
	EXPECT_EQ(Top(a), 1u);
	EXPECT_EQ(Final(a), 32u);
	EXPECT_EQ(Goal(a), tq::ToMetres(tq::PosOf(pit)));
	EXPECT_EQ(V(a).resourceHeld.at(0), 120);
	// within the speed: the pit takes it (object_resources::AddResource), the villager drops it, amount = added, 1
	auto b = MakeVillager({70.0f, 70.0f}, 32);
	V(b).town = town;
	villager::PickupFood(b, 120);
	amount = 120;
	EXPECT_EQ(villager::AtStructureAddResource(b, pit, ResourceType::Food, amount), 1u);
	EXPECT_EQ(amount, 120u);
	EXPECT_EQ(V(b).resourceHeld.at(0), 0);
	// the pit's stock went up (the storage pit's AddResource through object_resources)
	EXPECT_EQ(ecs::object_resources::GetResource(pit, ResourceType::Food), 120u);
	// an object that refuses (AddResource 0): 0, held unchanged
	const auto refuser = Reg().Create();
	Reg().Assign<Transform>(refuser, glm::vec3(100.0f, 0.0f, 130.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	auto c = MakeVillager({100.0f, 130.0f}, 32);
	villager::PickupFood(c, 40);
	amount = 40;
	EXPECT_EQ(villager::AtStructureAddResource(c, refuser, ResourceType::Food, amount), 0u);
	EXPECT_EQ(V(c).resourceHeld.at(0), 40);
	// the home as the "pit": its edge is its centre
	const auto home = MakeAbode({120.0f, 120.0f}, 0, 9);
	auto d = MakeVillager({100.0f, 130.0f}, 32);
	villager::PickupFood(d, 40);
	amount = 40;
	EXPECT_EQ(villager::AtStructureAddResource(d, home, ResourceType::Food, amount), 0x24u);
	EXPECT_EQ(Goal(d), tq::ToMetres(tq::PosOf(home)));
}

TEST_F(VillagerResourcesTest, GotoStoragePitForDropOff)
{
	// the town's functional pit: on to its arrive point with FINAL 32, 1
	const auto town = MakeTown();
	const auto pit = MakePit(town, 0);
	auto a = MakeVillager({100.0f, 130.0f}, 31);
	V(a).town = town;
	V(a).resourceHeld.at(0) = 120;
	EXPECT_EQ(villager::GotoStoragePitForDropOff(a), 1u);
	EXPECT_EQ(Top(a), 1u);
	EXPECT_EQ(Final(a), 32u);
	EXPECT_EQ(Goal(a), tq::ToMetres(tq::PosOf(pit)));
	// no pit, a functional home: to the home
	const auto home = MakeAbode({120.0f, 120.0f}, 0, 9);
	auto b = MakeVillager({100.0f, 130.0f}, 31);
	V(b).abode = home;
	V(b).resourceHeld.at(0) = 120;
	EXPECT_EQ(villager::GotoStoragePitForDropOff(b), 1u);
	EXPECT_EQ(Final(b), 32u);
	EXPECT_EQ(Goal(b), tq::ToMetres(tq::PosOf(home)));
	// neither, holding food: MOVE_TO_POS to the drop-off point (no town: its own position) with FINAL 32
	auto c = MakeVillager({100.0f, 130.0f}, 31);
	V(c).resourceHeld.at(0) = 120;
	EXPECT_EQ(villager::GotoStoragePitForDropOff(c), 1u);
	EXPECT_EQ(Top(c), 1u);
	EXPECT_EQ(Final(c), 32u);
	EXPECT_EQ(Goal(c), tq::ToMetres(tq::PosOf(c)));
	// holding nothing: 163 and 0
	auto d = MakeVillager({100.0f, 130.0f}, 31);
	EXPECT_EQ(villager::GotoStoragePitForDropOff(d), 0u);
	EXPECT_EQ(Top(d), 163u);
	// the state's adapter returns the same
	auto e = MakeVillager({100.0f, 130.0f}, 31);
	EXPECT_EQ(villager::GotoStoragePitForDropOffState(Action(e)), 0u);
}

TEST_F(VillagerResourcesTest, ArrivesAtStoragePitForDropOff)
{
	const auto town = MakeTown();
	const auto pit = MakePit(town, 0);
	// nothing held: 163
	auto a = MakeVillager({70.0f, 70.0f}, 32);
	V(a).town = town;
	EXPECT_EQ(villager::ArrivesAtStoragePitForDropOff(Action(a)), 1u);
	EXPECT_EQ(Top(a), 163u);
	// not there yet: AtStructureAddResource's 0x24, still 1
	auto b = MakeVillager({100.0f, 130.0f}, 32);
	V(b).town = town;
	villager::PickupFood(b, 120);
	EXPECT_EQ(villager::ArrivesAtStoragePitForDropOff(Action(b)), 1u);
	EXPECT_EQ(Top(b), 1u);
	EXPECT_EQ(Final(b), 32u);
	EXPECT_EQ(V(b).resourceHeld.at(0), 120);
	// there with food 120 and wood 60: only the food (the larger) goes, then back to the arrive point with FINAL 163
	auto c = MakeVillager({70.0f, 70.0f}, 32);
	V(c).town = town;
	villager::PickupFood(c, 120);
	villager::PickupWood(c, 60, 0);
	EXPECT_EQ(villager::ArrivesAtStoragePitForDropOff(Action(c)), 1u);
	EXPECT_EQ(V(c).resourceHeld.at(0), 0);
	EXPECT_EQ(V(c).resourceHeld.at(1), 60);
	// only b's 120 is still carried in the town (it has not arrived)
	EXPECT_FLOAT_EQ(T(town).stats.foodCarried, 120.0f);
	EXPECT_EQ(Top(c), 1u);
	EXPECT_EQ(Final(c), 163u);
	EXPECT_EQ(Goal(c), tq::ToMetres(tq::PosOf(pit)));
	// back in 163: wood 60 > 50 sends it to 31 again (the second trip, literal)
	Action(c).states.at(0) = 163;
	Action(c).states.at(1) = 0;
	EXPECT_EQ(villager::CheckTakeResourcesToStoragePit(c), 1u);
	EXPECT_EQ(Top(c), 31u);
	// a town without a pit, a homeless villager: the temporary pot. There (AreWeThere): pot +n, DropWood, 163
	const auto other = MakeTown(2);
	const auto pot = MakeMockStore({100.0f, 130.0f});
	UseTemporaryStore(pot, {100.0f, 130.0f});
	auto d = MakeVillager({100.0f, 130.0f}, 32);
	V(d).town = other;
	villager::PickupWood(d, 120, 0);
	EXPECT_EQ(villager::ArrivesAtStoragePitForDropOff(Action(d)), 1u);
	EXPECT_EQ(Reg().Get<Abode>(pot).woodAmount, 120u);
	EXPECT_EQ(V(d).resourceHeld.at(1), 0);
	EXPECT_FLOAT_EQ(T(other).stats.woodCarried, 0.0f);
	EXPECT_EQ(Top(d), 163u);
	// not there: walk to the pot's point with the final state, 1
	UseTemporaryStore(pot, {130.0f, 130.0f});
	auto e = MakeVillager({100.0f, 130.0f}, 32);
	V(e).town = other;
	villager::PickupFood(e, 120);
	EXPECT_EQ(villager::ArrivesAtStoragePitForDropOff(Action(e)), 1u);
	EXPECT_EQ(Top(e), 1u);
	EXPECT_EQ(Final(e), 32u);
	EXPECT_EQ(V(e).resourceHeld.at(0), 120);
	// without a town (and no home): 163
	auto f = MakeVillager({100.0f, 130.0f}, 32);
	villager::PickupFood(f, 120);
	EXPECT_EQ(villager::ArrivesAtStoragePitForDropOff(Action(f)), 1u);
	EXPECT_EQ(Top(f), 163u);
	EXPECT_EQ(V(f).resourceHeld.at(0), 120);
}

TEST_F(VillagerResourcesTest, GetResourceDropoffPos)
{
	// the town's functional pit: its arrive point
	const auto town = MakeTown();
	const auto pit = MakePit(town, 0);
	auto a = MakeVillager();
	V(a).town = town;
	EXPECT_EQ(villager::GetResourceDropoffPos(a, ResourceType::Food), tq::PosOf(pit));
	// no pit, a functional home: the home's arrive point
	const auto home = MakeAbode({120.0f, 120.0f}, 0, 9);
	auto b = MakeVillager();
	V(b).abode = home;
	EXPECT_EQ(villager::GetResourceDropoffPos(b, ResourceType::Food), tq::PosOf(home));
	// a town without a pit, no home: the temporary pot's edge towards me (the mock)
	const auto other = MakeTown(2);
	const auto pot = MakeMockStore({40.0f, 40.0f});
	UseTemporaryStore(pot, {42.0f, 41.0f});
	auto c = MakeVillager();
	V(c).town = other;
	EXPECT_EQ(villager::GetResourceDropoffPos(c, ResourceType::Wood), tq::ToMapCoords({42.0f, 41.0f}));
	// no town: my position
	auto d = MakeVillager();
	EXPECT_EQ(villager::GetResourceDropoffPos(d, ResourceType::Food), tq::PosOf(d));
}

TEST_F(VillagerResourcesTest, FoodFromNothingAtTheTemporaryPot)
{
	// 34 in a town without a pit, homeless: the (empty, poisoned) pot gives the whole need; no poison (literal)
	const auto town = MakeTown(2);
	const auto pot = Reg().Create();
	Reg().Assign<Transform>(pot, glm::vec3(100.0f, 0.0f, 130.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	auto& p = Reg().Assign<Pot>(pot);
	p.type = PotInfo::MagicFood;
	p.amount = 0;
	p.poisoned = true;
	UseTemporaryStore(pot, {100.0f, 130.0f});
	auto a = MakeVillager({100.0f, 130.0f}, 34);
	V(a).town = town;
	V(a).food = 0.5f;
	ASSERT_EQ(villager::GetAmountOfFoodRequiredForMeal(a), 74u);
	EXPECT_EQ(villager::ArrivesAtStoragePitForFood(Action(a)), 1u);
	EXPECT_EQ(V(a).resourceHeld.at(0), 74);
	EXPECT_EQ(Reg().Get<Pot>(pot).amount, 0);
	EXPECT_EQ(Top(a), 163u);
	EXPECT_FALSE(ecs::life::IsPoisoned(a));
	// not there: walk to the pot's edge with the final state kept (34), 0x24
	Reg().Get<Transform>(pot).position = glm::vec3(150.0f, 0.0f, 130.0f);
	auto b = MakeVillager({100.0f, 130.0f}, 34);
	V(b).town = town;
	V(b).food = 0.5f;
	EXPECT_EQ(villager::ArrivesAtStoragePitForFood(Action(b)), 0x24u);
	EXPECT_EQ(Top(b), 1u);
	EXPECT_EQ(Final(b), 34u);
	EXPECT_EQ(V(b).resourceHeld.at(0), 0);
}

TEST_F(VillagerResourcesTest, CheckSatisfyFoodDesire)
{
	// the formulas in float steps (24-bit float precision)
	EXPECT_EQ(Bits(villager::DropOffScore(74, 150, 0.0f)), 0x3EFC93DBu);   // 0.493315548
	EXPECT_EQ(Bits(villager::DropOffScore(74, 150, 100.0f)), 0x3EFC0AF3u); // 0.492271036
	EXPECT_EQ(villager::DropOffFraction(150, 150), 0.0f);
	EXPECT_EQ(Bits(villager::DropOffFraction(149, 150)), 0x3BDA7400u); // 0.00666666031
	EXPECT_EQ(Bits(villager::DropOffFraction(-20, 150)), 0x3F911110u); // 1.13333321
	// a town with no field, fish farm or flock: nothing held -> 0 (no 31)
	const auto town = MakeTown(2);
	auto a = MakeVillager();
	V(a).town = town;
	EXPECT_EQ(villager::CheckSatisfyFoodDesire(a), 0u);
	EXPECT_EQ(Top(a), 163u);
	// 74 held: the drop-off beats the empty head -> GotoStoragePitForDropOff's 1 (the drop-off point, FINAL 32)
	auto b = MakeVillager();
	V(b).town = town;
	V(b).resourceHeld.at(0) = 74;
	EXPECT_EQ(villager::CheckSatisfyFoodDesire(b), 1u);
	EXPECT_EQ(Top(b), 1u);
	EXPECT_EQ(Final(b), 32u);
	// one field in the town that scores 0 (not functional: GetDesireToBeFarmed 0): the food finders run,
	// the head is {0, null} and the drop-off wins -> 31's walk (FINAL 32)
	const auto field = Reg().Create();
	Reg().Assign<Field>(field, 2);
	auto c = MakeVillager();
	V(c).town = town;
	V(c).resourceHeld.at(0) = 74;
	EXPECT_EQ(villager::CheckSatisfyFoodDesire(c), 1u);
	EXPECT_EQ(Final(c), 32u);
}

TEST_F(VillagerResourcesTest, CreateDroppedResource)
{
	// the pure part: c0 1 or 16, or wood 50, -> nothing; wood 51 with c0 13 -> a log of multiplier float(51 / 350)
	EXPECT_FALSE(villager::DroppedLogFor(1, 200, 50, 350.0f).has_value());
	EXPECT_FALSE(villager::DroppedLogFor(16, 200, 50, 350.0f).has_value());
	EXPECT_FALSE(villager::DroppedLogFor(13, 50, 50, 350.0f).has_value());
	const auto log = villager::DroppedLogFor(13, 51, 50, 350.0f);
	ASSERT_TRUE(log.has_value());
	EXPECT_EQ(log->carriedObject, 13);
	EXPECT_EQ(log->multiplier, static_cast<float>(51.0 / 350.0));
	// the dead tree's default resource: 51 gives back 51 (350 x float(51 / 350) = 50.99999905 rounds to 51.0f under
	// 24-bit float precision), 100 gives 100
	EXPECT_EQ(villager::DroppedLogValue(350, log->multiplier, 1.0f), 51);
	EXPECT_EQ(villager::DroppedLogValue(350, villager::DroppedLogFor(13, 100, 50, 350.0f)->multiplier, 1.0f), 100);
	// the entity: TREE_1 shown, wood 51 -> a log with TREE_1's mesh (347) and float(51 / 350), then DropWood(0)
	const auto town = MakeTown();
	auto a = MakeVillager();
	V(a).town = town;
	Reg().Assign<SkeletalAnimation>(a).carriedObject = 13;
	villager::PickupWood(a, 51, 1);
	villager::CreateDroppedResource(a, std::nullopt, std::nullopt, std::nullopt);
	ASSERT_EQ(droppedLogs.size(), 1u);
	EXPECT_EQ(droppedLogs.at(0).mesh, 347u);
	EXPECT_EQ(droppedLogs.at(0).multiplier, static_cast<float>(51.0 / 350.0));
	EXPECT_EQ(droppedLogs.at(0).velocity, glm::vec3(0.0f));
	EXPECT_EQ(V(a).resourceHeld.at(1), 0);
	EXPECT_FLOAT_EQ(T(town).stats.woodCarried, 0.0f);
	// nothing shown (c0 1): nothing at all
	auto b = MakeVillager();
	villager::PickupWood(b, 120, 0);
	villager::CreateDroppedResource(b, glm::vec3(1.0f), std::nullopt, std::nullopt);
	EXPECT_EQ(V(b).resourceHeld.at(1), 120);
	EXPECT_EQ(droppedLogs.size(), 1u);
}

TEST_F(VillagerResourcesTest, VillagerDeadDrops)
{
	// flag 1: CreateDroppedResource, then DropWood(0) and DropFood(0); the town's carried totals back to 0
	const auto town = MakeTown();
	auto a = MakeVillager();
	V(a).town = town;
	Reg().Assign<SkeletalAnimation>(a).carriedObject = 12;
	villager::PickupWood(a, 60, 0);
	villager::PickupFood(a, 30);
	villager::VillagerDead(a, DeathReason::Starving, PlayerNames::NEUTRAL, 0.0f, 1);
	EXPECT_EQ(V(a).resourceHeld.at(0), 0);
	EXPECT_EQ(V(a).resourceHeld.at(1), 0);
	EXPECT_FLOAT_EQ(T(town).stats.foodCarried, 0.0f);
	EXPECT_FLOAT_EQ(T(town).stats.woodCarried, 0.0f);
	// flag 0: only the drops
	auto b = MakeVillager();
	V(b).town = town;
	villager::PickupWood(b, 20, 0);
	villager::PickupFood(b, 10);
	villager::VillagerDead(b, DeathReason::Starving, PlayerNames::NEUTRAL, 0.0f, 0);
	EXPECT_EQ(V(b).resourceHeld.at(0), 0);
	EXPECT_EQ(V(b).resourceHeld.at(1), 0);
	EXPECT_FLOAT_EQ(T(town).stats.foodCarried, 0.0f);
	EXPECT_FLOAT_EQ(T(town).stats.woodCarried, 0.0f);
}
