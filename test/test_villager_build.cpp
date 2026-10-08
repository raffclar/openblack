/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The builders (docs/bw1-notes/villagers.md): the pure layer (the
// build factor and amount at 24-bit float steps, the cycles, the ring index, the wood source), the states 39 / 40 / 41
// / 184, GotoBuildingSite, GotoStoragePitForBuildingMaterials, EnterBuilding / ExitBuilding, the CheckSatisfy
// functions, SetupBuildingObject and the Repair_Town plans. The fixture of test_villager_resources.cpp (a fake state
// table in the Locator, scripted draws, the temporary store mock) and a mock building side (a fake
// villagerBuildingSites service in the Locator).

#define LOCATOR_IMPLEMENTATIONS

#include <cstring>

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "Common/GUtilsDistance.h"
#include "Common/GameRandomTesting.h"
#include "Common/RandomNumberManager.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/VillagerChildFactory.h"
#include "ECS/Systems/Implementations/VillagerDiscipleJobs.h"
#include "ECS/Systems/Implementations/VillagerRules.h"
#include "ECS/Systems/Implementations/VillagerStores.h"
#include "ECS/Systems/Implementations/VillagerWorldQueries.h"
#include "ECS/Systems/Implementations/VillagerWorshipCheck.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Town/TownDesire.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Villager/VillagerBuild.h"
#include "ECS/Villager/VillagerCore.h"
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
namespace td = openblack::ecs::town_desire;
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

uint32_t Bits(float value)
{
	uint32_t bits = 0;
	std::memcpy(&bits, &value, sizeof(bits));
	return bits;
}

MapCoords Coords(glm::vec2 metres)
{
	const auto fixed = tq::ToMapCoords(metres);
	return {fixed.x, fixed.y, 0.0f};
}

/// One mock building site
struct MockSite
{
	entt::entity building {entt::null};
	bool valid {true};
	bool needsBuilders {true};
	bool available {true};
	bool built {false};
	bool repaired {true};
	bool shouldGetWood {false};
	bool deleteOnBuildBy {false};
	uint32_t pile {0};
	float woodValue {1400.0f};
	float percent {0.0f};
	std::vector<entt::entity> builders;
	int32_t builderCount {0};
	int32_t randomIndex {5};
	int32_t nextIndex {6};
	MapCoords ringPos;
};

class VillagerBuildTest: public ::testing::Test
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
		// GVillagerInfo of info.dat: 50 for the men, 40 for the housewives
		v.maxFoodCarried = 150;
		v.maxWoodCarried = 250;
		v.minWoodToShowGraphic = 50;
		v.minFoodToShowGraphic = 100;
		v.woodUsedPerBuildCycle = 50.0f;
		v.amountOfWoodPerBuilderWanted = 50;
		info->town.maxDistanceForTownForest = 250.0f;
		for (auto& row : info->villagerStateTable)
		{
			row.animation = 385;
			row.field0x4 = -1;
			row.field0x8 = 1.0f;
			row.field0xf0 = 1; // no out-of clip; no row pauses (no draw from SetTopState)
		}
		auto& table = info->villagerStateTable;
		for (const uint32_t s : {31u, 32u, 39u, 40u, 41u, 163u, 184u})
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
		game_random::testing::SetGameRand([](uint32_t) { return 0u; }, [](float) { return 0.0f; });
		test::FakeVillagerStores stores(std::make_shared<ecs::systems::VillagerStores>());
		stores.temporaryStore = [](entt::entity, const MapCoords& from, ResourceType) {
			return ecs::town_stores::TemporaryStore {entt::null, from};
		};
		Locator::villagerStores::emplace<test::FakeVillagerStores>(stores);
		Locator::villagerBuildingSites::emplace<test::FakeVillagerBuildingSites>(MockOps());
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

	// ---- the mock building side ----

	MockSite* Site(entt::entity site)
	{
		const auto it = sites.find(site);
		return it != sites.end() ? &it->second : nullptr;
	}
	MockSite* SiteOfBuilding(entt::entity building)
	{
		for (auto& [entity, site] : sites)
		{
			if (site.building == building)
			{
				return &site;
			}
		}
		return nullptr;
	}
	size_t Count(const std::string& call) const { return static_cast<size_t>(std::count(calls.begin(), calls.end(), call)); }

	test::FakeVillagerBuildingSites MockOps()
	{
		test::FakeVillagerBuildingSites ops;
		ops.isBuildingHappening = [this](entt::entity) { return !sites.empty(); };
		ops.getBestBuildingSite = [this](entt::entity, const MapCoords&, bool includeFull) {
			calls.push_back(includeFull ? "best 1" : "best 0");
			return bestSite;
		};
		ops.getBestRepairBuildingSite = [this](entt::entity) {
			calls.emplace_back("best repair");
			return repairSite;
		};
		ops.isBuildingSiteValid = [this](entt::entity, entt::entity site) {
			const auto* s = Site(site);
			return s != nullptr && s->valid;
		};
		ops.getBuildingSiteInList = [this](entt::entity, entt::entity building) {
			for (const auto& [entity, site] : sites)
			{
				if (site.building == building)
				{
					return entity;
				}
			}
			return entt::entity {entt::null};
		};
		ops.addBuildingSite = [this](entt::entity, entt::entity building) {
			calls.emplace_back("add site");
			return allowAddSite ? NewSite(building) : entt::entity {entt::null};
		};
		ops.requestBestPlanned = [this](entt::entity) {
			calls.emplace_back("request civic");
			return Request();
		};
		ops.requestANewAbode = [this](entt::entity) {
			calls.emplace_back("request abode");
			return Request();
		};
		ops.addWoodUsedForBuilding = [this](entt::entity, uint32_t wood) {
			calls.push_back("wood used " + std::to_string(wood));
		};
		ops.getBuilding = [this](entt::entity site) {
			const auto* s = Site(site);
			return s != nullptr ? s->building : entt::entity {entt::null};
		};
		ops.needsBuilders = [this](entt::entity site) { return Site(site)->needsBuilders; };
		ops.isBuilder = [this](entt::entity site, entt::entity v) {
			const auto& list = Site(site)->builders;
			return std::find(list.begin(), list.end(), v) != list.end();
		};
		ops.getBuilderCount = [this](entt::entity site) { return Site(site)->builderCount; };
		ops.getClearAreaRadius = [](entt::entity) { return 5.0f; };
		ops.getWoodValue = [this](entt::entity site) {
			calls.emplace_back("value");
			return Site(site)->woodValue;
		};
		ops.shouldIGetWood = [this](entt::entity site, entt::entity, const std::function<MapCoords()>&) {
			calls.emplace_back("should");
			return Site(site)->shouldGetWood;
		};
		ops.getResource = [this](entt::entity site, ResourceType) { return Site(site)->pile; };
		ops.addResource = [this](entt::entity site, ResourceType, uint32_t amount, const MapCoords* pos) {
			calls.push_back("add " + std::to_string(amount) + (pos != nullptr ? " pos" : ""));
			Site(site)->pile += amount;
			return amount;
		};
		ops.removeResource = [this](entt::entity site, ResourceType, uint32_t amount) {
			calls.push_back("remove " + std::to_string(amount));
			auto* s = Site(site);
			const auto removed = std::min(amount, s->pile);
			s->pile -= removed;
			return removed;
		};
		ops.buildBy = [this](entt::entity site, float amount) {
			calls.emplace_back("buildby");
			lastBuildBy = amount;
			auto* s = Site(site);
			s->percent += amount;
			if (s->deleteOnBuildBy)
			{
				s->available = false;
				s->valid = false;
			}
		};
		ops.isAvailable = [this](entt::entity site) {
			const auto* s = Site(site);
			return s != nullptr && s->available;
		};
		ops.getRandomBuildPos = [this](entt::entity site, entt::entity, int32_t& index) {
			calls.emplace_back("random");
			index = Site(site)->randomIndex;
			return Site(site)->ringPos;
		};
		ops.getNextPosFromIndex = [this](entt::entity site, int32_t& index) {
			calls.emplace_back("next");
			index = Site(site)->nextIndex;
			return Site(site)->ringPos;
		};
		ops.getBuildPos = [this](entt::entity site, int32_t index) -> std::optional<MapCoords> {
			if (index < 0 || index >= 128)
			{
				return std::nullopt;
			}
			return Site(site)->ringPos;
		};
		ops.addBuilder = [this](entt::entity site, entt::entity v) {
			calls.emplace_back("add builder");
			auto* s = Site(site);
			s->builders.insert(s->builders.begin(), v);
			++s->builderCount;
		};
		ops.removeBuilder = [this](entt::entity site, entt::entity v) {
			calls.emplace_back("remove builder");
			auto* s = Site(site);
			s->builders.erase(std::remove(s->builders.begin(), s->builders.end(), v), s->builders.end());
			--s->builderCount;
		};
		ops.isBuilt = [this](entt::entity building) {
			const auto* s = SiteOfBuilding(building);
			return s != nullptr && s->built;
		};
		ops.isRepaired = [this](entt::entity building) {
			const auto* s = SiteOfBuilding(building);
			return s == nullptr || s->repaired;
		};
		ops.isTouching = [this](entt::entity, entt::entity, float margin) {
			calls.push_back("touching");
			touchingMargin = margin;
			return touching;
		};
		ops.landAlignmentAt = [this](const glm::vec3&) { return alignment; };
		return ops;
	}

	entt::entity NewSite(entt::entity building)
	{
		const auto site = Reg().Create();
		MockSite s;
		s.building = building;
		s.ringPos = Coords({105.0f, 130.0f});
		sites[site] = s;
		return site;
	}

	bool Request()
	{
		if (!requestOk)
		{
			return false;
		}
		bestSite = NewSite(MakeBuilding({110.0f, 130.0f}));
		return true;
	}

	std::map<entt::entity, MockSite> sites;
	std::vector<std::string> calls;
	entt::entity bestSite {entt::null};
	entt::entity repairSite {entt::null};
	bool requestOk {false};
	bool allowAddSite {true};
	bool touching {false};
	float touchingMargin {0.0f};
	float alignment {0.0f};
	float lastBuildBy {0.0f};

	// ---- the entities ----

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

	static entt::entity MakeBuilding(glm::vec2 at)
	{
		auto& registry = Reg();
		const auto building = registry.Create();
		registry.Assign<Transform>(building, glm::vec3(at.x, 0.0f, at.y), glm::mat3(1.0f), glm::vec3(1.0f));
		return building;
	}

	/// A builder of town 1 at `at` with a valid site (the ring point 5 m east of (100, 130), the building at (110,
	/// 130))
	entt::entity MakeBuilder(entt::entity& site, glm::vec2 at = {100.0f, 130.0f}, uint32_t top = 41)
	{
		const auto town = MakeTown();
		const auto e = MakeVillager(at, top);
		V(e).town = town;
		site = NewSite(MakeBuilding({110.0f, 130.0f}));
		V(e).buildingSite = site;
		return e;
	}

	static LivingAction& Action(entt::entity e) { return Reg().Get<LivingAction>(e); }
	static Villager& V(entt::entity e) { return Reg().Get<Villager>(e); }
	static Town& T(entt::entity town) { return Reg().Get<Town>(town); }
	static uint32_t Top(entt::entity e) { return Action(e).states.at(0); }
	static uint32_t Final(entt::entity e) { return Action(e).states.at(1); }
	static glm::vec2 Goal(entt::entity e) { return Reg().Get<WallHug>(e).goal; }
};
} // namespace

