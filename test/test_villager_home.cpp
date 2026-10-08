/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The villager's home (docs/bw1-notes/villagers.md):
// 37 / 38 / ExitAtHome with PresentAtHome, HomeDecideWhatToDo, sleeping, CheckWhenGoingToBed, the tent by a tree,
// DoGoingHome without an abode, the abode's score and list, CheckNeedNewAbode, 238, 234, the shuffle, the abode's turn.
// A fake state table in the Locator (it runs the real ExitAtHome for the rows that have it) and scripted draws.

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>

#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/gtc/constants.hpp>
#include <gtest/gtest.h>

#include "Common/GUtilsDistance.h"
#include "Common/GameRandomTesting.h"
#include "Common/RandomNumberManager.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Life.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Poisoned.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/VillagerChildFactory.h"
#include "ECS/Systems/Implementations/VillagerDiscipleJobs.h"
#include "ECS/Systems/Implementations/VillagerRules.h"
#include "ECS/Systems/Implementations/VillagerWorldQueries.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Town/TownVillagers.h"
#include "ECS/Villager/VillagerAge.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerDecide.h"
#include "ECS/Villager/VillagerHome.h"
#include "EngineConfig.h"
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
	[[nodiscard]] size_t Count(const std::string& call) const
	{
		return static_cast<size_t>(std::count(calls.begin(), calls.end(), call));
	}
};

constexpr auto S(uint32_t n)
{
	return static_cast<VillagerStates>(n);
}

glm::vec2 Metres(glm::ivec2 pos)
{
	return tq::ToMetres(pos);
}

class VillagerHomeTest: public ::testing::Test
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
		v.hungerToLifeMultiplier = 0.001f;
		v.foodReqiredForDinner = 85;
		v.foodNurishmentMultiplier = 1.2f;
		v.ownDesireThreshold = 0.3f;
		v.restAtHomeTime = 100;
		v.restAtHomeRestoresLifeBy = 0.05f;
		v.startHavingSexAge = 13;
		v.stopHavingSexAge = 100;
		v.grownUpAge = 13;
		v.oldAge = 60;
		v.retirementAge = 100;
		v.sex = SexType::Male;
		v.life = 1.0f;
		v.moveState = LivingStates::LivingMoveToPos;
		for (auto& row : info->villagerStateTable)
		{
			row.animation = 385;
			row.field0x4 = -1;
			row.field0x8 = 1.0f;
			row.field0xf0 = 1;
		}
		auto& table = info->villagerStateTable;
		for (const uint32_t s :
		     {33u, 34u, 36u, 37u, 38u, 117u, 118u, 119u, 120u, 121u, 129u, 130u, 163u, 212u, 234u, 238u, 245u, 246u})
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
		info->town.maxDistanceFromCongreationPosThatPeopleChillOut = 50.0f;
		info->town.maxDistanceFromHouseThatPeopleChillOut = 10.0f;
		info->town.shuffleVillagersEvery = 5000;
		auto& abode = info->abode.at(0);
		abode.abodeNumber = AbodeNumber::A;
		abode.tribeType = Tribe::CELTIC;
		abode.maxVillagersInAbode = 2;
		abode.maxChildrenInAbode = 1;
		abode.percentTooCrowded = 0.5f;
		abode.emptyAbodeLifeReducer = 0.0001f;
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
		Locator::villagerChildFactory::emplace<ecs::systems::VillagerChildFactory>();
		Locator::villagerDiscipleJobs::emplace<ecs::systems::VillagerDiscipleJobs>();
		test::FakeVillagerWorshipCheck worship;
		worship.worshipCheck = [](entt::entity) { return false; };
		Locator::villagerWorshipCheck::emplace<test::FakeVillagerWorshipCheck>(worship);
		SetDraws({}, {});
		test::FakeTownCellObjects cellObjects;
		cellObjects.objectsInCell = [](glm::ivec2 cell) {
			std::vector<entt::entity> list;
			Locator::entitiesRegistry::value().Each<const Transform>([&](entt::entity e, const Transform& t) {
				if (static_cast<int>(std::floor(t.position.x / 10.0f)) == cell.x &&
				    static_cast<int>(std::floor(t.position.z / 10.0f)) == cell.y)
				{
					list.push_back(e);
				}
			});
			return list;
		};
		cellObjects.get2DRadius = [](entt::entity) { return 0.0f; };
		Locator::townCellObjects::emplace<test::FakeTownCellObjects>(cellObjects);
		test::FakeVillagerTentQueries tentQueries;
		tentQueries.nearestTree = [this](glm::ivec2, float) { return _tree; };
		tentQueries.collide = [](glm::ivec2) { return 0u; };
		Locator::villagerTentQueries::emplace<test::FakeVillagerTentQueries>(tentQueries);
		tv::ClearVagrants();
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

	static FakeStateTable& Table() { return static_cast<FakeStateTable&>(Locator::livingActionSystem::value()); }
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
		v.birthTurn = villager::BirthTurnForAge(25, 1000);
		auto& action = registry.Assign<LivingAction>(e, S(top), static_cast<uint16_t>(0));
		action.states.at(1) = static_cast<uint8_t>(final);
		registry.Assign<Transform>(e, glm::vec3(at.x, 0.0f, at.y), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<WallHug>(e, glm::vec2(), glm::vec2(), 0.0f, 0.1f);
		registry.Assign<Mobile>(e);
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

	static entt::entity MakeAbode(glm::vec2 at, float life = 1.0f, uint32_t townId = 1, uint32_t food = 0)
	{
		auto& registry = Reg();
		const auto abode = registry.Create();
		ecs::object_index::Assign(abode);
		registry.Assign<Abode>(abode, AbodeNumber::A, townId, food, 0u);
		registry.Assign<Transform>(abode, glm::vec3(at.x, 0.0f, at.y), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<Life>(abode, life);
		return abode;
	}

	static entt::entity MakeTree(glm::vec2 at)
	{
		auto& registry = Reg();
		const auto tree = registry.Create();
		registry.Assign<Tree>(tree);
		registry.Assign<Transform>(tree, glm::vec3(at.x, 0.0f, at.y), glm::mat3(1.0f), glm::vec3(1.0f));
		return tree;
	}

	/// Puts the villager in the abode as AddVillagerToAbode does and inside (flagged, PresentAtHome + 1) if asked
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
	static Abode& A(entt::entity e) { return Reg().Get<Abode>(e); }
	static uint32_t Top(entt::entity e) { return Action(e).states.at(0); }
	static uint32_t Final(entt::entity e) { return Action(e).states.at(1); }
	static glm::vec2 Goal(entt::entity e) { return Reg().Get<WallHug>(e).goal; }

	std::deque<uint32_t> _ints;
	std::deque<float> _floats;
	std::vector<std::string> _draws;
	entt::entity _tree {entt::null};
};
} // namespace

