/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The food jobs (docs/bw1-notes/villagers.md): the pure
// layer (the field / fish-farm scores, the catch), CheckSatisfyFoodDesire's ordering, the farmer (SetFarmerGotoField,
// 67 / 68 / 69, EnterFarming / ExitFarming) and the fisherman (VillagerBecomesFisherman, 55 / 56, EnterFishing /
// ExitFishing). The fixture of test_villager_build.cpp (a fake state table in the Locator, scripted draws, the
// temporary store mock) and mock field / fish-farm sides (fake villagerFields / villagerFishFarms services).

#define LOCATOR_IMPLEMENTATIONS

#include <algorithm>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "Common/EventManager.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandomTesting.h"
#include "Common/RandomNumberManager.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Events/CreatureMimicEvents.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/VillagerChildFactory.h"
#include "ECS/Systems/Implementations/VillagerDiscipleJobs.h"
#include "ECS/Systems/Implementations/VillagerRules.h"
#include "ECS/Systems/Implementations/VillagerStores.h"
#include "ECS/Systems/Implementations/VillagerWorldQueries.h"
#include "ECS/Systems/Implementations/VillagerWorshipCheck.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerFarmer.h"
#include "ECS/Villager/VillagerFisherman.h"
#include "ECS/Villager/VillagerInteract.h"
#include "ECS/Villager/VillagerResources.h"
#include "ECS/Villager/VillagerSatisfy.h"
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
namespace tq = openblack::ecs::town_queries;
using openblack::map_coords::MapCoords;

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

MapCoords Coords(glm::vec2 metres)
{
	const auto fixed = tq::ToMapCoords(metres);
	return {fixed.x, fixed.y, 0.0f};
}

struct MockField
{
	int activity {1};
	float desire {0.0f};
	bool ripe {false};
	bool plantOk {true};
	bool stillSowing {true};
	int32_t removeResult {0};
	float lastRemove {0.0f};
	bool isField {true};
	std::vector<entt::entity> farmers;
};

struct MockFarm
{
	int32_t score {1};
	std::vector<entt::entity> fishermen;
	bool available {true};
	MapCoords spot;
};