// ---- the pure layer ----------------------------------------------------------------------------------------------

TEST(VillagerBuildPure, BuildFactorAndAmount)
{
	// float steps (the x87 at 24 bits): good land 1; evil land up to 1.2f
	for (const float a : {1.0f, 0.5f, 0.0f})
	{
		EXPECT_EQ(villager::BuildFactor(a), 1.0f) << a;
		EXPECT_EQ(villager::BuildAmount(50.0f, villager::BuildFactor(a), 1000000), 50u) << a;
		EXPECT_EQ(villager::BuildAmount(40.0f, villager::BuildFactor(a), 1000000), 40u) << a;
	}
	struct Row
	{
		float a;
		uint32_t bits;
		uint32_t u50;
		uint32_t u40;
	};
	// 50 x 1.01999998f rounds to 51.0f at 24 bits (an extended product would give 50)
	for (const auto& row : {Row {-0.1f, 0x3F828F5C, 51, 40}, Row {-0.25f, 0x3F866666, 52, 42}, Row {-0.3f, 0x3F87AE14, 52, 42},
	                        Row {-0.5f, 0x3F8CCCCD, 55, 44}, Row {-0.75f, 0x3F933333, 57, 46}, Row {-1.0f, 0x3F99999A, 60, 48}})
	{
		const float f = villager::BuildFactor(row.a);
		EXPECT_EQ(Bits(f), row.bits) << row.a;
		EXPECT_EQ(villager::BuildAmount(50.0f, f, 1000000), row.u50) << row.a;
		EXPECT_EQ(villager::BuildAmount(40.0f, f, 1000000), row.u40) << row.a;
	}
	// the pile caps it
	EXPECT_EQ(villager::BuildAmount(50.0f, 1.0f, 30), 30u);
	EXPECT_EQ(villager::BuildAmount(50.0f, 1.0f, 0), 0u);
}

