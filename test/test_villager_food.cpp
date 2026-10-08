/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The villager's food (docs/bw1-notes/villagers.md; values worked out in float32, as the original's 24-bit float
// precision): CheckHungry's batch, the hunger damage, the
// interruptions, the amounts, ChangeStateToFindFoodToEat, EatFoodHeld, the double pick-up of GetFoodFromHome, 117 /
// 118 / 212 and 34. A fake state table in the Locator and scripted draws (the fixture of test_villager_decide.cpp).

#define LOCATOR_IMPLEMENTATIONS

#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "Common/GameRandomTesting.h"
#include "Common/RandomNumberManager.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Life.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Poisoned.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/ObjectCreationIndex.h"
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
#include "ECS/Villager/VillagerFood.h"
#include "ECS/Villager/VillagerResources.h"
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
using Index = LivingAction::Index;

namespace
{
class TestRng final: public RandomNumberManagerInterface
{
	std::mt19937 _generator {1};
	std::mt19937& Generator() override { return _generator; }
	std::optional<std::reference_wrapper<std::mutex>> LockAccess() override { return std::nullopt; }
};

/// A state table where each row records its calls and returns what the test says (default: no function = 1)
class FakeStateTable final: public ecs::systems::LivingActionSystemInterface
{
public:
	mutable std::vector<std::string> calls;
	void Update() override {}
	[[nodiscard]] VillagerStates VillagerGetState(const LivingAction& action, Index index) const override
	{
		return static_cast<VillagerStates>(action.states.at(static_cast<size_t>(index)));
	}
	void VillagerSetState(LivingAction&, Index, VillagerStates, bool) const override {}
	uint32_t VillagerCallState(LivingAction&, Index) const override { return 0; }
	uint32_t VillagerCallEntry(LivingAction&, VillagerStates row, VillagerStates, VillagerStates) const override
	{
		calls.push_back("entry " + std::to_string(static_cast<uint32_t>(row)));
		return 1;
	}
	uint32_t VillagerCallExit(LivingAction&, VillagerStates row, VillagerStates next) const override
	{
		calls.push_back("exit " + std::to_string(static_cast<uint32_t>(row)) + " " +
		                std::to_string(static_cast<uint32_t>(next)));
		return 1;
	}
	int VillagerCallOutOfAnimation(LivingAction&, Index) const override { return -1; }
	bool VillagerCallValidate(LivingAction&, Index) const override { return false; }
};

constexpr auto S(uint32_t n)
{
	return static_cast<VillagerStates>(n);
}

class VillagerFoodTest: public ::testing::Test
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
		v.damageThresholdToGoHome = 0.3f;
		v.damageThresholdToSleepUntil = 0.7f;
		v.gameTurnReducesFoodInBellyBy = 9e-5f;
		v.hungerToLifeMultiplier = 0.001f;
		v.foodReqiredForDinner = 85;
		v.foodNurishmentMultiplier = 1.2f;
		v.ownDesireThreshold = 0.3f;
		v.pauseForASecondChance = 0.01f;
		v.grownUpAge = 13;
		v.oldAge = 60;
		v.retirementAge = 100;
		v.sex = SexType::Male;
		v.life = 1.0f;
		v.moveState = LivingStates::LivingMoveToPos;
		v.speedGroup.speedDefault = static_cast<SpeedState>(100);
		for (auto& row : info->villagerStateTable)
		{
			row.animation = 385;
			row.field0x4 = -1;
			row.field0x8 = 1.0f;
			row.field0xf0 = 1; // no out-of clip; no row pauses (no draw from SetTopState)
		}
		auto& table = info->villagerStateTable;
		for (const uint32_t s : {33u, 34u, 36u, 37u, 38u, 117u, 118u, 163u, 212u, 245u, 248u})
		{
			table.at(s).isFinalState = 1;
		}
		table.at(1).field0x14 = 1;
		// 245 GO_AND_CHILLOUT_OUTSIDE_HOME: InterruptWhenHungry / Starving 1, as in info.dat
		table.at(245).field0xd0 = 1;
		table.at(245).field0xd4 = 1;
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
		// GetTemporaryResourceStorePotOrPos without a pile (its point is the villager's own position) unless a
		// test makes one
		test::FakeVillagerStores stores(std::make_shared<ecs::systems::VillagerStores>());
		stores.temporaryStore = [](entt::entity, const map_coords::MapCoords& from, ResourceType) {
			return ecs::town_stores::TemporaryStore {entt::null, from};
		};
		Locator::villagerStores::emplace<test::FakeVillagerStores>(stores);
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