TEST_F(VillagerHomeTest, ArrivesHome)
{
	MakeTown();
	const auto abode = MakeAbode({60.0f, 60.0f});
	// not at the door: the walk again with FINAL 37
	auto a = MakeVillager({90.0f, 60.0f}, 37);
	Home(a, abode, false);
	EXPECT_EQ(villager::ArrivesHome(a), 1u);
	EXPECT_EQ(Top(a), 1u);
	EXPECT_EQ(Final(a), 37u);
	// at the door of a built and repaired abode: in (flags 4, PresentAtHome 1), 38
	auto b = MakeVillager({60.0f, 60.0f}, 37);
	Home(b, abode, false);
	EXPECT_EQ(villager::ArrivesHome(b), 1u);
	EXPECT_NE(V(b).flags & Villager::k_FlagAtHome, 0);
	EXPECT_EQ(A(abode).presentAtHome, 1);
	EXPECT_EQ(Top(b), 38u);
	// no abode: 129 and 0
	auto c = MakeVillager({60.0f, 60.0f}, 37);
	EXPECT_EQ(villager::ArrivesHome(c), 0u);
	EXPECT_EQ(Top(c), 129u);
}

TEST_F(VillagerHomeTest, ArrivesHomeHurtOrHungry)
{
	MakeTown();
	// a damaged abode (life 0.5: not repaired, not functional)
	const auto abode = MakeAbode({60.0f, 60.0f}, 0.5f);
	// hurt (life 0.2): a tent by the tree 30 m away, 2 m from it towards the villager, FINAL 238
	_tree = MakeTree({60.0f, 90.0f});
	auto a = MakeVillager({60.0f, 60.0f}, 37);
	Home(a, abode, false);
	V(a).life = 0.2f;
	EXPECT_EQ(villager::ArrivesHome(a), 1u);
	EXPECT_EQ(Top(a), 1u);
	EXPECT_EQ(Final(a), 238u);
	EXPECT_NEAR(Goal(a).x, 60.0f, 0.01f);
	EXPECT_NEAR(Goal(a).y, 88.0f, 0.01f);
	EXPECT_EQ(V(a).flags & Villager::k_FlagAtHome, 0);
	// hungry: SetTopState(163) and in the same turn ArriveHome and 38 (no jump between, literal)
	auto b = MakeVillager({60.0f, 60.0f}, 37);
	Home(b, abode, false);
	V(b).food = 0.4f;
	Table().calls.clear();
	EXPECT_EQ(villager::ArrivesHome(b), 1u);
	EXPECT_EQ(Table().Count("entry 163"), 1u);
	EXPECT_EQ(Top(b), 38u);
	EXPECT_NE(V(b).flags & Villager::k_FlagAtHome, 0);
}