TEST(VillagerBuildPure, BuildStepCycles)
{
	const auto cycles = [](uint32_t u, float value, float from, float to) {
		const float x = villager::BuildStep(u, value);
		float p = from;
		uint32_t n = 0;
		while (p < to)
		{
			p = p + x;
			++n;
		}
		return n;
	};
	EXPECT_EQ(Bits(villager::BuildStep(50, 1400.0f)), Bits(50.0f / 1400.0f));
	EXPECT_EQ(cycles(50, 1400.0f, 0.0f, 1.0f), 29u); // Norse Hut
	EXPECT_EQ(cycles(40, 1400.0f, 0.0f, 1.0f), 35u);
	EXPECT_EQ(cycles(50, 6500.0f, 0.0f, 1.0f), 130u);  // Citadel Heart
	EXPECT_EQ(cycles(50, 6500.0f, 0.375f, 1.0f), 82u); // the Land 1 script's 37.5 %
	EXPECT_EQ(cycles(50, 6500.0f, 0.375f, 0.9f), 69u);
}

TEST(VillagerBuildPure, RingAndWoodSource)
{
	EXPECT_FALSE(villager::BuildPosOk(-1));
	EXPECT_TRUE(villager::BuildPosOk(0));
	EXPECT_TRUE(villager::BuildPosOk(127));
	EXPECT_FALSE(villager::BuildPosOk(128));
	EXPECT_TRUE(villager::NearRingPoint(0.2f));
	EXPECT_FALSE(villager::NearRingPoint(0.21f));
	// builder mode: the store only when it holds more than the villager can take; the forest 0.5
	auto w = villager::WoodSourceWeights(300, 250, 250, true);
	EXPECT_EQ(w.store, 1.0f);
	EXPECT_EQ(w.forest, 0.5f);
	EXPECT_EQ(villager::WoodSourceWeights(250, 250, 250, true).store, 0.0f);
	// a negative capacity is a huge unsigned: 0 (literal)
	EXPECT_EQ(villager::WoodSourceWeights(30000, -20, 250, true).store, 0.0f);
	// the store wins at 50 m with no forest
	auto c = villager::ChooseWoodSource(w, 50.0f, std::nullopt, false, 250.0f);
	EXPECT_EQ(c.how, 1u);
	EXPECT_EQ(Bits(c.store), Bits(gutils::GetDistanceModifier(50.0f, 250.0f)));
	// an empty store and no forest: 0; with a forest: 2 (a BigForest) / 3
	const auto empty = villager::WoodSourceWeights(250, 250, 250, true);
	EXPECT_EQ(villager::ChooseWoodSource(empty, 50.0f, std::nullopt, false, 250.0f).how, 0u);
	EXPECT_EQ(villager::ChooseWoodSource(empty, 50.0f, 10.0f, true, 250.0f).how, 2u);
	EXPECT_EQ(villager::ChooseWoodSource(empty, 50.0f, 10.0f, false, 250.0f).how, 3u);
	// the store far (200 m) against a forest at 10 m: the forest
	c = villager::ChooseWoodSource(w, 200.0f, 10.0f, false, 250.0f);
	EXPECT_EQ(c.how, 3u);
	EXPECT_EQ(Bits(c.forest), Bits(gutils::GetDistanceModifier(10.0f, 250.0f) * 0.5f));
	// mode 0: the store frac, the forest 1 - frac
	w = villager::WoodSourceWeights(0, 125, 250, false);
	EXPECT_EQ(Bits(w.store), Bits(villager::DropOffFraction(125, 250)));
	EXPECT_EQ(Bits(w.forest), Bits(1.0f - w.store));
}