	static FakeStateTable& Table() { return static_cast<FakeStateTable&>(Locator::livingActionSystem::value()); }
	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

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

	/// A storage pit (StoragePit + its food pile, a Pot) that is the town's storage pit
	static entt::entity MakePit(entt::entity town, uint16_t food)
	{
		auto& registry = Reg();
		const auto pit = MakeAbode({70.0f, 70.0f}, food, registry.Get<Town>(town).id);
		const auto pile = registry.Create();
		auto& pot = registry.Assign<Pot>(pile);
		pot.amount = food;
		auto& store = registry.Assign<StoragePit>(pit);
		store.woodPiles.fill(entt::null);
		store.foodPile = pile;
		registry.Get<Town>(town).storagePit = pit;
		return pit;
	}

	static LivingAction& Action(entt::entity e) { return Reg().Get<LivingAction>(e); }
	static Villager& V(entt::entity e) { return Reg().Get<Villager>(e); }
	static uint32_t Top(entt::entity e) { return Action(e).states.at(0); }
	static uint32_t Final(entt::entity e) { return Action(e).states.at(1); }
};
} // namespace

TEST_F(VillagerFoodTest, HungerBatch)
{
	// 0.8 - 9 x 9e-5 (0.79919)
	const float drop = 9.0f * 9e-5f;
	EXPECT_FLOAT_EQ(villager::HungerBatch(0.8f, 9, 9e-5f, std::nullopt, 1.0f, false), 0.8f - drop);
	EXPECT_NEAR(villager::HungerBatch(0.8f, 9, 9e-5f, std::nullopt, 1.0f, false), 0.79919f, 1e-6f);
	// twice the default speed and moving: twice the drop; not moving: once
	EXPECT_FLOAT_EQ(villager::HungerBatch(0.8f, 9, 9e-5f, std::nullopt, 2.0f, true), 0.8f - 2.0f * drop);
	EXPECT_FLOAT_EQ(villager::HungerBatch(0.8f, 9, 9e-5f, std::nullopt, 2.0f, false), 0.8f - drop);
	// a speed of 1 exactly is not above 1
	EXPECT_FLOAT_EQ(villager::HungerBatch(0.8f, 9, 9e-5f, std::nullopt, 1.0f, true), 0.8f - drop);
	// TribalPower 2: half
	EXPECT_FLOAT_EQ(villager::HungerBatch(0.8f, 9, 9e-5f, 2.0f, 1.0f, false), 0.8f - drop / 2.0f);
	// never below 0
	EXPECT_FLOAT_EQ(villager::HungerBatch(0.0001f, 9, 9e-5f, std::nullopt, 1.0f, false), 0.0f);
	// 0 turns: CheckHungry does nothing, LastCheckTurn untouched
	const auto a = MakeVillager();
	V(a).food = 0.8f;
	V(a).lastCheckTurn = 1000;
	EXPECT_FALSE(villager::CheckHungry(a, 1000));
	EXPECT_FLOAT_EQ(V(a).food, 0.8f);
	EXPECT_EQ(V(a).lastCheckTurn, 1000u);
	// 9 turns: the batch and the clock
	EXPECT_FALSE(villager::CheckHungry(a, 1009));
	EXPECT_FLOAT_EQ(V(a).food, 0.8f - drop);
	EXPECT_EQ(V(a).lastCheckTurn, 1009u);
}

TEST_F(VillagerFoodTest, HungerDamage)
{
	// food 0.49 < 0.5: -0.001 (max(1 - food / 0.5, 1) x 0.001)
	auto a = MakeVillager();
	V(a).food = 0.49f;
	villager::CheckHungry(a, 1001);
	EXPECT_FLOAT_EQ(V(a).life, 1.0f - 0.001f);
	// food 0.5 exactly (no drop): not hungry for CheckHungry (strict), though IsHungry (<=) says yes
	auto& info = const_cast<GVillagerInfo&>(villager::InfoOf(a));
	info.gameTurnReducesFoodInBellyBy = 0.0f;
	auto b = MakeVillager();
	V(b).food = 0.5f;
	EXPECT_TRUE(villager::IsHungry(b));
	villager::CheckHungry(b, 1001);
	EXPECT_FLOAT_EQ(V(b).life, 1.0f);
	// poisoned and fed: the damage, no state change
	auto c = MakeVillager({100.0f, 130.0f}, 245);
	V(c).food = 0.9f;
	Reg().Assign<Poisoned>(c);
	EXPECT_FALSE(villager::CheckHungry(c, 1001));
	EXPECT_FLOAT_EQ(V(c).life, 1.0f - 0.001f);
	EXPECT_EQ(Top(c), 245u);
}