class VillagerFarmingTest: public ::testing::Test
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
		v.pauseForASecondChance = 0.01f;
		v.grownUpAge = 13;
		v.oldAge = 60;
		v.retirementAge = 100;
		v.sex = SexType::Male;
		v.life = 1.0f;
		v.moveState = LivingStates::LivingMoveToPos;
		v.speedGroup.speedDefault = static_cast<SpeedState>(100);
		v.maxFoodCarried = 150;
		v.maxWoodCarried = 250;
		v.minWoodToShowGraphic = 50;
		v.minFoodToShowGraphic = 100;
		for (auto& row : info->villagerStateTable)
		{
			row.animation = 385;
			row.field0x4 = -1;
			row.field0x8 = 1.0f;
			row.field0xf0 = 1; // no out-of clip; no row pauses (no draw from SetTopState)
		}
		auto& table = info->villagerStateTable;
		for (const uint32_t s : {23u, 31u, 32u, 55u, 56u, 67u, 68u, 69u, 163u})
		{
			table.at(s).isFinalState = 1;
		}
		table.at(1).field0x14 = 1;
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
		game_random::testing::SetGameRand(
		    [this](uint32_t n) {
			    calls.push_back("rand " + std::to_string(n));
			    return roll;
		    },
		    [](float) { return 0.0f; });
		test::FakeVillagerStores stores(std::make_shared<ecs::systems::VillagerStores>());
		stores.temporaryStore = [](entt::entity, const MapCoords& from, ResourceType) {
			return ecs::town_stores::TemporaryStore {entt::null, from};
		};
		Locator::villagerStores::emplace<test::FakeVillagerStores>(stores);
		Locator::villagerFields::emplace<test::FakeVillagerFields>(FieldMocks());
		auto farmMocks = FarmMocks();
		farmMocks.season = [this]() { return season; };
		farmMocks.tribalPower = [this](entt::entity) { return tribal; };
		Locator::villagerFishFarms::emplace<test::FakeVillagerFishFarms>(farmMocks);
	}

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

	// ---- the mock field / fish farm sides ----

	test::FakeVillagerFields FieldMocks()
	{
		test::FakeVillagerFields ops;
		ops.townFields = [this](entt::entity) {
			calls.emplace_back("fields");
			return fieldOrder;
		};
		ops.getDesireToBeFarmed = [this](entt::entity f) { return fields.at(f).desire; };
		ops.getFieldActivity = [this](entt::entity f) {
			calls.emplace_back("activity");
			return fields.at(f).activity;
		};
		ops.getArrivePos = [this](entt::entity f) {
			calls.emplace_back("arrive");
			return CoordsOf(f);
		};
		ops.randomFarmPoint = [this](entt::entity) {
			calls.emplace_back("random");
			const auto p = points.empty() ? MapCoords {} : points.front();
			if (!points.empty())
			{
				points.pop_front();
			}
			return p;
		};
		ops.ripeFarmPoint = [this](entt::entity f, MapCoords& out) {
			calls.emplace_back("ripe");
			if (!fields.at(f).ripe)
			{
				return false;
			}
			calls.emplace_back("random");
			calls.emplace_back("random");
			out = MapCoords {};
			return true;
		};
		ops.plantCrop = [this](entt::entity f) { return fields.at(f).plantOk; };
		ops.isStillSowing = [this](entt::entity f) { return fields.at(f).stillSowing; };
		ops.removeFood = [this](entt::entity f, float amount) {
			fields.at(f).lastRemove = amount;
			return fields.at(f).removeResult;
		};
		ops.addFarmer = [this](entt::entity f, entt::entity v) {
			calls.emplace_back("add farmer");
			auto& list = fields.at(f).farmers;
			if (std::find(list.begin(), list.end(), v) == list.end())
			{
				list.insert(list.begin(), v);
			}
		};
		ops.removeFarmer = [this](entt::entity f, entt::entity v) {
			calls.emplace_back("remove farmer");
			auto& list = fields.at(f).farmers;
			list.erase(std::remove(list.begin(), list.end(), v), list.end());
			villager::SetTargetThing(v, entt::null); // the target is always cleared
		};
		ops.isField = [this](entt::entity f) { return fields.contains(f) && fields.at(f).isField; };
		return ops;
	}

	test::FakeVillagerFishFarms FarmMocks()
	{
		test::FakeVillagerFishFarms ops;
		ops.townFishFarms = [this](entt::entity) {
			calls.emplace_back("farms");
			return farmOrder;
		};
		ops.score = [this](entt::entity f) { return farms.at(f).score; };
		ops.getArrivePos = [this](entt::entity f) {
			calls.emplace_back("farm arrive");
			return CoordsOf(f);
		};
		ops.fishingSpot = [this](entt::entity f) {
			calls.emplace_back("spot");
			return farms.at(f).spot;
		};
		ops.fishermanCount = [this](entt::entity f) { return static_cast<uint32_t>(farms.at(f).fishermen.size()); };
		ops.hasFisherman = [this](entt::entity f, entt::entity v) {
			const auto& list = farms.at(f).fishermen;
			return std::find(list.begin(), list.end(), v) != list.end();
		};
		ops.addFisherman = [this](entt::entity f, entt::entity v) {
			calls.emplace_back("add fisherman");
			farms.at(f).fishermen.insert(farms.at(f).fishermen.begin(), v);
			villager::SetTargetThing(v, f); // the target becomes the farm
		};
		ops.removeFisherman = [this](entt::entity f, entt::entity v) {
			calls.emplace_back("remove fisherman");
			auto& list = farms.at(f).fishermen;
			list.erase(std::remove(list.begin(), list.end(), v), list.end());
		};
		ops.isAvailable = [this](entt::entity f) { return farms.contains(f) && farms.at(f).available; };
		return ops;
	}

	std::map<entt::entity, MockField> fields;
	std::vector<entt::entity> fieldOrder;
	std::map<entt::entity, MockFarm> farms;
	std::vector<entt::entity> farmOrder;
	std::deque<MapCoords> points;
	std::vector<std::string> calls;
	uint32_t roll {0};
	uint32_t season {0};
	float tribal {1.0f};

	size_t Count(const std::string& call) const { return static_cast<size_t>(std::count(calls.begin(), calls.end(), call)); }

	// ---- the entities ----

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	static MapCoords CoordsOf(entt::entity e)
	{
		const auto p = tq::PosOf(e);
		return {p.x, p.y, 0.0f};
	}

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

	static entt::entity MakeThing(glm::vec2 at)
	{
		auto& registry = Reg();
		const auto e = registry.Create();
		registry.Assign<Transform>(e, glm::vec3(at.x, 0.0f, at.y), glm::mat3(1.0f), glm::vec3(1.0f));
		return e;
	}

	entt::entity AddField(glm::vec2 at, float desire, int activity = 1)
	{
		const auto f = MakeThing(at);
		fields[f].desire = desire;
		fields[f].activity = activity;
		fieldOrder.push_back(f);
		return f;
	}

	entt::entity AddFarm(glm::vec2 at, int32_t score = 1)
	{
		const auto f = MakeThing(at);
		farms[f].score = score;
		farmOrder.push_back(f);
		return f;
	}

	static LivingAction& Action(entt::entity e) { return Reg().Get<LivingAction>(e); }
	static Villager& V(entt::entity e) { return Reg().Get<Villager>(e); }
	static uint32_t Top(entt::entity e) { return Action(e).states.at(0); }
	static uint32_t Final(entt::entity e) { return Action(e).states.at(1); }
	static glm::vec2 Goal(entt::entity e) { return Reg().Get<WallHug>(e).goal; }
};
} // namespace