// ---- the build cycle (41) ----------------------------------------------------------------------------------------

TEST_F(VillagerBuildTest, BuildingCycle)
{
	entt::entity site = entt::null;
	const auto e = MakeBuilder(site);
	Site(site)->pile = 120;
	Action(e).turnsSinceStateChange = 9;
	EXPECT_EQ(villager::BuildingState(Action(e)), 1u);
	// u = 50: the value read before the removal, RemoveResource before BuildBy, then the town's stats
	const std::vector<std::string> order {"value", "remove 50", "buildby", "wood used 50", "next"};
	ASSERT_GE(calls.size(), order.size());
	EXPECT_TRUE(std::equal(order.begin(), order.end(), calls.begin()));
	EXPECT_EQ(Bits(lastBuildBy), Bits(50.0f / 1400.0f));
	EXPECT_EQ(Site(site)->pile, 70u);
	// wood left: the next ring point, FINAL 40
	EXPECT_EQ(V(e).buildPosIndex, 6);
	EXPECT_EQ(Top(e), 1u);
	EXPECT_EQ(Final(e), 40u);
	EXPECT_NEAR(Goal(e).x, 105.0f, 0.01f);
	EXPECT_EQ(V(e).buildingSite, site);
	// evil land: u 60 from a pile of 70
	calls.clear();
	alignment = -1.0f;
	Action(e).states.at(0) = 41;
	EXPECT_EQ(villager::BuildingState(Action(e)), 1u);
	EXPECT_EQ(Count("remove 60"), 1u);
	EXPECT_EQ(Site(site)->pile, 10u);
	// the pile caps u (10); none left: SetupGetBuildingSupplies (ShouldIGetWood 0 -> GotoBuildingSite, FINAL 40)
	calls.clear();
	alignment = 0.0f;
	Action(e).states.at(0) = 41;
	EXPECT_EQ(villager::BuildingState(Action(e)), 1u);
	EXPECT_EQ(Count("remove 10"), 1u);
	EXPECT_EQ(Count("should"), 1u);
	EXPECT_EQ(Count("random"), 1u);
	EXPECT_EQ(Final(e), 40u);
	// an empty pile: no removal, BuildBy or stats
	calls.clear();
	Action(e).states.at(0) = 41;
	villager::BuildingState(Action(e));
	EXPECT_EQ(Count("buildby"), 0u);
	EXPECT_EQ(Count("value"), 0u);
}