TEST_F(VillagerFoodTest, HungerInterrupts)
{
	auto& info = const_cast<GVillagerInfo&>(Locator::infoConstants::value().villager.at(0));
	info.gameTurnReducesFoodInBellyBy = 0.0f;
	info.foodReqiredForDinner = 0; // need 0: ChangeStateToFindFoodToEat eats at once (117)
	// the final state 245 interrupts when hungry: 117 and 1
	auto a = MakeVillager({100.0f, 130.0f}, 245);
	V(a).food = 0.4f;
	EXPECT_TRUE(villager::CheckHungry(a, 1001));
	EXPECT_EQ(Top(a), 117u);
	// a disciple that ignores the needs: not when hungry...
	auto b = MakeVillager({100.0f, 130.0f}, 245);
	V(b).food = 0.4f;
	V(b).flags = Villager::k_FlagDisciple;
	V(b).discipleType = 1;
	EXPECT_FALSE(villager::CheckHungry(b, 1001));
	EXPECT_EQ(Top(b), 245u);
	// ... but when starving (InterruptWhenStarving, no disciple test)
	V(b).food = 0.2f;
	EXPECT_TRUE(villager::CheckHungry(b, 1002));
	EXPECT_EQ(Top(b), 117u);
	// a state that does not interrupt (36): no change
	auto c = MakeVillager({100.0f, 130.0f}, 36);
	V(c).food = 0.2f;
	EXPECT_FALSE(villager::CheckHungry(c, 1001));
	EXPECT_EQ(Top(c), 36u);
	// life to 0 with hunger: STARVING and 1; from worship (final 248): CHANT
	auto d = MakeVillager({100.0f, 130.0f}, 36);
	V(d).food = 0.2f;
	V(d).life = 0.0005f;
	EXPECT_TRUE(villager::CheckHungry(d, 1001));
	EXPECT_EQ(villager::GetDeathReason(d), DeathReason::Starving);
	auto e = MakeVillager({100.0f, 130.0f}, 248);
	V(e).food = 0.2f;
	V(e).life = 0.0005f;
	EXPECT_TRUE(villager::CheckHungry(e, 1001));
	EXPECT_EQ(villager::GetDeathReason(e), DeathReason::Chant);
}

TEST_F(VillagerFoodTest, Amounts)
{
	// the amounts worked out in float32
	EXPECT_EQ(villager::FoodToEat(0.5f, 85, std::nullopt), 74u);
	EXPECT_EQ(villager::FoodToEat(0.5f, 75, std::nullopt), 65u);
	EXPECT_EQ(villager::FoodToEat(0.5f, 85, 0.5f), 63u);
	EXPECT_EQ(villager::FoodToEat(0.45f, 85, 1.2f), 54u); // the town's desire clipped at 1
	EXPECT_EQ(villager::FoodToEat(0.45f, 85, -0.5f), villager::FoodToEat(0.45f, 85, std::nullopt)); // below 0: 0
	EXPECT_EQ(villager::FoodToEat(0.25f, 85, std::nullopt), 83u);
	EXPECT_EQ(villager::FoodToEat(0.0f, 85, std::nullopt), 85u);
	EXPECT_EQ(villager::FoodRequiredForMeal(74, 80), 0u);
	EXPECT_EQ(villager::FoodRequiredForMeal(74, 30), 44u);
}