// ---- the pure layer ----------------------------------------------------------------------------------------------

TEST(VillagerFarmingPure, Scores)
{
	// FindBestField: GDM(d, 300) x desire (GDM(150, 300) = 0.5)
	EXPECT_FLOAT_EQ(villager::FieldScore(150.0f, 1.0f), 0.5f);
	EXPECT_FLOAT_EQ(villager::FieldScore(150.0f, 0.5f), 0.25f);
	EXPECT_EQ(villager::FieldScore(10.0f, 0.0f), 0.0f);
	EXPECT_FLOAT_EQ(villager::FieldScore(0.0f, 1.0f), openblack::gutils::GetDistanceModifier(0.0f, 300.0f));
	// score 1 only with no fisherman (fish_farms::Score), x GDM(d, 500)
	EXPECT_FLOAT_EQ(villager::FishFarmScore(1, 250.0f), 0.5f);
	EXPECT_EQ(villager::FishFarmScore(0, 0.0f), 0.0f);
	// FarmerDigsUpCrop's capacity test: only a negative capacity
	EXPECT_FALSE(villager::OverFull(0));
	EXPECT_FALSE(villager::OverFull(150));
	EXPECT_TRUE(villager::OverFull(-1));
}

TEST(VillagerFarmingPure, FishCatch)
{
	// 37.5 x {1, 0.9, 0.7, 0.6}, truncated: 37 / 33 / 26 / 22
	const float base = villager::FishCatchBase(150);
	EXPECT_EQ(base, 37.5f);
	EXPECT_EQ(villager::FishCatch(base, 0, 150, 1.0f), 37);
	EXPECT_EQ(villager::FishCatch(base, 1, 150, 1.0f), 33);
	EXPECT_EQ(villager::FishCatch(base, 2, 150, 1.0f), 26);
	EXPECT_EQ(villager::FishCatch(base, 3, 150, 1.0f), 22);
	// the capacity clamps it; the tribal power multiplies after the clamp
	EXPECT_EQ(villager::FishCatch(base, 0, 2, 1.0f), 2);
	EXPECT_EQ(villager::FishCatch(base, 0, 0, 1.0f), 0);
	EXPECT_EQ(villager::FishCatch(base, 0, 150, 2.0f), 75);
}

// ---- the finders and CheckSatisfyFoodDesire ----------------------------------------------------------------------

TEST_F(VillagerFarmingTest, FindBestFieldStrict)
{
	const auto town = MakeTown();
	const auto v = MakeVillager();
	V(v).town = town;
	float score = -1.0f;
	// no field: none and 0
	EXPECT_EQ(villager::FindBestField(town, v, score), entt::entity(entt::null));
	EXPECT_EQ(score, 0.0f);
	// two equal fields: the first of the list (newest first) keeps it (strict >)
	const auto newer = AddField({100.0f, 130.0f}, 0.5f);
	const auto older = AddField({100.0f, 130.0f}, 0.5f);
	EXPECT_EQ(villager::FindBestField(town, v, score), newer);
	EXPECT_FLOAT_EQ(score, villager::FieldScore(0.0f, 0.5f));
	fields.at(older).desire = 0.6f;
	EXPECT_EQ(villager::FindBestField(town, v, score), older);
	// all 0: none
	fields.at(newer).desire = 0.0f;
	fields.at(older).desire = 0.0f;
	EXPECT_EQ(villager::FindBestField(town, v, score), entt::entity(entt::null));
	EXPECT_EQ(score, 0.0f);
}

TEST_F(VillagerFarmingTest, FoodDesireFieldWhenTheFarmHasAFisherman)
{
	const auto town = MakeTown();
	const auto v = MakeVillager();
	V(v).town = town;
	AddFarm({100.0f, 130.0f}, 0); // one fisherman: 0
	const auto field = AddField({110.0f, 130.0f}, 1.0f);
	points.push_back(Coords({112.0f, 128.0f}));
	EXPECT_EQ(villager::CheckSatisfyFoodDesire(v), 1u);
	// VillagerBecomesFarmer -> SetFarmerGotoField: 163, the target, the walk with FINAL 67, then the point (after the walk)
	EXPECT_EQ(V(v).targetThing, field);
	EXPECT_EQ(Top(v), 1u);
	EXPECT_EQ(Final(v), 67u);
	EXPECT_EQ(V(v).workPos, Coords({112.0f, 128.0f}));
	const auto arrive = std::find(calls.begin(), calls.end(), "arrive");
	const auto random = std::find(calls.begin(), calls.end(), "random");
	ASSERT_NE(arrive, calls.end());
	ASSERT_NE(random, calls.end());
	EXPECT_LT(arrive - calls.begin(), random - calls.begin());
	// the finders in k order: fish farms, fields
	EXPECT_LT(std::find(calls.begin(), calls.end(), "farms") - calls.begin(),
	          std::find(calls.begin(), calls.end(), "fields") - calls.begin());
}