TEST_F(VillagerBuildTest, BuildingRelease)
{
	entt::entity site = entt::null;
	const auto e = MakeBuilder(site);
	// BuildBy finishes the building (the site is deleted): the site cleared, 163, no RemoveBuilder
	Site(site)->pile = 50;
	Site(site)->deleteOnBuildBy = true;
	EXPECT_EQ(villager::BuildingState(Action(e)), 1u);
	EXPECT_TRUE(V(e).buildingSite == entt::null);
	EXPECT_EQ(Top(e), 163u);
	EXPECT_EQ(Count("remove builder"), 0u);
	EXPECT_EQ(Count("buildby"), 1u);
	// an invalid site: release without building
	calls.clear();
	const auto f = MakeVillager({100.0f, 130.0f}, 41);
	V(f).town = V(e).town;
	V(f).buildingSite = site;
	EXPECT_EQ(villager::BuildingState(Action(f)), 1u);
	EXPECT_EQ(Top(f), 163u);
	EXPECT_EQ(Count("buildby"), 0u);
	// no town: 0
	const auto g = MakeVillager({100.0f, 130.0f}, 41);
	EXPECT_EQ(villager::BuildingState(Action(g)), 0u);
	EXPECT_EQ(Top(g), 41u);
}

// ---- the ring (40, 184, GotoBuildingSite) ------------------------------------------------------------------------

TEST_F(VillagerBuildTest, ArrivesAtBuildingSite)
{
	entt::entity site = entt::null;
	// far from the ring point (5 m): the walk again, FINAL 40
	const auto e = MakeBuilder(site, {100.0f, 130.0f}, 40);
	V(e).buildPosIndex = 3;
	EXPECT_EQ(villager::ArrivesAtBuildingSite(Action(e)), 1u);
	EXPECT_EQ(Top(e), 1u);
	EXPECT_EQ(Final(e), 40u);
	// a ring index out of range: 163
	Action(e).states.at(0) = 40;
	V(e).buildPosIndex = 128;
	EXPECT_EQ(villager::ArrivesAtBuildingSite(Action(e)), 1u);
	EXPECT_EQ(Top(e), 163u);
	// an invalid site: 163, the site kept (the exit clears it)
	Action(e).states.at(0) = 40;
	V(e).buildPosIndex = 3;
	Site(site)->valid = false;
	EXPECT_EQ(villager::ArrivesAtBuildingSite(Action(e)), 1u);
	EXPECT_EQ(Top(e), 163u);
	EXPECT_EQ(V(e).buildingSite, site);
	// on the ring point with 120 wood: it turns to the building, then puts the wood on the pile and waits for the clip
	Site(site)->valid = true;
	const auto f = MakeVillager({105.0f, 130.0f}, 40);
	V(f).town = V(e).town;
	V(f).buildingSite = site;
	V(f).buildPosIndex = 3;
	V(f).resourceHeld.at(1) = 120;
	Action(f).turnsSinceStateChange = 7;
	for (int i = 0; i < 20 && Top(f) == 40u; ++i)
	{
		EXPECT_EQ(villager::ArrivesAtBuildingSite(Action(f)), 1u);
	}
	EXPECT_EQ(Count("add 120 pos"), 1u);
	EXPECT_EQ(Site(site)->pile, 120u);
	EXPECT_EQ(V(f).resourceHeld.at(1), 0);
	EXPECT_EQ(Top(f), 23u);
	EXPECT_EQ(Final(f), 41u);
	EXPECT_EQ(Action(f).turnsSinceStateChange, 7u);
}