TEST_F(VillagerHomeTest, ExitAtHome)
{
	MakeTown();
	const auto abode = MakeAbode({60.0f, 60.0f});
	auto a = MakeVillager({60.0f, 60.0f}, 38);
	Home(a, abode, true);
	V(a).flags = static_cast<uint16_t>(V(a).flags | Villager::k_FlagGoingToBed);
	EXPECT_EQ(A(abode).presentAtHome, 1);
	// 38 -> 119: stays (StaysAtHomeOnExit = 1)
	villager::SetTopState(a, S(119));
	EXPECT_NE(V(a).flags & Villager::k_FlagAtHome, 0);
	EXPECT_EQ(A(abode).presentAtHome, 1);
	// 119 -> 245: leaves (LeaveHome: bits 4 and 0x2000 off, PresentAtHome 0)
	villager::SetTopState(a, S(245));
	EXPECT_EQ(V(a).flags & (Villager::k_FlagAtHome | Villager::k_FlagGoingToBed), 0);
	EXPECT_EQ(A(abode).presentAtHome, 0);
	// 36 with the bit set -> 38
	auto b = MakeVillager({60.0f, 60.0f}, 36);
	Home(b, abode, true);
	villager::GoHome(b);
	EXPECT_EQ(Top(b), 38u);
}

TEST_F(VillagerHomeTest, HomeDecideWhatToDo)
{
	const auto town = MakeTown();
	const auto abode = MakeAbode({60.0f, 60.0f}, 1.0f, 1, 100);
	// the town's emergency -> 119
	auto a = MakeVillager({60.0f, 60.0f}, 38);
	Home(a, abode, true);
	Reg().Get<Town>(town).emergencyStartTurn = 950;
	EXPECT_EQ(villager::HomeDecideWhatToDo(a), 1u);
	EXPECT_EQ(Top(a), 119u);
	Reg().Get<Town>(town).emergencyStartTurn = 0;
	// CheckNeedsAtHome's trigger 0.9 x max(LifeDesire(0.7), POWER(0.5)) = 0.7875: life 0.6 -> sleep (119); 0.63 -> no
	auto b = MakeVillager({60.0f, 60.0f}, 38);
	Home(b, abode, true);
	V(b).life = 0.6f;
	EXPECT_EQ(villager::CheckNeedsAtHome(b), 1u);
	EXPECT_EQ(Top(b), 119u);
	auto c = MakeVillager({60.0f, 60.0f}, 38);
	V(c).life = 0.63f;
	EXPECT_EQ(villager::CheckNeedsAtHome(c), 0u);
	EXPECT_EQ(Top(c), 38u);
	// food 0.45 (POWER 0.908875 above it, and IsHungry): eat at home (118)
	auto d = MakeVillager({60.0f, 60.0f}, 38);
	Home(d, abode, true);
	V(d).food = 0.45f;
	EXPECT_EQ(villager::CheckNeedsAtHome(d), 1u);
	EXPECT_EQ(Top(d), 118u);
	// nothing: inside, GameRand(4) = 0 -> 119 with the counter 0 (and HomeDecideWhatToDo gives 0)
	auto e = MakeVillager({60.0f, 60.0f}, 38);
	Home(e, abode, true);
	Action(e).turnsUntilStateChange = 7;
	SetDraws({0}, {});
	EXPECT_EQ(villager::HomeDecideWhatToDo(e), 0u);
	EXPECT_EQ(Top(e), 119u);
	EXPECT_EQ(Action(e).turnsUntilStateChange, 0);
	EXPECT_EQ(_draws.front(), "R4");
	// GameRand(4) != 0 -> SetupNothingToDo (its GameRand(9))
	auto f = MakeVillager({60.0f, 60.0f}, 38);
	Home(f, abode, true);
	SetDraws({1, 3}, {});
	villager::HomeDecideWhatToDo(f);
	ASSERT_GE(_draws.size(), 2u);
	EXPECT_EQ(_draws.at(0), "R4");
	EXPECT_EQ(_draws.at(1), "R9");
}