TEST_F(VillagerFarmingTest, FoodDesireTieKeepsKOrder)
{
	const auto town = MakeTown();
	const auto v = MakeVillager();
	V(v).town = town;
	const auto farm = AddFarm({100.0f, 130.0f}, 1);
	AddField({100.0f, 130.0f}, 1.0f);
	// GDM(0, 500) == GDM(0, 300) (k_Sigmoid[30]): equal scores keep k's order, the fish farm (k 0) heads the list
	EXPECT_EQ(villager::FishFarmScore(1, 0.0f), villager::FieldScore(0.0f, 1.0f));
	EXPECT_EQ(villager::CheckSatisfyFoodDesire(v), 1u);
	EXPECT_EQ(V(v).targetThing, farm);
	EXPECT_EQ(Top(v), 1u);
	EXPECT_EQ(Final(v), 55u);
	EXPECT_EQ(Count("random"), 0u);
}

TEST_F(VillagerFarmingTest, FoodDesireDropAndNothing)
{
	const auto town = MakeTown();
	// nothing in the town, nothing held: 0
	const auto a = MakeVillager();
	V(a).town = town;
	EXPECT_EQ(villager::CheckSatisfyFoodDesire(a), 0u);
	EXPECT_EQ(Top(a), 163u);
	// 74 held and a field that scores 0: the drop-off (0.49) beats the head -> 31's walk (FINAL 32)
	AddField({100.0f, 130.0f}, 0.0f);
	const auto b = MakeVillager();
	V(b).town = town;
	V(b).resourceHeld.at(0) = 74;
	EXPECT_EQ(villager::CheckSatisfyFoodDesire(b), 1u);
	EXPECT_EQ(Final(b), 32u);
	// a town flock: (pending) the shepherd, the search finds none; the field scores 0: nothing
	auto& registry = Reg();
	const auto flock = registry.Create();
	auto& f = registry.Assign<Flock>(flock);
	f.domainCentre = glm::vec3(100.0f, 0.0f, 130.0f);
	registry.Get<Town>(town).flocks.push_back(flock);
	const auto c = MakeVillager();
	V(c).town = town;
	EXPECT_EQ(villager::CheckSatisfyFoodDesire(c), 0u);
	EXPECT_EQ(Top(c), 163u);
}

TEST_F(VillagerFarmingTest, FoodDesireFieldWithATownFlock)
{
	// a town flock beside the villager and a field. The flock search skips a flock with a shepherd; (pending) the
	// shepherd: every flock counts as taken, so the search gives none and 0, its node goes last and the field heads
	// the list -> VillagerBecomesFarmer, as in the original once the flock has its shepherd
	const auto town = MakeTown();
	const auto v = MakeVillager();
	V(v).town = town;
	auto& registry = Reg();
	const auto flock = registry.Create();
	registry.Assign<Flock>(flock).domainCentre = glm::vec3(100.0f, 0.0f, 130.0f);
	registry.Get<Town>(town).flocks.push_back(flock);
	const auto field = AddField({100.0f, 130.0f}, 0.5f);
	points.push_back(Coords({102.0f, 128.0f}));
	// a free flock here would outscore the field: GetDistanceModifier(0, 300) > FieldScore(0, 0.5)
	EXPECT_GT(gutils::GetDistanceModifier(0.0f, 300.0f), villager::FieldScore(0.0f, 0.5f));
	float score = -1.0f;
	EXPECT_EQ(villager::FindFlockWithoutShepherd(town, v, score), entt::entity(entt::null));
	EXPECT_EQ(score, 0.0f);
	EXPECT_EQ(villager::CheckSatisfyFoodDesire(v), 1u);
	EXPECT_EQ(V(v).targetThing, field);
	EXPECT_EQ(Top(v), 1u);
	EXPECT_EQ(Final(v), 67u);
}

// ---- the farmer --------------------------------------------------------------------------------------------------