TEST_F(VillagerBuildTest, GotoBuildingSite)
{
	entt::entity site = entt::null;
	const auto e = MakeBuilder(site, {100.0f, 130.0f}, 41);
	// not a builder, the site full, not a BUILDER disciple: 0 and no change
	Site(site)->needsBuilders = false;
	EXPECT_EQ(villager::GotoBuildingSite(e, site), 0u);
	EXPECT_EQ(Top(e), 41u);
	EXPECT_EQ(Count("random"), 0u);
	// a BUILDER disciple: SetTopState(163), the site set, the random ring point (index 5), a wall-hug walk (5 m)
	V(e).discipleType = 4;
	V(e).buildingSite = entt::null;
	EXPECT_EQ(villager::GotoBuildingSite(e, site), 1u);
	EXPECT_EQ(V(e).buildingSite, site);
	EXPECT_EQ(V(e).buildPosIndex, 5);
	EXPECT_EQ(Top(e), 1u);
	EXPECT_EQ(Final(e), 40u);
	EXPECT_NEAR(Goal(e).x, 105.0f, 0.01f);
	EXPECT_NEAR(Goal(e).y, 130.0f, 0.01f);
}

TEST_F(VillagerBuildTest, ReenterBuildingState)
{
	entt::entity site = entt::null;
	const auto e = MakeBuilder(site, {100.0f, 130.0f}, 184);
	// touching the building: SetTopState(40), after one GetRandomBuildPos
	touching = true;
	EXPECT_EQ(villager::ReenterBuildingState(Action(e)), 1u);
	EXPECT_EQ(Top(e), 40u);
	EXPECT_EQ(Count("random"), 1u);
	EXPECT_EQ(touchingMargin, 0.001f);
	// not touching: the walk to the ring point
	touching = false;
	Action(e).states.at(0) = 184;
	EXPECT_EQ(villager::ReenterBuildingState(Action(e)), 1u);
	EXPECT_EQ(Top(e), 1u);
	EXPECT_EQ(Final(e), 40u);
	// it should get wood, but no store and no forest: SetupGetBuildingSupplies 0 -> 163
	Site(site)->shouldGetWood = true;
	Action(e).states.at(0) = 184;
	EXPECT_EQ(villager::ReenterBuildingState(Action(e)), 1u);
	EXPECT_EQ(Top(e), 163u);
	// an invalid site: 163
	Site(site)->valid = false;
	Action(e).states.at(0) = 184;
	EXPECT_EQ(villager::ReenterBuildingState(Action(e)), 1u);
	EXPECT_EQ(Top(e), 163u);
}

// ---- the wood (GotoStoragePitForBuildingMaterials, 39) -----------------------------------------------------------

TEST_F(VillagerBuildTest, GotoStoragePitForBuildingMaterials)
{
	entt::entity site = entt::null;
	const auto e = MakeBuilder(site, {100.0f, 130.0f}, 163);
	V(e).buildingSite = entt::null;
	// no pit, no home: GetResourceDropoffPos (the temporary store's point), a wall-hug walk with FINAL 39; from 163 the
	// site is written
	const auto store = tq::ToMapCoords({80.0f, 130.0f});
	test::FakeVillagerStores stores(std::make_shared<ecs::systems::VillagerStores>());
	stores.temporaryStore = [store](entt::entity, const MapCoords&, ResourceType) {
		return ecs::town_stores::TemporaryStore {entt::null, {store.x, store.y, 0.0f}};
	};
	Locator::villagerStores::emplace<test::FakeVillagerStores>(stores);
	EXPECT_EQ(villager::GotoStoragePitForBuildingMaterials(e, site), 1u);
	EXPECT_EQ(V(e).buildingSite, site);
	EXPECT_EQ(Final(e), 39u);
	EXPECT_NEAR(Goal(e).x, 80.0f, 0.01f);
	// from a building state (TOP 41) the site is not touched
	const auto f = MakeVillager({100.0f, 130.0f}, 41);
	V(f).town = V(e).town;
	EXPECT_EQ(villager::GotoStoragePitForBuildingMaterials(f, site), 1u);
	EXPECT_TRUE(V(f).buildingSite == entt::null);
	EXPECT_EQ(Final(f), 39u);
	// not a builder and the site full: 0
	Site(site)->needsBuilders = false;
	const auto g = MakeVillager({100.0f, 130.0f}, 163);
	V(g).town = V(e).town;
	EXPECT_EQ(villager::GotoStoragePitForBuildingMaterials(g, site), 0u);
	EXPECT_EQ(Top(g), 163u);
	// full of wood (capacity 0, and -20): GotoBuildingSite (a builder already)
	Site(site)->builders.push_back(g);
	for (const int16_t wood : {int16_t {250}, int16_t {270}})
	{
		calls.clear();
		V(g).resourceHeld.at(1) = wood;
		Action(g).states.at(0) = 163;
		EXPECT_EQ(villager::GotoStoragePitForBuildingMaterials(g, site), 1u);
		EXPECT_EQ(Count("random"), 1u);
		EXPECT_EQ(Final(g), 40u);
	}
}