TEST_F(VillagerHomeTest, NeedsAtHomePregnantOrChild)
{
	MakeTown();
	const auto abode = MakeAbode({60.0f, 60.0f});
	// a pregnant woman stays (1), her pregnancy counted down by the turns since the last check (0 here)
	Info().sex = SexType::Female;
	auto a = MakeVillager({60.0f, 60.0f}, 38);
	Home(a, abode, true);
	V(a).pregnancy = 20;
	EXPECT_EQ(villager::CheckNeedsAtHome(a), 1u);
	EXPECT_EQ(V(a).pregnancy, 20);
	EXPECT_EQ(Top(a), 38u);
	Info().sex = SexType::Male;
	// a child: CheckChildActivity (ChildDecideWhatToDo -> 114) and 1
	auto c = MakeVillager({60.0f, 60.0f}, 38);
	V(c).flags = Villager::k_FlagChild;
	EXPECT_EQ(villager::CheckNeedsAtHome(c), 1u);
	EXPECT_EQ(Top(c), 114u);
}

TEST_F(VillagerHomeTest, Sleeping)
{
	const auto town = MakeTown();
	const auto abode = MakeAbode({60.0f, 60.0f});
	// 119 -> 120 with the counter 100
	auto a = MakeVillager({60.0f, 60.0f}, 119);
	Home(a, abode, true);
	villager::GotoBedAtHome(Action(a));
	EXPECT_EQ(Top(a), 120u);
	EXPECT_EQ(Action(a).turnsUntilStateChange, 100);
	// by day (order 1 not Sleep first) from 0.4: 6 DoSleeping to 0.70000005, then awake
	V(a).life = 0.4f;
	int cycles = 0;
	while (villager::DoSleeping(a, 1.0f) != 0)
	{
		++cycles;
	}
	EXPECT_EQ(cycles + 1, 6);
	EXPECT_NEAR(V(a).life, 0.70000005f, 1e-7f);
	// the 100 calls of 120 then DoSleeping: awake -> 38
	V(a).life = 1.0f;
	Action(a).turnsUntilStateChange = 1;
	villager::SleepingAtHome(Action(a));
	EXPECT_EQ(Top(a), 38u);
	// Sleep (16) first in the town's order 1: it keeps sleeping with life 1
	Reg().Get<Town>(town).desire.sorted.at(0) = {0.0f, 1.0f, 16};
	auto b = MakeVillager({60.0f, 60.0f}, 120);
	Home(b, abode, true);
	EXPECT_EQ(villager::DoSleeping(b, 1.0f), 1u);
	EXPECT_EQ(Action(b).turnsUntilStateChange, 100);
	// poisoned: no sleep, no healing
	auto c = MakeVillager({60.0f, 60.0f}, 120);
	V(c).life = 0.4f;
	V(c).town = town;
	Reg().Assign<Poisoned>(c);
	EXPECT_EQ(villager::DoSleeping(c, 1.0f), 0u);
	EXPECT_FLOAT_EQ(V(c).life, 0.4f);
	// without a town the counter does not move
	auto d = MakeVillager({60.0f, 60.0f}, 120);
	Action(d).turnsUntilStateChange = 5;
	villager::SleepingAtHome(Action(d));
	EXPECT_EQ(Action(d).turnsUntilStateChange, 5);
}