TEST_F(VillagerFarmingTest, SetFarmerGotoField)
{
	const auto v = MakeVillager();
	const auto field = AddField({110.0f, 130.0f}, 1.0f, 0);
	// activity 0: nothing set, 0
	EXPECT_EQ(villager::SetFarmerGotoField(v, field), 0u);
	EXPECT_EQ(V(v).targetThing, entt::entity(entt::null));
	EXPECT_EQ(Top(v), 163u);
	EXPECT_EQ(Count("random"), 0u);
	// activity 2: the same as 1
	fields.at(field).activity = 2;
	points.push_back(Coords({108.0f, 133.0f}));
	EXPECT_EQ(villager::SetFarmerGotoField(v, field), 1u);
	EXPECT_EQ(V(v).targetThing, field);
	EXPECT_EQ(Final(v), 67u);
	EXPECT_EQ(Goal(v), tq::ToMetres({CoordsOf(field).x, CoordsOf(field).z}));
	EXPECT_EQ(V(v).workPos, Coords({108.0f, 133.0f}));
	// VillagerBecomesFarmer with no field: the town's FindBestField
	const auto town = MakeTown();
	const auto w = MakeVillager();
	EXPECT_EQ(villager::VillagerBecomesFarmer(w, entt::null), 0u); // no town
	V(w).town = town;
	fields.at(field).activity = 1;
	EXPECT_EQ(villager::VillagerBecomesFarmer(w, entt::null), 1u);
	EXPECT_EQ(V(w).targetThing, field);
}

TEST_F(VillagerFarmingTest, FarmerArrivesAtFarm)
{
	const auto field = AddField({110.0f, 130.0f}, 1.0f, 1);
	const auto v = MakeVillager({100.0f, 130.0f}, 67);
	V(v).targetThing = field;
	// sowing, at the point: a new point (2 draws) and PlayAnimThenSetState(68): 23 WAIT_FOR_ANIMATION, FINAL 68
	V(v).workPos = Coords({100.0f, 130.0f});
	points.push_back(Coords({104.0f, 131.0f}));
	EXPECT_EQ(villager::FarmerArrivesAtFarm(Action(v)), 1u);
	EXPECT_EQ(Top(v), 23u);
	EXPECT_EQ(Final(v), 68u);
	EXPECT_EQ(V(v).workPos, Coords({104.0f, 131.0f}));
	// sowing, not there: the walk to the point with FINAL 67, no draw
	calls.clear();
	Action(v).states.at(0) = 67;
	EXPECT_EQ(villager::FarmerArrivesAtFarm(Action(v)), 1u);
	EXPECT_EQ(Top(v), 1u);
	EXPECT_EQ(Final(v), 67u);
	EXPECT_EQ(Count("random"), 0u);
	// harvesting an unripe field: 163, no draw
	calls.clear();
	fields.at(field).activity = 2;
	Action(v).states.at(0) = 67;
	EXPECT_EQ(villager::FarmerArrivesAtFarm(Action(v)), 1u);
	EXPECT_EQ(Top(v), 163u);
	EXPECT_EQ(Count("random"), 0u);
	// ripe, there: the 2 discarded draws, then the new point's 2, and 69
	calls.clear();
	fields.at(field).ripe = true;
	V(v).targetThing = field;
	V(v).workPos = Coords({100.0f, 130.0f});
	Action(v).states.at(0) = 67;
	EXPECT_EQ(villager::FarmerArrivesAtFarm(Action(v)), 1u);
	EXPECT_EQ(Top(v), 23u);
	EXPECT_EQ(Final(v), 69u);
	EXPECT_EQ(Count("random"), 3u); // the mock counts RipeFarmPoint's 2 draws and RandomFarmPoint as one call
	// activity 0: 163
	fields.at(field).activity = 0;
	Action(v).states.at(0) = 67;
	EXPECT_EQ(villager::FarmerArrivesAtFarm(Action(v)), 1u);
	EXPECT_EQ(Top(v), 163u);
}

TEST_F(VillagerFarmingTest, FarmerPlantsCrop)
{
	const auto field = AddField({110.0f, 130.0f}, 1.0f, 1);
	const auto v = MakeVillager({100.0f, 130.0f}, 68);
	V(v).targetThing = field;
	EXPECT_EQ(villager::FarmerPlantsCrop(Action(v)), 1u);
	EXPECT_EQ(Top(v), 67u); // still sowing
	fields.at(field).stillSowing = false;
	Action(v).states.at(0) = 68;
	EXPECT_EQ(villager::FarmerPlantsCrop(Action(v)), 1u);
	EXPECT_EQ(Top(v), 163u); // the 30th crop
	fields.at(field).plantOk = false;
	V(v).targetThing = field;
	Action(v).states.at(0) = 68;
	EXPECT_EQ(villager::FarmerPlantsCrop(Action(v)), 1u);
	EXPECT_EQ(Top(v), 163u); // already full
}