TEST_F(VillagerBuildTest, ArrivesAtStoragePitForBuildingMaterials)
{
	entt::entity site = entt::null;
	const auto e = MakeBuilder(site, {100.0f, 130.0f}, 39);
	// capacity 0: GotoBuildingSite
	V(e).resourceHeld.at(1) = 250;
	EXPECT_EQ(villager::ArrivesAtStoragePitForBuildingMaterials(Action(e)), 1u);
	EXPECT_EQ(Final(e), 40u);
	EXPECT_EQ(Count("random"), 1u);
	// room for wood but no pit and no temporary pot: ArrivesAtStoragePitForResource's fail, 163
	V(e).resourceHeld.at(1) = 0;
	Action(e).states.at(0) = 39;
	villager::ArrivesAtStoragePitForBuildingMaterials(Action(e));
	EXPECT_EQ(Top(e), 163u);
	// no valid site: 163
	Site(site)->valid = false;
	Action(e).states.at(0) = 39;
	EXPECT_EQ(villager::ArrivesAtStoragePitForBuildingMaterials(Action(e)), 1u);
	EXPECT_EQ(Top(e), 163u);
}

// ---- the builder book-keeping ------------------------------------------------------------------------------------

TEST_F(VillagerBuildTest, EnterAndExitBuilding)
{
	entt::entity site = entt::null;
	const auto e = MakeBuilder(site, {100.0f, 130.0f}, 163);
	// from 163 into 39: AddBuilder
	EXPECT_EQ(villager::EnterBuilding(Action(e), S(163), S(39)), 1u);
	EXPECT_EQ(Site(site)->builderCount, 1);
	// 41 -> 40 (the same entry function): no AddBuilder
	EXPECT_EQ(villager::EnterBuilding(Action(e), S(41), S(40)), 1u);
	EXPECT_EQ(Site(site)->builderCount, 1);
	// TOP 41 told 40 (the same exit): it stays a builder
	Action(e).states.at(0) = 41;
	EXPECT_EQ(villager::ExitBuilding(Action(e), S(40)), 1u);
	EXPECT_EQ(V(e).buildingSite, site);
	EXPECT_EQ(Site(site)->builderCount, 1);
	// told 163: RemoveBuilder and the site cleared
	EXPECT_EQ(villager::ExitBuilding(Action(e), S(163)), 1u);
	EXPECT_TRUE(V(e).buildingSite == entt::null);
	EXPECT_EQ(Site(site)->builderCount, 0);
	EXPECT_EQ(Count("remove builder"), 1u);
	// no valid site: the entry refuses (0)
	EXPECT_EQ(villager::EnterBuilding(Action(e), S(163), S(184)), 0u);
	EXPECT_EQ(Site(site)->builderCount, 0);
	// an invalid site on exit: no RemoveBuilder, the site still cleared
	V(e).buildingSite = site;
	Site(site)->valid = false;
	EXPECT_EQ(villager::ExitBuilding(Action(e), S(163)), 1u);
	EXPECT_TRUE(V(e).buildingSite == entt::null);
	EXPECT_EQ(Count("remove builder"), 1u);
}

// ---- the decision path -------------------------------------------------------------------------------------------