TEST_F(VillagerHomeTest, CheckWhenGoingToBed)
{
	MakeTown();
	const auto abode = MakeAbode({60.0f, 60.0f});
	// 70 years old: the old age is tried once a stay (r = 0 -> n = 0 -> lives)
	auto a = MakeVillager({60.0f, 60.0f}, 38);
	Home(a, abode, true);
	V(a).birthTurn = villager::BirthTurnForAge(70, 1000);
	SetDraws({}, {});
	EXPECT_EQ(villager::CheckWhenGoingToBed(a), 1u);
	EXPECT_EQ(_draws, std::vector<std::string>({"F1.0", "R0"}));
	SetDraws({}, {});
	EXPECT_EQ(villager::CheckWhenGoingToBed(a), 1u);
	EXPECT_TRUE(_draws.empty());
	// after LeaveHome (the bit 0x2000 off) it is tried again
	villager::LeaveHome(a);
	SetDraws({}, {});
	villager::CheckWhenGoingToBed(a);
	EXPECT_EQ(_draws.size(), 2u);
	// 63 with r = 65534 / 65535 and d = 38: dies -> 0 (OLD_AGE)
	auto b = MakeVillager({60.0f, 60.0f}, 38);
	Home(b, abode, true);
	V(b).birthTurn = villager::BirthTurnForAge(63, 1000);
	SetDraws({38}, {65534.0f / 65535.0f});
	EXPECT_EQ(villager::CheckWhenGoingToBed(b), 0u);
	EXPECT_EQ(villager::GetDeathReason(b), DeathReason::OldAge);
}

TEST_F(VillagerHomeTest, TentNextToTree)
{
	_tree = MakeTree({100.0f, 160.0f});
	const auto a = MakeVillager({100.0f, 130.0f}, 36);
	// no one near the tree: 2 m from it towards the villager
	glm::ivec2 pos = tq::ToMapCoords({100.0f, 130.0f});
	ASSERT_TRUE(villager::GetTentPos(a, pos));
	EXPECT_NEAR(Metres(pos).x, 100.0f, 0.01f);
	EXPECT_NEAR(Metres(pos).y, 158.0f, 0.01f);
	// one villager in 238 between them (1 m from the tree): on the other side
	const auto sleeper = MakeVillager({100.0f, 159.0f}, 238);
	pos = tq::ToMapCoords({100.0f, 130.0f});
	ASSERT_TRUE(villager::GetTentPos(a, pos));
	EXPECT_NEAR(Metres(pos).y, 162.0f, 0.01f);
	// two: the tree is full; no other tree is tried: the spiral tries near pos, (-2, +1) cells = (-20 m, +10 m)
	MakeVillager({101.0f, 160.0f}, 238);
	pos = tq::ToMapCoords({100.0f, 130.0f});
	SetDraws({}, {});
	ASSERT_TRUE(villager::GetTentPos(a, pos));
	EXPECT_NEAR(Metres(pos).x, 80.0f, 0.01f);
	EXPECT_NEAR(Metres(pos).y, 140.0f, 0.01f);
	EXPECT_TRUE(_draws.empty());
	(void)sleeper;
}

TEST_F(VillagerHomeTest, TentSpiral)
{
	// no tree: a villager in 238 3 m from the point fails the first try (the GameFloatRand(5) then (2 pi))
	const auto a = MakeVillager({200.0f, 200.0f}, 36);
	MakeVillager({103.0f, 130.0f}, 238);
	glm::ivec2 pos = tq::ToMapCoords({100.0f, 130.0f});
	SetDraws({}, {2.0f, 0.0f}); // +5 m along x: the next try at (105, 130), 2 m from the sleeper: fails too
	villager::GetTentPos(a, pos);
	ASSERT_GE(_draws.size(), 2u);
	EXPECT_EQ(_draws.at(0), "F5.0");
	EXPECT_EQ(_draws.at(1), "F6.2");
}