TEST_F(VillagerFarmingTest, FarmerDigsUpCrop)
{
	const auto town = MakeTown();
	const auto field = AddField({110.0f, 130.0f}, 1.0f, 2);
	const auto v = MakeVillager({100.0f, 130.0f}, 69);
	V(v).town = town;
	V(v).targetThing = field;
	// capacity 150: RemoveFood(150.0) gives 150 -> picked up, capacity 0 is not below 0 -> 67
	fields.at(field).removeResult = 150;
	EXPECT_EQ(villager::FarmerDigsUpCrop(Action(v)), 1u);
	EXPECT_EQ(fields.at(field).lastRemove, 150.0f);
	EXPECT_EQ(V(v).resourceHeld.at(0), 150);
	EXPECT_EQ(Top(v), 67u);
	// capacity 0: RemoveFood(0) gives 0 -> nothing picked up, 67 again (the farmer digs for 0, literal)
	fields.at(field).removeResult = 0;
	Action(v).states.at(0) = 69;
	EXPECT_EQ(villager::FarmerDigsUpCrop(Action(v)), 1u);
	EXPECT_EQ(fields.at(field).lastRemove, 0.0f);
	EXPECT_EQ(Top(v), 67u);
	// 170 held (capacity -20): the unsigned cost empties a ripe field (300) -> 470 held, capacity < 0 -> 31 (FINAL 32)
	V(v).resourceHeld.at(0) = 170;
	fields.at(field).removeResult = 300;
	Action(v).states.at(0) = 69;
	EXPECT_EQ(villager::FarmerDigsUpCrop(Action(v)), 1u);
	EXPECT_EQ(fields.at(field).lastRemove, -20.0f);
	EXPECT_EQ(V(v).resourceHeld.at(0), 470);
	EXPECT_EQ(Final(v), 32u);
}

TEST_F(VillagerFarmingTest, EnterExitFarming)
{
	const auto field = AddField({110.0f, 130.0f}, 1.0f, 1);
	const auto v = MakeVillager({100.0f, 130.0f}, 1, 67);
	// no field: refused
	EXPECT_EQ(villager::EnterFarming(Action(v), S(163), S(67)), 0u);
	V(v).targetThing = field;
	// 163 -> 67: another entry function -> added once
	EXPECT_EQ(villager::EnterFarming(Action(v), S(163), S(67)), 1u);
	EXPECT_EQ(fields.at(field).farmers.size(), 1u);
	// 67 -> 68: the same entry function -> nothing
	EXPECT_EQ(villager::EnterFarming(Action(v), S(67), S(68)), 1u);
	EXPECT_EQ(Count("add farmer"), 1u);
	// leaving to 68 (the same exit as the final 67): kept
	EXPECT_EQ(villager::ExitFarming(Action(v), S(68)), 1u);
	EXPECT_EQ(fields.at(field).farmers.size(), 1u);
	EXPECT_EQ(V(v).targetThing, field);
	// leaving to 163: removed, the target cleared by RemoveFarmer
	EXPECT_EQ(villager::ExitFarming(Action(v), S(163)), 1u);
	EXPECT_TRUE(fields.at(field).farmers.empty());
	EXPECT_EQ(V(v).targetThing, entt::entity(entt::null));
	// a field no longer in the global list: the target kept
	V(v).targetThing = field;
	fields.at(field).isField = false;
	EXPECT_EQ(villager::ExitFarming(Action(v), S(163)), 1u);
	EXPECT_EQ(V(v).targetThing, field);
}

// ---- the fisherman -----------------------------------------------------------------------------------------------

TEST_F(VillagerFarmingTest, FishermanArrives)
{
	const auto farm = AddFarm({100.0f, 130.0f}, 1);
	farms.at(farm).spot = Coords({101.0f, 131.5f});
	// VillagerBecomesFisherman: 163, the target, the walk to its arrive point with FINAL 55
	const auto v = MakeVillager({130.0f, 130.0f});
	EXPECT_EQ(villager::VillagerBecomesFisherman(v, farm), 1u);
	EXPECT_EQ(V(v).targetThing, farm);
	EXPECT_EQ(Final(v), 55u);
	// 55 out of the farm's map cell: the walk again
	Action(v).states.at(0) = 55;
	EXPECT_FALSE(villager::IsAtValidFishingPos(v));
	EXPECT_EQ(villager::FishermanArrivesAtFishing(Action(v)), 1u);
	EXPECT_EQ(Top(v), 1u);
	EXPECT_EQ(Final(v), 55u);
	// on the arrive point (where the walk's goal put it: the arrive point in metres): the fishing spot (FINAL 55)
	const auto at = tq::ToMetres({CoordsOf(farm).x, CoordsOf(farm).z});
	Reg().Get<Transform>(v).position = glm::vec3(at.x, 0.0f, at.y);
	Action(v).states.at(0) = 55;
	calls.clear();
	EXPECT_TRUE(villager::IsAtValidFishingPos(v));
	EXPECT_EQ(villager::FishermanArrivesAtFishing(Action(v)), 1u);
	EXPECT_EQ(Count("spot"), 1u);
	EXPECT_EQ(Top(v), 1u);
	EXPECT_EQ(Goal(v), tq::ToMetres({farms.at(farm).spot.x, farms.at(farm).spot.z}));
	// in the cell, off the arrive point: PlayAnimThenSetState(56)
	Reg().Get<Transform>(v).position = glm::vec3(101.0f, 0.0f, 131.5f);
	Action(v).states.at(0) = 55;
	EXPECT_EQ(villager::FishermanArrivesAtFishing(Action(v)), 1u);
	EXPECT_EQ(Top(v), 23u);
	EXPECT_EQ(Final(v), 56u);
}

