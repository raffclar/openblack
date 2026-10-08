/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Villager birth, the live part (docs/bw1-notes/villagers.md): the pregnancy at home (WillHousewifeGetPregnant,
// HousewifeGetsPregnant), the count down and the birth (UpdatePregnancy, HousewifeStartsGivingBirth, HousewifeGivingBirth,
// HousewifeGivenBirth), ChildBorn's links (abode, town, vagrants, mother, poison), the child's decision
// (ChildDecideWhatToDo, IsMotherAlive) and the creche (ChildGotoCreche, ChildAtCreche, the promenade step) with a plain
// functional abode as the town's creche. A fake state table in the Locator, scripted draws and a child factory (the
// fixture of test_villager_home.cpp). Every expected value is the original game's.
// The player's birth statistic has no API yet (TODO(Intro)): not tested.

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>

#include <algorithm>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "Common/GameRandomTesting.h"
#include "Common/RandomNumberManager.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Life.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Life.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/VillagerDiscipleJobs.h"
#include "ECS/Systems/Implementations/VillagerRules.h"
#include "ECS/Systems/Implementations/VillagerWorldQueries.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Town/TownVillagers.h"
#include "ECS/Villager/VillagerAge.h"
#include "ECS/Villager/VillagerBirth.h"
#include "ECS/Villager/VillagerChild.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerDecide.h"
#include "ECS/Villager/VillagerHome.h"
#include "Enums.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/MapFakes.h"
#include "support/TestServices.h"
#include "support/VillagerFakes.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace villager = openblack::ecs::villager;
namespace tq = openblack::ecs::town_queries;
namespace av = openblack::ecs::abode_villagers;
namespace tv = openblack::ecs::town_villagers;
using Index = LivingAction::Index;

namespace
{
class TestRng final: public RandomNumberManagerInterface
{
	std::mt19937 _generator {1};
	std::mt19937& Generator() override { return _generator; }
	std::optional<std::reference_wrapper<std::mutex>> LockAccess() override { return std::nullopt; }
};

/// A state table that records the calls; the exit of the at-home rows (35..38, 118..121) is the real ExitAtHome
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
	uint32_t VillagerCallExit(LivingAction& action, VillagerStates row, VillagerStates next) const override
	{
		const auto r = static_cast<uint32_t>(row);
		calls.push_back("exit " + std::to_string(r) + " " + std::to_string(static_cast<uint32_t>(next)));
		if ((r >= 35 && r <= 38) || (r >= 118 && r <= 121))
		{
			return villager::ExitAtHome(action, next);
		}
		return 1;
	}
	int VillagerCallOutOfAnimation(LivingAction&, Index) const override { return -1; }
	bool VillagerCallValidate(LivingAction&, Index) const override { return false; }
};

constexpr auto S(uint32_t n)
{
	return static_cast<VillagerStates>(n);
}

constexpr uint32_t k_Turn = 1000;

class VillagerBirthTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::EmplaceMapAndVillagerDefaults();
		auto info = std::make_unique<InfoConstants>();
		// row 0: the Celtic housewife (a woman); row 1: the Celtic forester (a man); row 2: an African woman
		for (size_t i = 0; i < 3; ++i)
		{
			auto& v = info->villager.at(i);
			v.hungryForFood = 0.5f;
			v.starvingForFood = 0.25f;
			v.processChecksEvery = 8;
			v.damageThresholdToGoHome = 0.3f;
			v.damageThresholdToSleepUntil = 0.7f;
			v.ownDesireThreshold = 0.3f;
			v.restAtHomeTime = 100;
			v.startHavingSexAge = 13;
			v.stopHavingSexAge = 100;
			v.grownUpAge = 13;
			v.oldAge = 60;
			v.retirementAge = 100;
			v.timePregnantFor = 999;
			v.boyGirlChance = 50;
			v.life = 1.0f;
			v.moveState = LivingStates::LivingMoveToPos;
		}
		info->villager.at(0).sex = SexType::Female;
		info->villager.at(1).sex = SexType::Male;
		info->villager.at(1).villagerNumber = VillagerNumber::Forester;
		info->villager.at(2).sex = SexType::Female;
		info->villager.at(2).tribeType = Tribe::AFRICAN;
		// no row pauses (no GameFloatRand from SetTopState)
		for (auto& row : info->villagerStateTable)
		{
			row.animation = 385;
			row.field0x4 = -1;
			row.field0x8 = 1.0f;
			row.field0xf0 = 1;
		}
		auto& table = info->villagerStateTable;
		for (const uint32_t s : {36u, 37u, 38u, 110u, 111u, 112u, 113u, 114u, 119u, 120u, 129u, 130u, 163u, 234u})
		{
			table.at(s).isFinalState = 1;
		}
		table.at(1).field0x14 = 1;
		// StaysAtHomeOnExit: 35, 36, 37, 38, 118..121
		for (const uint32_t s : {35u, 36u, 37u, 38u, 118u, 119u, 120u, 121u})
		{
			table.at(s).staysAtHomeOnExit = 1;
		}
		info->town.gameTurnsAfterEmergencyVillagersReact = 100;
		auto& abode = info->abode.at(0);
		abode.abodeNumber = AbodeNumber::A;
		abode.tribeType = Tribe::CELTIC;
		abode.maxVillagersInAbode = 4;
		abode.maxChildrenInAbode = 1;
		abode.percentTooCrowded = 0.9f;
		abode.thresholdForStopBeingFunctional = 0.75f;
		Locator::infoConstants::reset(info.release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		Locator::rng::emplace<TestRng>();
		Locator::livingActionSystem::emplace<FakeStateTable>();
		// a filler, so that no villager or town is entity 0
		Reg().Create();
		game_clock::SetTurn(k_Turn);
		// The villager services the code under test reaches: the game's, unless faked here
		Locator::villagerRules::emplace<ecs::systems::VillagerRules>();
		Locator::villagerDiscipleJobs::emplace<ecs::systems::VillagerDiscipleJobs>();
		test::FakeVillagerWorldQueries world(std::make_shared<ecs::systems::VillagerWorldQueries>());
		world.isVisualNight = [] { return false; };
		Locator::villagerWorldQueries::emplace<test::FakeVillagerWorldQueries>(world);
		test::FakeVillagerWorshipCheck worship;
		worship.worshipCheck = [](entt::entity) { return false; };
		Locator::villagerWorshipCheck::emplace<test::FakeVillagerWorshipCheck>(worship);
		SetDraws({}, {});
		test::FakeTownCellObjects cellObjects;
		cellObjects.objectsInCell = [](glm::ivec2) { return std::vector<entt::entity> {}; };
		cellObjects.get2DRadius = [](entt::entity) { return 0.0f; };
		Locator::townCellObjects::emplace<test::FakeTownCellObjects>(cellObjects);
		// A bare villager of the info's row, age 1 (a child), in CREATED (85)
		test::FakeVillagerChildFactory children;
		children.createChild = [this](const glm::vec3& at, VillagerInfo row, uint32_t age) {
			_childRow = row;
			_childAge = age;
			_childAt = at;
			const auto& info = Locator::infoConstants::value().villager.at(static_cast<size_t>(row));
			const auto e = MakeVillager({at.x, at.z}, 85);
			V(e).tribe = info.tribeType;
			V(e).number = info.villagerNumber;
			V(e).birthTurn = villager::BirthTurnForAge(age, k_Turn);
			V(e).flags = Villager::k_FlagChild;
			return e;
		};
		Locator::villagerChildFactory::emplace<test::FakeVillagerChildFactory>(children);
		tv::ClearVagrants();
	}

	void TearDown() override
	{
		tv::ClearVagrants();
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

	/// Scripted draws: GameRand and GameFloatRand take the next value of their list (0 when it is empty) and log the call
	void SetDraws(std::deque<uint32_t> ints, std::deque<float> floats)
	{
		_ints = std::move(ints);
		_floats = std::move(floats);
		_draws.clear();
		game_random::testing::SetGameRand(
		    [this](uint32_t n) {
			    _draws.push_back("R" + std::to_string(n));
			    if (_ints.empty())
			    {
				    return 0u;
			    }
			    const auto r = _ints.front();
			    _ints.pop_front();
			    return r;
		    },
		    [this](float x) {
			    _draws.push_back("F" + std::to_string(x).substr(0, 3));
			    if (_floats.empty())
			    {
				    return 0.0f;
			    }
			    const auto r = _floats.front();
			    _floats.pop_front();
			    return r;
		    });
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	/// A villager of row 0 (the Celtic housewife) aged 25
	static entt::entity MakeVillager(glm::vec2 at = {60.0f, 60.0f}, uint32_t top = 38, uint32_t final = 0)
	{
		auto& registry = Reg();
		const auto e = registry.Create();
		auto& v = registry.Assign<Villager>(e);
		v.life = 1.0f;
		v.town = entt::null;
		v.abode = entt::null;
		v.food = 1.0f;
		v.lastCheckTurn = k_Turn;
		v.birthTurn = villager::BirthTurnForAge(25, k_Turn);
		auto& action = registry.Assign<LivingAction>(e, S(top), static_cast<uint16_t>(0));
		action.states.at(1) = static_cast<uint8_t>(final);
		registry.Assign<Transform>(e, glm::vec3(at.x, 0.0f, at.y), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<WallHug>(e, glm::vec2(), glm::vec2(), 0.0f, 0.1f);
		registry.Assign<Mobile>(e);
		return e;
	}

	/// A child (the child flag) of row 0, aged 5
	static entt::entity MakeChild(glm::vec2 at = {70.0f, 60.0f}, uint32_t top = 163)
	{
		const auto e = MakeVillager(at, top);
		V(e).flags = Villager::k_FlagChild;
		V(e).birthTurn = villager::BirthTurnForAge(5, k_Turn);
		return e;
	}

	static entt::entity MakeTown(glm::vec2 at = {50.0f, 50.0f}, uint32_t id = 1)
	{
		auto& registry = Reg();
		const auto town = registry.Create();
		registry.Assign<Town>(town, id);
		registry.Assign<Tribe>(town, Tribe::CELTIC);
		registry.Assign<Transform>(town, glm::vec3(at.x, 0.0f, at.y), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Context().towns[id] = town;
		return town;
	}

	/// A built abode of the Celtic A row (functional at life 1); the creche tests use one as the town's creche
	static entt::entity MakeAbode(glm::vec2 at, float life = 1.0f, uint32_t townId = 1)
	{
		auto& registry = Reg();
		const auto abode = registry.Create();
		ecs::object_index::Assign(abode);
		registry.Assign<Abode>(abode, AbodeNumber::A, townId, 0u, 0u);
		registry.Assign<Transform>(abode, glm::vec3(at.x, 0.0f, at.y), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<Life>(abode, life);
		return abode;
	}

	static entt::entity MakeCreche(entt::entity town, glm::vec2 at, float life = 1.0f)
	{
		const auto creche = MakeAbode(at, life);
		Reg().Get<Town>(town).creche = creche;
		return creche;
	}

	static void Home(entt::entity v, entt::entity abode, bool inside)
	{
		av::AddVillagerToAbode(abode, v);
		if (inside)
		{
			villager::ArriveHome(v);
		}
	}

	static LivingAction& Action(entt::entity e) { return Reg().Get<LivingAction>(e); }
	static Villager& V(entt::entity e) { return Reg().Get<Villager>(e); }
	static uint32_t Top(entt::entity e) { return Action(e).states.at(0); }
	static uint32_t Final(entt::entity e) { return Action(e).states.at(1); }
	static glm::vec2 Goal(entt::entity e) { return Reg().Get<WallHug>(e).goal; }
	static uint16_t Counter(entt::entity e) { return Action(e).turnsUntilStateChange; }

	std::deque<uint32_t> _ints;
	std::deque<float> _floats;
	std::vector<std::string> _draws;
	VillagerInfo _childRow {VillagerInfo::None};
	uint32_t _childAge {0};
	glm::vec3 _childAt {0.0f};
};
} // namespace

// ---- the pure layer ----------------------------------------------------------------------------------------------

TEST_F(VillagerBirthTest, BirthCounter)
{
	// GameRand(ftol(36000.0f / 365.25f = 98.5626f)) = GameRand(98)
	EXPECT_EQ(villager::BirthCounterRange(), 98u);
	// ftol(r + 24.6407f + 1): r + 25
	EXPECT_EQ(villager::BirthCounter(0), 25);
	EXPECT_EQ(villager::BirthCounter(97), 122);
}

TEST_F(VillagerBirthTest, PromenadeStep)
{
	// at the door p = GameRand(n), k = 0, point 0
	SetDraws({2}, {});
	auto s = villager::NextPromenadeStep(0x50003, 3, true);
	EXPECT_EQ(_draws, std::vector<std::string>({"R3"}));
	EXPECT_EQ(s.path, 2);
	EXPECT_EQ(s.step, 0);
	EXPECT_EQ(s.point, 0);
	EXPECT_EQ(s.index, 2);
	// a draw outside [0, n) -> 0
	SetDraws({5}, {});
	EXPECT_EQ(villager::NextPromenadeStep(0, 3, true).path, 0);
	// on the way: ++k; k 1..4 -> j = k; 5..9 -> j = 9 - k; no draw
	SetDraws({}, {});
	s = villager::NextPromenadeStep((3 << 16) | 1, 3, false);
	EXPECT_EQ(s.step, 4);
	EXPECT_EQ(s.point, 4);
	s = villager::NextPromenadeStep((4 << 16) | 1, 3, false);
	EXPECT_EQ(s.step, 5);
	EXPECT_EQ(s.point, 4);
	EXPECT_EQ(s.index, (5 << 16) | 1);
	s = villager::NextPromenadeStep((8 << 16) | 1, 3, false);
	EXPECT_EQ(s.point, 0);
	EXPECT_TRUE(_draws.empty());
	// k > 9 -> k = 0, p = GameRand(n)
	SetDraws({1}, {});
	s = villager::NextPromenadeStep((9 << 16) | 2, 3, false);
	EXPECT_EQ(_draws, std::vector<std::string>({"R3"}));
	EXPECT_EQ(s.path, 1);
	EXPECT_EQ(s.step, 0);
	EXPECT_EQ(s.index, 1);
	// p past the paths -> n - 1; k negative after ++ -> 0
	SetDraws({}, {});
	s = villager::NextPromenadeStep(7, 3, false);
	EXPECT_EQ(s.path, 2);
	EXPECT_EQ(s.step, 1);
	s = villager::NextPromenadeStep(static_cast<int32_t>(0xFFFE0001u), 3, false);
	EXPECT_EQ(s.step, 0);
	EXPECT_EQ(s.point, 0);
	EXPECT_EQ(s.path, 1);
	// no path (n 0) and not at the door: p = n - 1 = -1, so the index is (1 << 16) | 0xFFFFFFFF = -1 (literal)
	s = villager::NextPromenadeStep(0, 0, false);
	EXPECT_EQ(s.path, -1);
	EXPECT_EQ(s.index, -1);
}

// ---- the pregnancy -----------------------------------------------------------------------------------------------

TEST_F(VillagerBirthTest, WillHousewifeGetPregnant)
{
	const auto town = MakeTown();
	const auto abode = MakeAbode({60.0f, 60.0f});
	auto a = MakeVillager();
	Home(a, abode, true);
	// the FOR_CHILDREN (8) significance 0 -> 0
	EXPECT_FALSE(villager::WillHousewifeGetPregnant(a));
	// above the trigger (0) and room for a child (MaxChildrenInAbode 1 > 0) -> 1
	Reg().Get<Town>(town).desire.desire.at(8) = 0.5f;
	EXPECT_TRUE(villager::WillHousewifeGetPregnant(a));
	// pregnant already -> 0
	V(a).pregnancy = 10;
	EXPECT_FALSE(villager::WillHousewifeGetPregnant(a));
	V(a).pregnancy = 0;
	// another pregnant woman of the abode counts as a child: 1 >= MaxChildren 1 -> 0
	auto b = MakeVillager();
	Home(b, abode, true);
	V(b).pregnancy = 10;
	EXPECT_FALSE(villager::WillHousewifeGetPregnant(a));
	V(b).pregnancy = 0;
	EXPECT_TRUE(villager::WillHousewifeGetPregnant(a));
	// a child in the abode -> 0
	auto c = MakeChild();
	Home(c, abode, false);
	EXPECT_FALSE(villager::WillHousewifeGetPregnant(a));
	// a town but no abode -> 0
	auto d = MakeVillager();
	V(d).town = town;
	EXPECT_FALSE(villager::WillHousewifeGetPregnant(d));
}

TEST_F(VillagerBirthTest, HousewifeGetsPregnant)
{
	MakeTown();
	const auto abode = MakeAbode({60.0f, 60.0f});
	// at home: the pregnancy = TimePregnantFor (999); she stays
	auto a = MakeVillager({60.0f, 60.0f}, 38);
	Home(a, abode, true);
	EXPECT_EQ(villager::HousewifeGetsPregnant(a), 1u);
	EXPECT_EQ(V(a).pregnancy, 999);
	EXPECT_TRUE(villager::IsPregnant(a));
	EXPECT_EQ(Top(a), 38u);
	// not at home: GoHome (the walk to the door, FINAL 37)
	auto b = MakeVillager({90.0f, 60.0f}, 163);
	Home(b, abode, false);
	EXPECT_EQ(villager::HousewifeGetsPregnant(b), 1u);
	EXPECT_EQ(V(b).pregnancy, 999);
	EXPECT_EQ(Final(b), 37u);
}

TEST_F(VillagerBirthTest, CheckGetPregnantAtHome)
{
	const auto town = MakeTown();
	const auto abode = MakeAbode({60.0f, 60.0f});
	auto a = MakeVillager();
	Home(a, abode, true);
	// WillHousewifeGetPregnant == 0 -> 0, nothing
	EXPECT_EQ(villager::CheckGetPregnantAtHome(a), 0u);
	EXPECT_EQ(V(a).pregnancy, 0);
	// == 1 -> HousewifeGetsPregnant
	Reg().Get<Town>(town).desire.desire.at(8) = 0.5f;
	EXPECT_NE(villager::CheckGetPregnantAtHome(a), 0u);
	EXPECT_EQ(V(a).pregnancy, 999);
}

// ---- the birth ---------------------------------------------------------------------------------------------------

TEST_F(VillagerBirthTest, PregnancyCountDownStartsTheBirth)
{
	auto a = MakeVillager({60.0f, 60.0f}, 38);
	// UpdatePregnancy: 20 - 9 turns since the last check -> 11, 0
	V(a).pregnancy = 20;
	V(a).lastCheckTurn = k_Turn - 9;
	EXPECT_EQ(villager::UpdatePregnancy(a), 0u);
	EXPECT_EQ(V(a).pregnancy, 11);
	EXPECT_EQ(Top(a), 38u);
	// 5 - 9 <= 0 -> HousewifeStartsGivingBirth: the pregnancy = 0, GameRand(98) = 40 -> the
	// counter 65, SetTopState(111), HousewifeGivingBirth's first turn (64, not 0 -> 1)
	V(a).pregnancy = 5;
	SetDraws({40}, {});
	EXPECT_EQ(villager::UpdatePregnancy(a), 1u);
	EXPECT_EQ(_draws.at(0), "R98");
	EXPECT_EQ(V(a).pregnancy, 0);
	EXPECT_FALSE(villager::IsPregnant(a));
	EXPECT_EQ(Top(a), 111u);
	EXPECT_EQ(Counter(a), 64);
	// 111 counts down; not 0 -> 1, no birth
	EXPECT_EQ(villager::HousewifeGivingBirthState(Action(a)), 1u);
	EXPECT_EQ(Counter(a), 63);
	EXPECT_EQ(Top(a), 111u);
}

TEST_F(VillagerBirthTest, GivingBirthToGivenBirth)
{
	const auto town = MakeTown();
	const auto abode = MakeAbode({60.0f, 60.0f});
	auto a = MakeVillager({60.0f, 60.0f}, 111);
	Home(a, abode, true);
	Action(a).turnsUntilStateChange = 1;
	// the counter down to 0 -> ChildBorn; SetTopState(112); 1
	SetDraws({60}, {});
	EXPECT_EQ(villager::HousewifeGivingBirthState(Action(a)), 1u);
	EXPECT_EQ(_draws.at(0), "R100");
	EXPECT_EQ(Top(a), 112u);
	ASSERT_EQ(av::VillagersOf(abode).size(), 2u);
	const auto child = av::VillagersOf(abode).front();
	EXPECT_TRUE(V(child).mother == a);
	EXPECT_TRUE(V(child).town == town);
	// 112: HousewifeGivenBirth: the pregnancy = 0, GoHome: inside -> 38
	V(a).pregnancy = 3;
	EXPECT_EQ(villager::HousewifeGivenBirth(Action(a)), 1u);
	EXPECT_EQ(V(a).pregnancy, 0);
	EXPECT_EQ(Top(a), 38u);
}

TEST_F(VillagerBirthTest, ChildBornIntoTheAbode)
{
	const auto town = MakeTown();
	const auto abode = MakeAbode({60.0f, 60.0f});
	auto a = MakeVillager({61.0f, 62.0f}, 111);
	Home(a, abode, true);
	// r = 60 > BoyGirlChance 50 -> the mother's info; no second draw
	SetDraws({60}, {});
	const auto child = villager::ChildBorn(a);
	ASSERT_TRUE(child != entt::null);
	ASSERT_GE(_draws.size(), 1u);
	EXPECT_EQ(_draws.at(0), "R100");
	EXPECT_TRUE(std::find(_draws.begin(), _draws.end(), "R6") == _draws.end());
	EXPECT_EQ(_childRow, VillagerInfo::CelticHousewifeFemale);
	// the child is made at the mother's position, aged 1
	EXPECT_EQ(_childAge, 1u);
	EXPECT_FLOAT_EQ(_childAt.x, 61.0f);
	EXPECT_FLOAT_EQ(_childAt.z, 62.0f);
	// AddVillagerToAbode: the head of the list, the abode's town, ChildCount 1; the mother linked
	EXPECT_TRUE(av::VillagersOf(abode).front() == child);
	EXPECT_TRUE(V(child).abode == abode);
	EXPECT_TRUE(V(child).town == town);
	EXPECT_EQ(Reg().Get<Abode>(abode).childCount, 1);
	EXPECT_TRUE(V(child).mother == a);
	// ChildDecideWhatToDo: no creche -> 114 CHILD_FOLLOWS_MOTHER
	EXPECT_EQ(Top(child), 114u);
	EXPECT_FALSE(ecs::life::IsPoisoned(child));
}

TEST_F(VillagerBirthTest, ChildBornNumber)
{
	MakeTown();
	const auto abode = MakeAbode({60.0f, 60.0f});
	auto a = MakeVillager({60.0f, 60.0f}, 111);
	Home(a, abode, true);
	// r = 50 <= 50 -> the Celtic row of number GameRand(6) + 1: 0 + 1 = FORESTER, row 1
	SetDraws({50, 0}, {});
	villager::ChildBorn(a);
	ASSERT_GE(_draws.size(), 2u);
	EXPECT_EQ(_draws.at(0), "R100");
	EXPECT_EQ(_draws.at(1), "R6");
	EXPECT_EQ(_childRow, static_cast<VillagerInfo>(1));
	// 4 + 1 = LEADER: no Celtic leader in this table -> the mother's info
	SetDraws({10, 4}, {});
	villager::ChildBorn(a);
	EXPECT_EQ(_childRow, VillagerInfo::CelticHousewifeFemale);
}

TEST_F(VillagerBirthTest, ChildBornTownVagrantPoison)
{
	// no abode, a town: AddVillagerToTown (no abode with room: homeless in that town)
	const auto town = MakeTown();
	auto a = MakeVillager({60.0f, 60.0f}, 111);
	V(a).town = town;
	SetDraws({60}, {});
	const auto c1 = villager::ChildBorn(a);
	EXPECT_TRUE(V(c1).town == town);
	EXPECT_TRUE(V(c1).abode == entt::null);
	EXPECT_TRUE(V(c1).mother == a);
	EXPECT_FALSE(tv::IsVagrant(c1));
	// neither: the head of the vagrants; a poisoned mother: SetPoisoned(1)
	auto b = MakeVillager({90.0f, 90.0f}, 111);
	ecs::life::SetPoisoned(b, true);
	SetDraws({60}, {});
	const auto c2 = villager::ChildBorn(b);
	EXPECT_TRUE(tv::IsVagrant(c2));
	ASSERT_FALSE(tv::Vagrants().empty());
	EXPECT_TRUE(tv::Vagrants().front() == c2);
	EXPECT_TRUE(V(c2).mother == b);
	EXPECT_TRUE(ecs::life::IsPoisoned(c2));
}

// ---- the child ---------------------------------------------------------------------------------------------------

TEST_F(VillagerBirthTest, IsMotherAlive)
{
	auto c = MakeChild();
	// no mother -> 0
	EXPECT_EQ(villager::IsMotherAlive(c), 0u);
	// available, the same tribe, a woman, not dead -> 1
	auto m = MakeVillager();
	V(c).mother = m;
	EXPECT_EQ(villager::IsMotherAlive(c), 1u);
	// dead -> 0
	V(m).status = Villager::k_StatusDead;
	EXPECT_EQ(villager::IsMotherAlive(c), 0u);
	V(m).status = 0;
	// a man (row 1, not a mother) -> 0
	V(m).number = VillagerNumber::Forester;
	EXPECT_EQ(villager::IsMotherAlive(c), 0u);
	// another tribe (row 2) -> 0
	V(m).number = VillagerNumber::Housewife;
	V(m).tribe = Tribe::AFRICAN;
	EXPECT_EQ(villager::IsMotherAlive(c), 0u);
	// CheckChild: not alive -> the mother unlinked
	EXPECT_EQ(villager::CheckChild(c), 0u);
	EXPECT_TRUE(V(c).mother == entt::null);
}

TEST_F(VillagerBirthTest, ChildDecideWhatToDo)
{
	const auto town = MakeTown();
	const auto abode = MakeAbode({60.0f, 60.0f});
	// not a child: CheckChild -> GoHome (FINAL 37), nothing more
	auto a = MakeVillager({90.0f, 60.0f}, 163);
	Home(a, abode, false);
	EXPECT_EQ(villager::ChildDecideWhatToDo(a), 1u);
	EXPECT_EQ(Final(a), 37u);
	// a child, no creche -> SetTopState(114)
	auto c = MakeChild({90.0f, 60.0f});
	V(c).town = town;
	EXPECT_EQ(villager::ChildDecideWhatToDo(c), 1u);
	EXPECT_EQ(Top(c), 114u);
	// a hungry child: GoHome
	auto d = MakeChild({90.0f, 60.0f});
	Home(d, abode, false);
	V(d).food = 0.4f;
	villager::ChildDecideWhatToDo(d);
	EXPECT_EQ(Final(d), 37u);
	// a functional creche: ChildGotoCreche -> the walk to its door (no mesh: its position), FINAL 113
	MakeCreche(town, {80.0f, 80.0f});
	auto e = MakeChild({90.0f, 60.0f});
	V(e).town = town;
	EXPECT_EQ(villager::ChildDecideWhatToDo(e), 1u);
	EXPECT_EQ(Top(e), 1u);
	EXPECT_EQ(Final(e), 113u);
	EXPECT_NEAR(Goal(e).x, 80.0f, 0.01f);
	EXPECT_NEAR(Goal(e).y, 80.0f, 0.01f);
}

TEST_F(VillagerBirthTest, ChildGotoCreche)
{
	const auto town = MakeTown();
	auto c = MakeChild();
	// no town -> 0
	EXPECT_EQ(villager::ChildGotoCreche(c), 0u);
	V(c).town = town;
	// no creche -> 0
	EXPECT_EQ(villager::ChildGotoCreche(c), 0u);
	// not functional (life 0.5 <= 0.75) -> 0
	MakeCreche(town, {80.0f, 80.0f}, 0.5f);
	EXPECT_EQ(villager::ChildGotoCreche(c), 0u);
	EXPECT_EQ(Top(c), 163u);
	MakeCreche(town, {80.0f, 80.0f});
	EXPECT_EQ(villager::ChildGotoCreche(c), 1u);
	EXPECT_EQ(Final(c), 113u);
}

TEST_F(VillagerBirthTest, ChildAtCrecheByDay)
{
	const auto town = MakeTown();
	const auto creche = MakeCreche(town, {80.0f, 80.0f});
	// at the door: the promenade's GameRand(n) (n = 0 without the mesh: R0), out = the door; SetupMoveToPos(113); 1
	auto c = MakeChild({80.0f, 80.0f}, 113);
	V(c).town = town;
	SetDraws({}, {});
	EXPECT_EQ(villager::ChildAtCreche(Action(c)), 1u);
	ASSERT_FALSE(_draws.empty());
	EXPECT_EQ(_draws.back(), "R0");
	EXPECT_EQ(V(c).crecheWalk, 0);
	EXPECT_EQ(Top(c), 1u);
	EXPECT_EQ(Final(c), 113u);
	// away from the door: no draw; p = n - 1 = -1, k = 1: the index -1 (literal for a creche without paths)
	auto d = MakeChild({90.0f, 80.0f}, 113);
	V(d).town = town;
	SetDraws({}, {});
	EXPECT_EQ(villager::ChildAtCreche(Action(d)), 1u);
	EXPECT_TRUE(std::find(_draws.begin(), _draws.end(), "R0") == _draws.end());
	EXPECT_EQ(V(d).crecheWalk, -1);
	EXPECT_EQ(Final(d), 113u);
	EXPECT_NEAR(Goal(d).x, 80.0f, 0.01f);
	EXPECT_NEAR(Goal(d).y, 80.0f, 0.01f);
	// no town -> 0
	auto e = MakeChild({80.0f, 80.0f}, 113);
	EXPECT_EQ(villager::ChildAtCreche(Action(e)), 0u);
	EXPECT_EQ(Top(e), 113u);
	(void)creche;
}

TEST_F(VillagerBirthTest, ChildAtCrecheByNight)
{
	const auto town = MakeTown();
	const auto abode = MakeAbode({60.0f, 60.0f});
	MakeCreche(town, {80.0f, 80.0f});
	test::FakeVillagerWorldQueries night(std::make_shared<ecs::systems::VillagerWorldQueries>());
	night.isVisualNight = [] { return true; };
	Locator::villagerWorldQueries::emplace<test::FakeVillagerWorldQueries>(night);
	// an abode whose first inhabitant is at home -> GoHome (FINAL 37); 0
	auto m = MakeVillager();
	Home(m, abode, true);
	auto c = MakeChild({80.0f, 80.0f}, 113);
	V(c).town = town;
	V(c).abode = abode;
	EXPECT_EQ(villager::ChildAtCreche(Action(c)), 0u);
	EXPECT_EQ(Final(c), 37u);
	// the first inhabitant outside -> 0, nothing
	villager::LeaveHome(m);
	auto d = MakeChild({80.0f, 80.0f}, 113);
	V(d).town = town;
	V(d).abode = abode;
	EXPECT_EQ(villager::ChildAtCreche(Action(d)), 0u);
	EXPECT_EQ(Top(d), 113u);
	// no abode, touching the creche (0.001): the next promenade point, FINAL 113; still 0
	auto e = MakeChild({80.0f, 80.0f}, 113);
	V(e).town = town;
	EXPECT_EQ(villager::ChildAtCreche(Action(e)), 0u);
	EXPECT_EQ(Top(e), 1u);
	EXPECT_EQ(Final(e), 113u);
	// no abode, far from it: 0, nothing
	auto f = MakeChild({200.0f, 200.0f}, 113);
	V(f).town = town;
	EXPECT_EQ(villager::ChildAtCreche(Action(f)), 0u);
	EXPECT_EQ(Top(f), 113u);
}