TEST_F(VillagerHomeTest, DoGoingHomeWithoutAbode)
{
	MakeTown({50.0f, 50.0f});
	// 150 m from the town: a walk to 10..35 m from it on my side, FINAL the TOP (36)
	auto a = MakeVillager({200.0f, 50.0f}, 36);
	V(a).town = Reg().Context().towns[1];
	SetDraws({}, {glm::quarter_pi<float>(), 0.0f});
	villager::GoHome(a);
	EXPECT_EQ(_draws, std::vector<std::string>({"F1.5", "F25."}));
	EXPECT_EQ(Top(a), 1u);
	EXPECT_EQ(Final(a), 36u);
	EXPECT_NEAR(Goal(a).x, 60.0f, 0.05f);
	EXPECT_NEAR(Goal(a).y, 50.0f, 0.05f);
	// 50 m: a tent near me (no tree; the spiral), FINAL 238
	auto b = MakeVillager({100.0f, 50.0f}, 36);
	V(b).town = Reg().Context().towns[1];
	SetDraws({}, {});
	villager::GoHome(b);
	EXPECT_EQ(Final(b), 238u);
	// no town: 130
	auto c = MakeVillager({100.0f, 50.0f}, 36);
	villager::GoHome(c);
	EXPECT_EQ(Top(c), 130u);
}

TEST_F(VillagerHomeTest, ScoreAndFindAbode)
{
	const float near = gutils::GetDistanceModifier(0.0f, 500.0f);
	// an empty abode A (2 adults) at 0 m: (modifier + 1) x 0.5 x 1 x 1
	EXPECT_FLOAT_EQ(av::ScoreForAdding(0, 2, 0.0f, 0, 0.0f), (near + 1.0f) * 0.5f);
	// one adult of the same sex: room 0.5, sex ((1 - 1) + 1) x 0.5 = 0.5
	EXPECT_FLOAT_EQ(av::ScoreForAdding(1, 2, 1.0f, 1, 0.0f), (near + 1.0f) * 0.5f * 0.5f * 0.5f);
	// full: 0
	EXPECT_FLOAT_EQ(av::ScoreForAdding(2, 2, 1.0f, 2, 0.0f), 0.0f);
	// no places (MaxVillagers 0): 0
	EXPECT_FLOAT_EQ(av::ScoreForAdding(0, 0, 0.0f, 0, 0.0f), 0.0f);
	// FindAbodeWithSpaceInTown: the best strictly above the minimum; on a tie the newest (the head of the list)
	const auto town = MakeTown({60.0f, 60.0f});
	const auto older = MakeAbode({60.0f, 60.0f});
	const auto newer = MakeAbode({60.0f, 60.0f});
	auto v = MakeVillager({60.0f, 60.0f});
	EXPECT_EQ(tv::FindAbodeWithSpaceInTown(town, v, 0.0f), newer);
	EXPECT_TRUE(tv::FindAbodeWithSpaceInTown(town, v, 2.0f) == entt::null);
	// one man in the newer: the older (empty) wins
	auto m = MakeVillager({60.0f, 60.0f});
	av::AddVillagerToAbode(newer, m);
	EXPECT_EQ(tv::FindAbodeWithSpaceInTown(town, v, 0.0f), older);
}

TEST_F(VillagerHomeTest, AbodeList)
{
	const auto town = MakeTown();
	const auto abode = MakeAbode({60.0f, 60.0f});
	auto a = MakeVillager();
	auto b = MakeVillager();
	// a starts homeless: AddVillagerToAbode takes it out of the list
	V(a).town = town;
	tv::AddToHomelessList(town, a);
	av::AddVillagerToAbode(abode, a);
	av::AddVillagerToAbode(abode, b);
	EXPECT_TRUE(tv::Homeless(town).empty());
	// at the head: b, a; the counts; the first man only in MaleFemale
	EXPECT_EQ(A(abode).inhabitants, std::vector<entt::entity>({b, a}));
	EXPECT_EQ(A(abode).adultCount, 2);
	EXPECT_EQ(A(abode).adultMaleCount, 2);
	EXPECT_EQ(A(abode).maleFemale.at(0), a);
	EXPECT_EQ(V(a).abode, abode);
	EXPECT_EQ(V(a).town, town);
	// RemoveAlive of the one inside: SetTopState(163), whose exit (ExitAtHome) does the LeaveHome; the pair kept
	villager::ArriveHome(a);
	Action(a).states.at(0) = 38;
	EXPECT_EQ(A(abode).presentAtHome, 1);
	av::RemoveAliveVillagerFromAbode(abode, a);
	EXPECT_EQ(Top(a), 163u);
	EXPECT_EQ(A(abode).presentAtHome, 0);
	EXPECT_TRUE(V(a).abode == entt::null);
	EXPECT_EQ(A(abode).adultCount, 1);
	EXPECT_EQ(A(abode).maleFemale.at(0), a);
	// RemoveDeleted of the pair's man: both pairs to 0
	auto c = MakeVillager();
	av::AddVillagerToAbode(abode, c);
	A(abode).maleFemale.at(0) = c;
	A(abode).maleFemale.at(1) = b;
	av::RemoveDeletedVillagerFromAbode(abode, c);
	EXPECT_TRUE(A(abode).maleFemale.at(0) == entt::null);
	EXPECT_TRUE(A(abode).maleFemale.at(1) == entt::null);
}