TEST_F(VillagerFoodTest, ChangeStateToFindFoodToEat)
{
	const auto town = MakeTown();
	// need 0 (it carries enough): 117 outside, 118 inside
	auto a = MakeVillager();
	V(a).food = 0.5f;
	V(a).resourceHeld.at(0) = 80;
	EXPECT_EQ(villager::ChangeStateToFindFoodToEat(a), 1u);
	EXPECT_EQ(Top(a), 117u);
	V(a).flags = Villager::k_FlagAtHome;
	EXPECT_EQ(villager::ChangeStateToFindFoodToEat(a), 1u);
	EXPECT_EQ(Top(a), 118u);
	// a functional abode with 50 and 30 carried: need 74 - 30 = 44 <= 80 -> 36 (118 inside)
	const auto home = MakeAbode({60.0f, 60.0f}, 50);
	auto b = MakeVillager();
	V(b).food = 0.5f;
	V(b).town = town;
	V(b).abode = home;
	V(b).resourceHeld.at(0) = 30;
	EXPECT_EQ(villager::ChangeStateToFindFoodToEat(b), 1u);
	EXPECT_EQ(Top(b), 36u);
	V(b).flags = Villager::k_FlagAtHome;
	EXPECT_EQ(villager::ChangeStateToFindFoodToEat(b), 1u);
	EXPECT_EQ(Top(b), 118u);
	// the abode has 40 < 74: the town's storage pit with 100 -> 33
	MakePit(town, 100);
	const auto poor = MakeAbode({62.0f, 60.0f}, 40);
	auto c = MakeVillager();
	V(c).food = 0.5f;
	V(c).town = town;
	V(c).abode = poor;
	EXPECT_EQ(villager::ChangeStateToFindFoodToEat(c), 1u);
	EXPECT_EQ(Top(c), 33u);
}