TEST_F(VillagerFarmingTest, FishingTrip)
{
	const auto town = MakeTown();
	const auto farm = AddFarm({100.0f, 130.0f}, 1);
	const auto v = MakeVillager({101.0f, 131.0f}, 56);
	V(v).town = town;
	V(v).targetThing = farm;
	farms.at(farm).fishermen.push_back(v);
	// spring, power 1: 37, 74, 111; the fourth catch (37) makes 148, and the capacity left (2) is below it
	// (GetFoodCapacity after the PickupFood < f) -> GotoStoragePitForDropOff (FINAL 32)
	for (const int16_t held : {37, 74, 111})
	{
		Action(v).turnsSinceStateChange = 5;
		EXPECT_EQ(villager::Fishing(Action(v)), 1u);
		EXPECT_EQ(Action(v).turnsSinceStateChange, 0u);
		EXPECT_EQ(V(v).resourceHeld.at(0), held);
		EXPECT_EQ(Top(v), 56u);
	}
	Action(v).turnsSinceStateChange = 5;
	EXPECT_EQ(villager::Fishing(Action(v)), 1u);
	EXPECT_EQ(Count("rand 1"), 4u); // GameRand(fishermen)
	EXPECT_EQ(V(v).resourceHeld.at(0), 148);
	EXPECT_EQ(Final(v), 32u);
	// a failed roll: nothing caught
	roll = 1;
	V(v).resourceHeld.at(0) = 0;
	Action(v).states.at(0) = 56;
	EXPECT_EQ(villager::Fishing(Action(v)), 1u);
	EXPECT_EQ(V(v).resourceHeld.at(0), 0);
	EXPECT_EQ(Top(v), 56u);
	// winter, power 2: trunc(2 x 22.5) = 45
	roll = 0;
	season = 3;
	tribal = 2.0f;
	EXPECT_EQ(villager::Fishing(Action(v)), 1u);
	EXPECT_EQ(V(v).resourceHeld.at(0), 45);
	// (openblack, guard) no target farm: 163, no draw and no catch
	const auto draws = Count("rand 0") + Count("rand 1");
	V(v).targetThing = entt::null;
	V(v).resourceHeld.at(0) = 0;
	Action(v).states.at(0) = 56;
	EXPECT_EQ(villager::Fishing(Action(v)), 1u);
	EXPECT_EQ(Top(v), 163u);
	EXPECT_EQ(V(v).resourceHeld.at(0), 0);
	EXPECT_EQ(Count("rand 0") + Count("rand 1"), draws);
}

TEST_F(VillagerFarmingTest, EnterExitFishing)
{
	const auto town = MakeTown();
	const auto farm = AddFarm({100.0f, 130.0f}, 1);
	const auto v = MakeVillager({100.0f, 130.0f}, 1, 55);
	V(v).targetThing = farm;
	// no town: nothing
	EXPECT_EQ(villager::EnterFishing(Action(v), S(163), S(55)), 1u);
	EXPECT_TRUE(farms.at(farm).fishermen.empty());
	V(v).town = town;
	// 55 -> 56: the same entry function
	EXPECT_EQ(villager::EnterFishing(Action(v), S(55), S(56)), 1u);
	EXPECT_TRUE(farms.at(farm).fishermen.empty());
	// 163 -> 55: added (once)
	EXPECT_EQ(villager::EnterFishing(Action(v), S(163), S(55)), 1u);
	EXPECT_EQ(villager::EnterFishing(Action(v), S(163), S(55)), 1u);
	EXPECT_EQ(farms.at(farm).fishermen.size(), 1u);
	// leaving to 56 (same exit): kept; to 163: removed and the target cleared
	EXPECT_EQ(villager::ExitFishing(Action(v), S(56)), 1u);
	EXPECT_EQ(V(v).targetThing, farm);
	EXPECT_EQ(villager::ExitFishing(Action(v), S(163)), 1u);
	EXPECT_TRUE(farms.at(farm).fishermen.empty());
	EXPECT_EQ(V(v).targetThing, entt::entity(entt::null));
	// a farm that is not available: not removed, the target still cleared
	V(v).targetThing = farm;
	farms.at(farm).fishermen.push_back(v);
	farms.at(farm).available = false;
	EXPECT_EQ(villager::ExitFishing(Action(v), S(163)), 1u);
	EXPECT_EQ(farms.at(farm).fishermen.size(), 1u);
	EXPECT_EQ(V(v).targetThing, entt::entity(entt::null));
}

// ---- what the player's creature hears ----------------------------------------------------------------------------