TEST_F(VillagerHomeTest, CheckNeedNewAbode)
{
	const auto town = MakeTown();
	const auto abode = MakeAbode({60.0f, 60.0f});
	// a child: 0
	auto c = MakeVillager({60.0f, 60.0f});
	V(c).flags = Villager::k_FlagChild;
	EXPECT_EQ(villager::CheckNeedNewAbode(c), 0u);
	// an adult alone in an A (1 / 2 >= 0.5: too crowded), no better abode: homeless (129, no abode, in the list)
	auto a = MakeVillager({60.0f, 60.0f});
	av::AddVillagerToAbode(abode, a);
	EXPECT_TRUE(av::IsTooCrowded(abode));
	EXPECT_EQ(villager::CheckNeedNewAbode(a), 1u);
	EXPECT_EQ(Top(a), 129u);
	EXPECT_TRUE(V(a).abode == entt::null);
	EXPECT_EQ(V(a).town, town);
	EXPECT_EQ(tv::Homeless(town), std::vector<entt::entity>({a}));
	// 129 next: HomelessStart finds the empty abode -> 36
	EXPECT_EQ(villager::CheckHomelessMoveIntoAbode(a), 1u);
	EXPECT_EQ(Top(a), 36u);
	EXPECT_EQ(V(a).abode, abode);
	EXPECT_TRUE(tv::Homeless(town).empty());
}

TEST_F(VillagerHomeTest, VagrantStroll)
{
	// no town anywhere: CheckNeedNewAbode -> VagrantStart: a stroll ahead (FINAL 130)
	auto a = MakeVillager({100.0f, 130.0f});
	EXPECT_EQ(villager::CheckNeedNewAbode(a), 1u);
	EXPECT_EQ(Top(a), 1u);
	EXPECT_EQ(Final(a), 130u);
	// hurt: a tent (FINAL 238)
	auto b = MakeVillager({200.0f, 130.0f}, 130);
	V(b).life = 0.2f;
	villager::VagrantStart(b);
	EXPECT_EQ(Final(b), 238u);
}

TEST_F(VillagerHomeTest, SleepInTent)
{
	auto a = MakeVillager({100.0f, 130.0f}, 238);
	// the counter first
	Action(a).turnsUntilStateChange = 3;
	villager::SleepInTent(a);
	EXPECT_EQ(Action(a).turnsUntilStateChange, 2);
	// hurt: DoSleeping keeps it (counter 100)
	Action(a).turnsUntilStateChange = 0;
	V(a).life = 0.5f;
	villager::SleepInTent(a);
	EXPECT_EQ(Action(a).turnsUntilStateChange, 100);
	// awake, nothing to do: HomeDecideWhatToDo gives 0 -> 99 more turns (SetupNothingToDo: r9 0 -> r100 0 -> 36)
	Action(a).turnsUntilStateChange = 0;
	V(a).life = 1.0f;
	SetDraws({0, 0}, {});
	villager::SleepInTent(a);
	EXPECT_EQ(Action(a).turnsUntilStateChange, 99);
	EXPECT_EQ(Top(a), 36u);
}