TEST_F(VillagerBuildTest, CheckSatisfyAbodesAndCivic)
{
	const auto town = MakeTown();
	const auto e = MakeVillager();
	V(e).town = town;
	// no site, the request fails: 0, but the town's request flag is set before the request
	EXPECT_EQ(villager::CheckSatisfyAbodesDesire(e), 0u);
	EXPECT_TRUE(T(town).requestedPlanThisTurn);
	EXPECT_EQ(Count("request abode"), 1u);
	// already requested this turn: no request
	EXPECT_EQ(villager::CheckSatisfyCivicBuildings(e), 0u);
	EXPECT_EQ(Count("request civic"), 0u);
	// a new turn, the request makes a site: CheckNeededForBuilding again -> the site (GotoBuildingSite)
	T(town).requestedPlanThisTurn = false;
	requestOk = true;
	EXPECT_EQ(villager::CheckSatisfyCivicBuildings(e), 1u);
	EXPECT_EQ(Count("request civic"), 1u);
	EXPECT_EQ(V(e).buildingSite, bestSite);
	EXPECT_EQ(Final(e), 40u);
	// with a site, CheckNeededForBuilding first (no request)
	calls.clear();
	Action(e).states.at(0) = 163;
	T(town).requestedPlanThisTurn = false;
	EXPECT_EQ(villager::CheckSatisfyAbodesDesire(e), 1u);
	EXPECT_EQ(Count("request abode"), 0u);
	EXPECT_FALSE(T(town).requestedPlanThisTurn);
}

TEST_F(VillagerBuildTest, CheckSatisfyToBuildAndRepair)
{
	const auto town = MakeTown();
	const auto e = MakeVillager();
	V(e).town = town;
	// no site: 0; includeFull is the BUILDER disciple's
	EXPECT_EQ(villager::CheckSatisfyToBuild(e), 0u);
	EXPECT_EQ(Count("best 0"), 1u);
	V(e).discipleType = 4;
	villager::CheckSatisfyToBuild(e);
	EXPECT_EQ(Count("best 1"), 1u);
	// the repair site
	repairSite = NewSite(MakeBuilding({110.0f, 130.0f}));
	Site(repairSite)->built = true;
	Site(repairSite)->repaired = false;
	EXPECT_EQ(villager::CheckSatisfyToRepair(e), 1u);
	EXPECT_EQ(V(e).buildingSite, repairSite);
	// built and repaired: SetupBuildingObject 0
	Site(repairSite)->repaired = true;
	Action(e).states.at(0) = 163;
	EXPECT_EQ(villager::CheckSatisfyToRepair(e), 0u);
	// no town: 0
	const auto f = MakeVillager();
	EXPECT_EQ(villager::CheckSatisfyToRepair(f), 0u);
}

TEST_F(VillagerBuildTest, SetupBuildingObjectForBuilding)
{
	const auto town = MakeTown();
	const auto e = MakeVillager();
	V(e).town = town;
	const auto home = MakeBuilding({110.0f, 130.0f});
	// AddBuildingSite fails: 0
	allowAddSite = false;
	EXPECT_EQ(villager::SetupBuildingObjectForBuilding(e, home), 0u);
	EXPECT_EQ(Count("add site"), 1u);
	// no site yet: AddBuildingSite (a repair site), then the site's SetupBuildingObject
	allowAddSite = true;
	EXPECT_EQ(villager::SetupBuildingObjectForBuilding(e, home), 1u);
	EXPECT_EQ(Count("add site"), 2u);
	ASSERT_NE(SiteOfBuilding(home), nullptr);
	EXPECT_EQ(Final(e), 40u);
	// the site exists: no new one
	Action(e).states.at(0) = 163;
	EXPECT_EQ(villager::SetupBuildingObjectForBuilding(e, home), 1u);
	EXPECT_EQ(Count("add site"), 2u);
}

// ---- Repair_Town: the plans after the abodes ---------------------------------------------------------------------

TEST(VillagerBuildTownDesire, RepairPlans)
{
	TownDesire desire {};
	td::DesireInputs in {};
	GTownInfo town {};
	town.thresholdToStartRepairing = 0.9f;
	std::array<GTownDesireInfo, 17> info {};
	in.planRepairDesires = {0.25f, 0.5f};
	EXPECT_FLOAT_EQ(td::DesireToRepair(td::DesireContext {desire, in, town, info}), 0.75f);
	// an abode first (0.375), then the plans; at most 1
	in.abodes = {{0.5f, true, 3, 0.5f}};
	in.planRepairDesires = {0.5f};
	EXPECT_FLOAT_EQ(td::DesireToRepair(td::DesireContext {desire, in, town, info}), 0.875f);
	in.planRepairDesires = {0.5f, 0.5f};
	EXPECT_FLOAT_EQ(td::DesireToRepair(td::DesireContext {desire, in, town, info}), 1.0f);
}