TEST_F(VillagerFoodTest, ChangeStateToFindFoodToEatWithLittle)
{
	// the pit has 10 < 69 and it carries 5: it eats what it carries (117)
	const auto town = MakeTown();
	MakePit(town, 10);
	auto a = MakeVillager();
	V(a).food = 0.5f;
	V(a).town = town;
	V(a).resourceHeld.at(0) = 5;
	EXPECT_EQ(villager::ChangeStateToFindFoodToEat(a), 1u);
	EXPECT_EQ(Top(a), 117u);
	// nothing at all (no town, no abode): the drop-off point is where it stands -> 0, no change
	auto b = MakeVillager();
	V(b).food = 0.5f;
	EXPECT_EQ(villager::ChangeStateToFindFoodToEat(b), 0u);
	EXPECT_EQ(Top(b), 163u);
	// a town without a functional storage pit whose temporary store point is the villager's own position -> 0
	const auto other = MakeTown(2);
	auto c = MakeVillager();
	V(c).food = 0.5f;
	V(c).town = other;
	EXPECT_EQ(villager::ChangeStateToFindFoodToEat(c), 0u);
	EXPECT_EQ(Top(c), 163u);
	// the town's temporary pot elsewhere: walk to its edge with FINAL 34
	const auto pot = Reg().Create();
	Reg().Assign<Transform>(pot, glm::vec3(130.0f, 0.0f, 130.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	const auto edge = ecs::town_queries::ToMapCoords({130.0f, 130.0f});
	test::FakeVillagerStores stores(std::make_shared<ecs::systems::VillagerStores>());
	stores.temporaryStore = [pot, edge](entt::entity, const map_coords::MapCoords&, ResourceType) {
		return ecs::town_stores::TemporaryStore {pot, {edge.x, edge.y, 0.0f}};
	};
	Locator::villagerStores::emplace<test::FakeVillagerStores>(stores);
	EXPECT_EQ(villager::ChangeStateToFindFoodToEat(c), 1u);
	EXPECT_EQ(Top(c), 1u);
	EXPECT_EQ(Final(c), 34u);
}

TEST_F(VillagerFoodTest, EatFoodHeld)
{
	const auto town = MakeTown();
	// carries 74, eats 74 (food 0.5, 85): the belly full, nothing left, the town's food used += 74
	auto a = MakeVillager();
	V(a).food = 0.5f;
	V(a).town = town;
	V(a).resourceHeld.at(0) = 74;
	EXPECT_FLOAT_EQ(villager::EatFoodHeld(a), 1.0f);
	EXPECT_EQ(V(a).resourceHeld.at(0), 0);
	EXPECT_FLOAT_EQ(Reg().Get<Town>(town).foodUsed, 74.0f);
	// carries 30: 30 / 74 x 1.2 + 0.5 (0.9864865)
	auto b = MakeVillager();
	V(b).food = 0.5f;
	V(b).resourceHeld.at(0) = 30;
	const float expected = 30.0f / 74.0f * 1.2f + 0.5f;
	EXPECT_FLOAT_EQ(villager::EatFoodHeld(b), expected);
	EXPECT_NEAR(V(b).food, 0.9864865f, 1e-6f);
	// the pure layer: 0 / 0 (nothing to eat, nothing carried) is NaN -> 0
	EXPECT_FLOAT_EQ(villager::EatHeld(0.5f, 0, 0, 1.2f).food, 0.0f);
	// DropFood(0) drops all of it
	auto c = MakeVillager();
	V(c).resourceHeld.at(0) = 7;
	EXPECT_EQ(villager::DropFood(c, 0), 7u);
	EXPECT_EQ(V(c).resourceHeld.at(0), 0);
}

TEST_F(VillagerFoodTest, GetFoodFromHomeTakesTwice)
{
	// no town (no CallDesireFunction): the abode loses 74, the villager gains 148 (the literal double pick-up)
	const auto home = MakeAbode({100.0f, 130.0f}, 100, 9);
	auto a = MakeVillager();
	V(a).food = 0.5f;
	V(a).abode = home;
	V(a).flags = Villager::k_FlagAtHome;
	villager::GetFoodFromHome(a, 74);
	EXPECT_EQ(Reg().Get<Abode>(home).foodAmount, 26u);
	EXPECT_EQ(V(a).resourceHeld.at(0), 148);
	// then it eats 74: 74 left in its hands
	villager::EatFoodHeld(a);
	EXPECT_EQ(V(a).resourceHeld.at(0), 74);
}

TEST_F(VillagerFoodTest, EatingStates)
{
	// 117: EatFoodHeld, then PlayAnimThenSetState(163): TOP 23 WAIT_FOR_ANIMATION, FINAL 163
	auto a = MakeVillager({100.0f, 130.0f}, 117);
	V(a).food = 0.5f;
	V(a).resourceHeld.at(0) = 74;
	EXPECT_EQ(villager::EatFood(Action(a)), 1u);
	EXPECT_EQ(Top(a), 23u);
	EXPECT_EQ(Final(a), 163u);
	// poisoned: FINAL 212
	auto b = MakeVillager({100.0f, 130.0f}, 117);
	V(b).food = 0.5f;
	Reg().Assign<Poisoned>(b);
	villager::EatFood(Action(b));
	EXPECT_EQ(Final(b), 212u);
	// 118 at home: GetFoodFromHome, EatFoodHeld, 38 (212 poisoned)
	const auto home = MakeAbode({100.0f, 130.0f}, 100, 9);
	auto c = MakeVillager({100.0f, 130.0f}, 118);
	V(c).food = 0.5f;
	V(c).abode = home;
	V(c).flags = Villager::k_FlagAtHome;
	EXPECT_EQ(villager::EatFoodAtHome(Action(c)), 1u);
	EXPECT_EQ(Top(c), 38u);
	EXPECT_EQ(Reg().Get<Abode>(home).foodAmount, 26u);
	EXPECT_FLOAT_EQ(V(c).food, 1.0f);
	// 212 outside: PlayAnimThenSetState(163)
	auto d = MakeVillager({100.0f, 130.0f}, 212);
	villager::ShowPoisoned(Action(d));
	EXPECT_EQ(Top(d), 23u);
	EXPECT_EQ(Final(d), 163u);
}

TEST_F(VillagerFoodTest, ArrivesAtStoragePitForFood)
{
	// no town: GetStoragePit is its abode (functional, 50 food); need 74: it takes the 50 (all of m = min(74, 50): 1)
	// and walks back to the door with FINAL 163
	const auto home = MakeAbode({100.0f, 130.0f}, 50, 9);
	auto a = MakeVillager({100.0f, 130.0f}, 34);
	V(a).food = 0.5f;
	V(a).abode = home;
	EXPECT_EQ(villager::ArrivesAtStoragePitForFood(Action(a)), 1u);
	EXPECT_EQ(V(a).resourceHeld.at(0), 50);
	EXPECT_EQ(Reg().Get<Abode>(home).foodAmount, 0u);
	EXPECT_EQ(Top(a), 1u);
	EXPECT_EQ(Final(a), 163u);
	// again with the abode empty: m = 0 -> 163 and 0
	Action(a).states.at(0) = 34;
	Action(a).states.at(1) = 0;
	EXPECT_EQ(villager::ArrivesAtStoragePitForFood(Action(a)), 0u);
	EXPECT_EQ(Top(a), 163u);
	// carrying enough already: need 0 -> 163 and 1
	auto b = MakeVillager({100.0f, 130.0f}, 34);
	V(b).food = 0.5f;
	V(b).resourceHeld.at(0) = 80;
	EXPECT_EQ(villager::ArrivesAtStoragePitForFood(Action(b)), 1u);
	EXPECT_EQ(Top(b), 163u);
}