TEST_F(VillagerHomeTest, GoHomeAndChange)
{
	// a config of its own for this test; the one before it (if any) comes back at the end
	const auto previousConfig = Locator::config::handle();
	Locator::config::emplace();
	MakeTown();
	const auto abode = MakeAbode({60.0f, 60.0f});
	// away from the door: a direct walk (FINAL 234)
	auto a = MakeVillager({90.0f, 60.0f}, 234);
	Home(a, abode, false);
	villager::GoHomeAndChange(Action(a));
	EXPECT_EQ(Top(a), 1u);
	EXPECT_EQ(Final(a), 234u);
	// at the door, outside: 37; a scale below 0.95 is reset for the age (two GameFloatRand(0.1) for an adult)
	auto b = MakeVillager({60.0f, 60.0f}, 234);
	Home(b, abode, false);
	Reg().Get<Transform>(b).scale = glm::vec3(0.9f);
	SetDraws({}, {0.02f, 0.03f});
	villager::GoHomeAndChange(Action(b));
	EXPECT_EQ(Top(b), 37u);
	EXPECT_FLOAT_EQ(Reg().Get<Transform>(b).scale.x, (0.05f - 0.03f) + 1.0f);
	// the exit to 37 (another exit): ChangeTribeIfRequired -> ChangeInfo; 37 stays at home: no puff
	Action(b).states.at(0) = 234;
	EXPECT_EQ(villager::ExitGoHomeAndChange(Action(b), S(37)), 1u);
	Locator::config::reset(previousConfig);
}

TEST_F(VillagerHomeTest, Shuffle)
{
	// two men in X, two women in Y: X is first (|-2000|), Y the best partner -> Y.SwapMaleForFemaleFrom(X): a man of X
	// goes to Y and a woman of Y to X
	const auto town = MakeTown();
	const auto x = MakeAbode({60.0f, 60.0f});
	const auto y = MakeAbode({70.0f, 60.0f});
	Info().sex = SexType::Male;
	auto m1 = MakeVillager();
	auto m2 = MakeVillager();
	av::AddVillagerToAbode(x, m1);
	av::AddVillagerToAbode(x, m2);
	// the women: a second info (Female) of the same number would need another record; set their sex through the
	// counts the desires read instead: the abode's adultMaleCount and the town's men / women
	auto& t = Reg().Get<Town>(town);
	t.stats.males = 2;
	t.stats.females = 2;
	t.stats.adults = 4;
	t.stats.adultPlaces = 4;
	A(y).adultCount = 2;
	A(y).adultMaleCount = 0;
	const float mx = av::CalculateDesireToGainMale(x);
	const float my = av::CalculateDesireToGainMale(y);
	EXPECT_FLOAT_EQ(mx, av::DesireToGainMale(2, 2, 2, 2));
	EXPECT_FLOAT_EQ(my, av::DesireToGainMale(2, 2, 2, 0));
	std::vector<tv::ShuffleEntry> list = {{y, my, 0.0f}, {x, mx, 0.0f}};
	tv::SortShuffle(list);
	EXPECT_EQ(list.at(0).abode, x);
	tv::ShufflePlan plan;
	ASSERT_TRUE(tv::PlanShuffle(list, 0, &av::GetPercentAbodeFullWithAdults, plan));
	EXPECT_TRUE(plan.swap);
	EXPECT_FALSE(plan.firstIsA); // X's -2000 is not above Y's: Y.SwapMaleForFemaleFrom(X)
	// the comparator never answers 0
	EXPECT_EQ(tv::ShuffleCompare(list.at(0), list.at(0)), 1);
}

TEST_F(VillagerHomeTest, AbodeProcess)
{
	MakeTown();
	// an empty built abode: +0.001 a processed turn; in float the clock reaches 1 at the 1001st (1.0009907) ->
	// ReduceLife(0.0001)
	const auto empty = MakeAbode({60.0f, 60.0f});
	for (int i = 0; i < 1000; ++i)
	{
		av::ProcessAbode(empty);
	}
	EXPECT_FLOAT_EQ(Reg().Get<Life>(empty).value, 1.0f);
	av::ProcessAbode(empty);
	EXPECT_FLOAT_EQ(Reg().Get<Life>(empty).value, 1.0f - 0.0001f);
	EXPECT_FLOAT_EQ(A(empty).emptyTimer, 0.0f);
	EXPECT_EQ(A(empty).field0xB9, 200);
	// with a child (1 / 1 = 1 with the integer division): not empty, no decay
	const auto withChild = MakeAbode({70.0f, 60.0f});
	A(withChild).childCount = 1;
	for (int i = 0; i < 1001; ++i)
	{
		av::ProcessAbode(withChild);
	}
	EXPECT_FLOAT_EQ(Reg().Get<Life>(withChild).value, 1.0f);
	EXPECT_FLOAT_EQ(av::GetPercentAbodeFullWithChildren(withChild), 1.0f);
}