namespace
{
/// The town needs published while it lives, with an event manager of its own
struct HeardTownNeeds
{
	HeardTownNeeds()
	{
		Locator::events::emplace<EventManager>();
		Locator::events::value().AddHandler<ecs::events::CreatureEmpathyWithTownDesire>(
		    [this](const ecs::events::CreatureEmpathyWithTownDesire& event) { needs.push_back(event); });
	}

	const test::RestoreService<Locator::events> restore;
	std::vector<ecs::events::CreatureEmpathyWithTownDesire> needs;
};
} // namespace

TEST_F(VillagerFarmingTest, AFieldTakenOnSharesTheTownsNeedForFoodWithItsPlayer)
{
	HeardTownNeeds heard;
	const auto town = MakeTown(1);
	Reg().Get<Town>(town).owner = PlayerNames::PLAYER_ONE;
	const auto other = MakeTown(2);
	Reg().Get<Town>(other).owner = PlayerNames::PLAYER_TWO;
	const auto v = MakeVillager();
	V(v).town = town;
	const auto at = Reg().Get<const Transform>(v).position;
	const auto field = AddField({110.0f, 130.0f}, 1.0f, 0);
	Reg().Assign<Field>(field).town = 1;
	const auto theirs = AddField({120.0f, 130.0f}, 1.0f, 1);
	Reg().Assign<Field>(theirs).town = 2;
	// another player's field, or a field with nothing to do: not taken on, nothing shared
	V(v).targetThing = theirs;
	EXPECT_EQ(villager::CheckInteractWithField(Action(v)), 0u);
	V(v).targetThing = field;
	EXPECT_EQ(villager::CheckInteractWithField(Action(v)), 0u);
	EXPECT_TRUE(heard.needs.empty());
	// taken on: the need for food, 0.5, where the villager is, once
	fields.at(field).activity = 1;
	EXPECT_EQ(villager::CheckInteractWithField(Action(v)), 1u);
	ASSERT_EQ(heard.needs.size(), 1u);
	EXPECT_EQ(heard.needs.front().player, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(heard.needs.front().desire, TownDesireInfo::ForFood);
	EXPECT_FLOAT_EQ(heard.needs.front().weight, 0.5f);
	EXPECT_EQ(heard.needs.front().point, at);
	// a villager with no town takes on a field with no town: no player, nothing shared
	const auto loner = MakeVillager();
	const auto wild = AddField({130.0f, 130.0f}, 1.0f, 1);
	Reg().Assign<Field>(wild).town = -1;
	V(loner).targetThing = wild;
	EXPECT_EQ(villager::CheckInteractWithField(Action(loner)), 1u);
	EXPECT_EQ(heard.needs.size(), 1u);
}

TEST_F(VillagerFarmingTest, AFishFarmTakenOnSharesTheTownsNeedForFoodWithItsPlayer)
{
	HeardTownNeeds heard;
	const auto town = MakeTown(1);
	Reg().Get<Town>(town).owner = PlayerNames::PLAYER_ONE;
	const auto other = MakeTown(2);
	Reg().Get<Town>(other).owner = PlayerNames::PLAYER_TWO;
	const auto v = MakeVillager();
	V(v).town = town;
	const auto at = Reg().Get<const Transform>(v).position;
	const auto farm = AddFarm({110.0f, 130.0f});
	Reg().Assign<FishFarm>(farm).town = town;
	const auto theirs = AddFarm({120.0f, 130.0f});
	Reg().Assign<FishFarm>(theirs).town = other;
	// something that is not a fish farm, or another player's farm: not taken on, nothing shared
	V(v).targetThing = MakeThing({115.0f, 130.0f});
	EXPECT_EQ(villager::CheckInteractWithFishFarm(Action(v)), 0u);
	V(v).targetThing = theirs;
	EXPECT_EQ(villager::CheckInteractWithFishFarm(Action(v)), 0u);
	EXPECT_TRUE(heard.needs.empty());
	// taken on: the need for food, 0.5, where the villager is, once
	V(v).targetThing = farm;
	EXPECT_EQ(villager::CheckInteractWithFishFarm(Action(v)), 1u);
	ASSERT_EQ(heard.needs.size(), 1u);
	EXPECT_EQ(heard.needs.front().player, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(heard.needs.front().desire, TownDesireInfo::ForFood);
	EXPECT_FLOAT_EQ(heard.needs.front().weight, 0.5f);
	EXPECT_EQ(heard.needs.front().point, at);
	// a villager with no town takes on a farm with no town: no player, nothing shared
	const auto loner = MakeVillager();
	const auto wild = AddFarm({130.0f, 130.0f});
	Reg().Assign<FishFarm>(wild);
	V(loner).targetThing = wild;
	EXPECT_EQ(villager::CheckInteractWithFishFarm(Action(loner)), 1u);
	EXPECT_EQ(heard.needs.size(), 1u);
}
