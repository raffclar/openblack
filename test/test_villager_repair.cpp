/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The villager side of repairs and of the town emergency (docs/bw1-notes/villagers.md): the town emergency reaction
// column (k_TownEmergencyReaction) through GetFinalState, CallToTownEmergency, SetStateWhenTappedOnAbode / 197,
// 242 / 243 and CongregationDistance, CheckSatisfyToRepair and ArrivesHome's repair branch with a mock building side
// (a fake villagerBuildingSites service), and the repair cycles of the build formula (through the real
// life::IncreaseLife). The fixture of test_villager_home.cpp (a state table that records the calls, the real
// ExitAtHome for the at-home rows and the real ExitBuilding for 41, scripted draws).

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <gtest/gtest.h>

#include "Common/GameRandomTesting.h"
#include "Common/RandomNumberManager.h"
#include "ECS/Abodes.h"
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
#include "ECS/Systems/Implementations/VillagerChildFactory.h"
#include "ECS/Systems/Implementations/VillagerDiscipleJobs.h"
#include "ECS/Systems/Implementations/VillagerRules.h"
#include "ECS/Systems/Implementations/VillagerWorldQueries.h"
#include "ECS/Systems/Implementations/VillagerWorshipCheck.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Villager/VillagerBuild.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerEmergency.h"
#include "ECS/Villager/VillagerHome.h"
#include "ECS/Villager/VillagerOriginalFns.h"
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
namespace tq = openblack::ecs::town_queries;
namespace av = openblack::ecs::abode_villagers;
using openblack::map_coords::MapCoords;
using Index = LivingAction::Index;
using villager::TownEmergencyReaction;

namespace
{
class TestRng final: public RandomNumberManagerInterface
{
	std::mt19937 _generator {1};
	std::mt19937& Generator() override { return _generator; }
	std::optional<std::reference_wrapper<std::mutex>> LockAccess() override { return std::nullopt; }
};

/// A state table that records the calls; the exit of the at-home rows (35..38, 118..121) is the real ExitAtHome, the
/// exit of 41 BUILDING the real ExitBuilding (RemoveBuilder through the mock, the building site cleared)
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
		if (r == 41)
		{
			return villager::ExitBuilding(action, next);
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

/// One mock building site (only what SetupBuildingObject's path to GotoBuildingSite reads, and what the mock
/// GetBestRepairBuildingSite filters on)
struct MockSite
{
	entt::entity building {entt::null};
	bool needsBuilders {true};
	bool repairSite {false}; ///< Set only for a repair site (not for the ordinary site a damaged abode makes)
	float desire {0.0f};     ///< GetDesireToBeRepaired
};

class VillagerRepairTest: public ::testing::Test
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
			row.field0xf0 = 1; // no out-of clip; no row pauses (no draw from SetTopState)
		}
		auto& table = info->villagerStateTable;
		// final rows (isFinalState in info.dat) used here; 1 MOVE_TO_POS and 23 WAIT_FOR_ANIMATION are not final
		for (const uint32_t s : {14u, 36u, 37u, 38u, 40u, 41u, 163u, 197u, 220u, 242u, 243u})
		{
			table.at(s).isFinalState = 1;
		}
		table.at(1).field0x14 = 1;
		// StaysAtHomeOnExit (field0xc0): 35, 36, 37, 38, 118..121
		for (const uint32_t s : {35u, 36u, 37u, 38u, 118u, 119u, 120u, 121u})
		{
			table.at(s).staysAtHomeOnExit = 1;
		}
		info->town.gameTurnsAfterEmergencyVillagersReact = 1200;
		auto& abode = info->abode.at(0);
		abode.abodeNumber = AbodeNumber::A;
		abode.tribeType = Tribe::CELTIC;
		abode.maxVillagersInAbode = 4;
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
		SetDraws({}, {});
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

	/// Scripted draws: GameRand and GameFloatRand take the next value of their list (0 when it is empty) and log the
	/// call
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
	[[nodiscard]] size_t Draws(const std::string& draw) const
	{
		return static_cast<size_t>(std::count(_draws.begin(), _draws.end(), draw));
	}

	// ---- the mock building side ----

	test::FakeVillagerBuildingSites MockOps()
	{
		test::FakeVillagerBuildingSites ops;
		// the town's best repair site as building_sites::GetBestRepairBuildingSite picks it (its own tests cover the
		// real one): among the repair sites, the strictly largest desire above 0, first on ties
		ops.getBestRepairBuildingSite = [this](entt::entity) {
			_calls.emplace_back("best repair");
			entt::entity best = entt::null;
			float bestDesire = 0.0f;
			for (const auto& [entity, site] : _sites)
			{
				if (site.repairSite && site.desire > bestDesire)
				{
					best = entity;
					bestDesire = site.desire;
				}
			}
			return best;
		};
		ops.getBuildingSiteInList = [this](entt::entity, entt::entity building) {
			for (const auto& [entity, site] : _sites)
			{
				if (site.building == building)
				{
					return entity;
				}
			}
			return entt::entity {entt::null};
		};
		ops.addBuildingSite = [this](entt::entity, entt::entity building) {
			_calls.emplace_back("add site");
			return NewSite(building);
		};
		ops.getBuilding = [this](entt::entity site) {
			const auto it = _sites.find(site);
			return it != _sites.end() ? it->second.building : entt::entity {entt::null};
		};
		ops.isBuildingSiteValid = [this](entt::entity, entt::entity site) { return _sites.count(site) != 0; };
		ops.needsBuilders = [this](entt::entity site) { return _sites.at(site).needsBuilders; };
		ops.isBuilder = [](entt::entity, entt::entity) { return false; };
		ops.getBuilderCount = [](entt::entity) { return 0; };
		ops.removeBuilder = [this](entt::entity, entt::entity) { _calls.emplace_back("remove builder"); };
		ops.getClearAreaRadius = [](entt::entity) { return 5.0f; };
		ops.shouldIGetWood = [this](entt::entity, entt::entity, const std::function<MapCoords()>&) {
			_calls.emplace_back("should");
			return false;
		};
		ops.getRandomBuildPos = [](entt::entity, entt::entity, int32_t& index) {
			index = 5;
			const auto fixed = tq::ToMapCoords({62.0f, 60.0f});
			return MapCoords {fixed.x, fixed.y, 0.0f};
		};
		// the real IsBuilt / IsRepaired of the test's abodes
		ops.isBuilt = [](entt::entity building) { return ecs::abodes::IsBuilt(building); };
		ops.isRepaired = [](entt::entity building) { return ecs::abodes::IsRepaired(building); };
		return ops;
	}

	entt::entity NewSite(entt::entity building)
	{
		const auto site = Reg().Create();
		_sites[site] = MockSite {building, _needsBuilders};
		return site;
	}

	// ---- the entities ----

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

	/// Puts the villager in the abode as AddVillagerToAbode does, and inside (at home, PresentAtHome + 1) if asked
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
	static Town& T(entt::entity town) { return Reg().Get<Town>(town); }
	static uint32_t Top(entt::entity e) { return Action(e).states.at(0); }
	static uint32_t Final(entt::entity e) { return Action(e).states.at(1); }
	static uint32_t Previous(entt::entity e) { return Action(e).states.at(2); }
	static glm::vec2 Goal(entt::entity e) { return Reg().Get<WallHug>(e).goal; }

	std::deque<uint32_t> _ints;
	std::deque<float> _floats;
	std::vector<std::string> _draws;
	std::map<entt::entity, MockSite> _sites;
	std::vector<std::string> _calls;
	bool _needsBuilders {true};
};
} // namespace

// ---- the town emergency reaction column ----------------------------------------------------------------------------

TEST(VillagerRepairPure, TownEmergencyReactionTable)
{
	const auto& t = villager::k_TownEmergencyReaction;
	EXPECT_EQ(std::count(t.begin(), t.end(), TownEmergencyReaction::Always), 206);
	EXPECT_EQ(std::count(t.begin(), t.end(), TownEmergencyReaction::PreviousState), 1);
	EXPECT_EQ(std::count(t.begin(), t.end(), TownEmergencyReaction::None), 48);
	EXPECT_EQ(t.at(220), TownEmergencyReaction::PreviousState); // MOVE_AROUND_FIRE
	for (const size_t s : {0u, 4u, 5u, 10u, 11u, 15u, 24u, 38u, 100u, 120u, 168u, 198u, 213u, 219u, 242u, 243u, 246u})
	{
		EXPECT_EQ(t.at(s), TownEmergencyReaction::None) << s;
	}
	for (const size_t s : {1u, 23u, 31u, 37u, 39u, 40u, 41u, 129u, 163u, 184u, 197u, 209u, 221u, 232u, 248u, 254u})
	{
		EXPECT_EQ(t.at(s), TownEmergencyReaction::Always) << s;
	}
}

TEST(VillagerRepairPure, CongregationDistance)
{
	EXPECT_EQ(villager::CongregationDistance(0), 10.0f);
	EXPECT_EQ(villager::CongregationDistance(10), 15.0f);
	EXPECT_EQ(villager::CongregationDistance(20), 20.0f);
	EXPECT_EQ(villager::CongregationDistance(39), 29.5f);
	EXPECT_EQ(villager::CongregationDistance(40), 30.0f);
	EXPECT_EQ(villager::CongregationDistance(100), 30.0f);
}

/// The repair step x = u / WoodValue (BuildStep) added by the real life::IncreaseLife (capped at 1, float steps; what
/// BuildBy calls) on a Life component, from a damaged life to 1
TEST_F(VillagerRepairTest, RepairCycles)
{
	const auto cycles = [](uint32_t u, float value, float from) {
		const float x = villager::BuildStep(u, value);
		const auto building = Reg().Create();
		Reg().Assign<Life>(building, from);
		uint32_t n = 0;
		while (ecs::life::LifeOf(building) < 1.0f && n < 1000)
		{
			ecs::life::IncreaseLife(building, x);
			++n;
		}
		EXPECT_EQ(ecs::life::LifeOf(building), 1.0f);
		return n;
	};
	EXPECT_EQ(cycles(50, 1400.0f, 0.6f), 12u); // Norse Hut
	EXPECT_EQ(cycles(40, 1400.0f, 0.6f), 14u);
	EXPECT_EQ(cycles(50, 1400.0f, 0.9f), 3u);
	EXPECT_EQ(cycles(50, 4000.0f, 0.6f), 33u); // Storage Pit
	EXPECT_EQ(cycles(40, 4000.0f, 0.6f), 41u);
	EXPECT_EQ(cycles(50, 6500.0f, 0.1f), 117u); // Citadel Heart
	EXPECT_EQ(cycles(40, 6500.0f, 0.1f), 147u);
}

TEST_F(VillagerRepairTest, ReactsToTownEmergencyByFinalState)
{
	// TOP 1 (not final): FINAL decides. The walk home (FINAL 37 ARRIVES_HOME, the state ArrivesHome uses; no
	// villager walk has FINAL 38) -> yes; already inside (TOP 38 AT_HOME, final) -> no; a site -> yes
	const auto home = MakeVillager({100.0f, 130.0f}, 1, 37);
	EXPECT_TRUE(villager::ReactsToTownEmergency(home));
	const auto a = MakeVillager({100.0f, 130.0f}, 38);
	EXPECT_FALSE(villager::ReactsToTownEmergency(a));
	const auto b = MakeVillager({100.0f, 130.0f}, 1, 40);
	EXPECT_TRUE(villager::ReactsToTownEmergency(b));
	// 220 MOVE_AROUND_FIRE: its PREVIOUS state
	const auto c = MakeVillager({100.0f, 130.0f}, 220);
	EXPECT_FALSE(villager::ReactsToTownEmergency(c));
	Action(c).states.at(2) = 163;
	EXPECT_TRUE(villager::ReactsToTownEmergency(c));
	// 242 itself: no
	const auto d = MakeVillager({100.0f, 130.0f}, 242);
	EXPECT_FALSE(villager::ReactsToTownEmergency(d));
}

TEST_F(VillagerRepairTest, CallToTownEmergency)
{
	const auto town = MakeTown();
	// a builder (41) of a valid site: SetTopState(242), its exit told 242 = ExitBuilding: RemoveBuilder, and
	// buildingSite cleared; no draw
	const auto builder = MakeVillager({100.0f, 130.0f}, 41);
	V(builder).town = town;
	V(builder).buildingSite = NewSite(MakeAbode({100.0f, 120.0f}, 0.6f));
	villager::CallToTownEmergency(builder);
	EXPECT_EQ(Top(builder), 242u);
	EXPECT_EQ(Table().Count("exit 41 242"), 1u);
	EXPECT_EQ(std::count(_calls.begin(), _calls.end(), "remove builder"), 1);
	EXPECT_TRUE(V(builder).buildingSite == entt::null);
	EXPECT_TRUE(_draws.empty());
	// at home (38): unchanged
	const auto abode = MakeAbode({60.0f, 60.0f});
	const auto home = MakeVillager({60.0f, 60.0f}, 38);
	Home(home, abode, true);
	Table().calls.clear();
	villager::CallToTownEmergency(home);
	EXPECT_EQ(Top(home), 38u);
	EXPECT_TRUE(Table().calls.empty());
	EXPECT_TRUE(_draws.empty());
}

// ---- the tap on a home -------------------------------------------------------------------------------------------

TEST_F(VillagerRepairTest, SetStateWhenTappedOnAbode)
{
	MakeTown();
	const auto abode = MakeAbode({60.0f, 60.0f}, 0.6f);
	// in the house list but not inside: nothing, no draw
	const auto out = MakeVillager({70.0f, 60.0f}, 163);
	Home(out, abode, false);
	villager::SetStateWhenTappedOnAbode(out);
	EXPECT_EQ(Top(out), 163u);
	EXPECT_TRUE(_draws.empty());
	EXPECT_EQ(V(out).flags & Villager::k_FlagAfterTapOnAbode, 0);
	// inside (38): PREVIOUS 163, the walk FINAL 197 to the door + 1.5..3 m, flag 1; FindPosOutsideAbode's two draws
	// first (GameFloatRand(1.5), then GameFloatRand(pi/4)); out of the home (ExitAtHome)
	const auto in = MakeVillager({60.0f, 60.0f}, 38);
	Home(in, abode, true);
	SetDraws({}, {0.5f, glm::quarter_pi<float>() / 2.0f});
	villager::SetStateWhenTappedOnAbode(in);
	ASSERT_GE(_draws.size(), 2u);
	EXPECT_EQ(_draws.at(0), "F1.5");
	EXPECT_EQ(_draws.at(1), "F0.7");
	EXPECT_EQ(Top(in), 1u);
	EXPECT_EQ(Final(in), 197u);
	EXPECT_EQ(Previous(in), 163u);
	EXPECT_NE(V(in).flags & Villager::k_FlagAfterTapOnAbode, 0);
	EXPECT_EQ(V(in).flags & Villager::k_FlagAtHome, 0);
	// d = 0.5 + 1.5 = 2 m from the door (the abode's position without a mesh), angle = the door's angle + 0
	const glm::vec2 goal = Goal(in);
	EXPECT_NEAR(glm::length(goal - glm::vec2(60.0f, 60.0f)), 2.0f, 0.01f);
	// not available (DYING): nothing
	const auto dying = MakeVillager({60.0f, 60.0f}, 14);
	Home(dying, abode, true);
	SetDraws({}, {});
	villager::SetStateWhenTappedOnAbode(dying);
	EXPECT_EQ(Top(dying), 14u);
	EXPECT_TRUE(_draws.empty());
}

TEST_F(VillagerRepairTest, AfterTapOnAbode)
{
	const auto e = MakeVillager({60.0f, 60.0f}, 197);
	Action(e).states.at(2) = 163;
	EXPECT_EQ(villager::AfterTapOnAbode(Action(e)), 1u);
	// PlayAnimThenSetState(PREVIOUS): TOP 23 WAIT_FOR_ANIMATION, FINAL 163
	EXPECT_EQ(Top(e), 23u);
	EXPECT_EQ(Final(e), 163u);
}

// ---- 242 / 243 -----------------------------------------------------------------------------------------------------

TEST_F(VillagerRepairTest, GotoCongregateInTownAfterEmergency)
{
	// no town: 0, no move, no draw
	const auto lost = MakeVillager({100.0f, 130.0f}, 242);
	EXPECT_EQ(villager::GotoCongregateInTownAfterEmergency(Action(lost)), 0u);
	EXPECT_EQ(Top(lost), 242u);
	EXPECT_TRUE(_draws.empty());
	// a town of 20: d = 20 m; one GameFloatRand(2 pi) first; the walk FINAL 243 to congregation + GetPosFromAngle(a, d)
	const auto town = MakeTown();
	T(town).stats.adults = 15;
	T(town).stats.children = 5;
	const auto e = MakeVillager({100.0f, 130.0f}, 242);
	V(e).town = town;
	// the town's congregation position cached, so GetCongregationPos draws nothing of its own
	const auto congregation = tq::ToMapCoords({55.0f, 45.0f});
	T(town).congregationPos = congregation;
	SetDraws({}, {1.0f});
	EXPECT_EQ(villager::GotoCongregateInTownAfterEmergency(Action(e)), 1u);
	ASSERT_FALSE(_draws.empty());
	EXPECT_EQ(_draws.front(), "F6.2");
	EXPECT_EQ(Draws("F6.2"), 1u);
	EXPECT_EQ(Top(e), 1u);
	EXPECT_EQ(Final(e), 243u);
	const auto expected = tq::ToMetres(congregation + tq::GetPosFromAngle(1.0f, 20.0f));
	EXPECT_NEAR(Goal(e).x, expected.x, 0.01f);
	EXPECT_NEAR(Goal(e).y, expected.y, 0.01f);
}

TEST_F(VillagerRepairTest, CongregateInTownAfterEmergency)
{
	const auto town = MakeTown();
	T(town).emergencyStartTurn = 900; // 100 turns ago, 1200 last
	const auto e = MakeVillager({100.0f, 130.0f}, 243);
	V(e).town = town;
	// in the emergency: GameRand(12) 0 -> 36, 5 -> 242
	SetDraws({0}, {});
	EXPECT_EQ(villager::CongregateInTownAfterEmergency(Action(e)), 1u);
	EXPECT_EQ(_draws.front(), "R12");
	EXPECT_EQ(Top(e), 23u);
	EXPECT_EQ(Final(e), 36u);
	Action(e).states = {243, 0, 0};
	SetDraws({5}, {});
	villager::CongregateInTownAfterEmergency(Action(e));
	EXPECT_EQ(Final(e), 242u);
	// after it: GameRand(5) 0 -> 163, 3 -> 242
	T(town).emergencyStartTurn = 0;
	Action(e).states = {243, 0, 0};
	SetDraws({0}, {});
	villager::CongregateInTownAfterEmergency(Action(e));
	EXPECT_EQ(_draws.front(), "R5");
	EXPECT_EQ(Final(e), 163u);
	Action(e).states = {243, 0, 0};
	SetDraws({3}, {});
	villager::CongregateInTownAfterEmergency(Action(e));
	EXPECT_EQ(Final(e), 242u);
	// no town: the branch after the emergency
	const auto lost = MakeVillager({100.0f, 130.0f}, 243);
	SetDraws({0}, {});
	villager::CongregateInTownAfterEmergency(Action(lost));
	EXPECT_EQ(_draws.front(), "R5");
	EXPECT_EQ(Final(lost), 163u);
}

// ---- who repairs ---------------------------------------------------------------------------------------------------

TEST_F(VillagerRepairTest, CheckSatisfyToRepair)
{
	const auto town = MakeTown();
	const auto e = MakeVillager({60.0f, 60.0f});
	V(e).town = town;
	// no site: 0
	EXPECT_EQ(villager::CheckSatisfyToRepair(e), 0u);
	EXPECT_EQ(std::count(_calls.begin(), _calls.end(), "best repair"), 1);
	// the ordinary site (not a repair site) of a rock-damaged house at 0.6, even with a desire: not a repair site -> 0
	// (the rock case: its builders come through To_Build)
	const auto rocked = MakeAbode({80.0f, 60.0f}, 0.6f);
	const auto ordinary = NewSite(rocked);
	_sites.at(ordinary).desire = 5.0f;
	EXPECT_EQ(villager::CheckSatisfyToRepair(e), 0u);
	EXPECT_TRUE(V(e).buildingSite == entt::null);
	// a repair site with desire 0: not taken -> 0
	const auto house = MakeAbode({60.0f, 60.0f}, 0.6f);
	const auto site = NewSite(house);
	_sites.at(site).repairSite = true;
	EXPECT_EQ(villager::CheckSatisfyToRepair(e), 0u);
	// with a desire > 0: SetupBuildingObject(site) -> the walk to the ring, FINAL 40
	_sites.at(site).desire = 2.0f;
	EXPECT_EQ(villager::CheckSatisfyToRepair(e), 1u);
	EXPECT_TRUE(V(e).buildingSite == site);
	EXPECT_EQ(Final(e), 40u);
	// a repair site whose building is at full life (built and repaired, as a rebuild plan's new building at life 1):
	// SetupBuildingObject 0 -> 0
	Reg().Get<Life>(house).value = 1.0f;
	Action(e).states = {163, 0, 0};
	V(e).buildingSite = entt::null;
	EXPECT_EQ(villager::CheckSatisfyToRepair(e), 0u);
}

TEST_F(VillagerRepairTest, ArrivesHomeRepairBranch)
{
	MakeTown();
	// a damaged home (0.6: not repaired, not functional), the villager at the door, fed and well: it repairs it (the
	// site is made, SetupBuildingObject -> the ring, FINAL 40), it does not go in
	const auto abode = MakeAbode({60.0f, 60.0f}, 0.6f);
	const auto a = MakeVillager({60.0f, 60.0f}, 37);
	Home(a, abode, false);
	EXPECT_EQ(villager::ArrivesHome(a), 1u);
	EXPECT_EQ(std::count(_calls.begin(), _calls.end(), "add site"), 1);
	EXPECT_EQ(Final(a), 40u);
	EXPECT_EQ(V(a).flags & Villager::k_FlagAtHome, 0);
	// the site needs no builders (home life in (0.9, 1)): in
	_needsBuilders = false;
	const auto lightly = MakeAbode({80.0f, 60.0f}, 0.95f);
	const auto b = MakeVillager({80.0f, 60.0f}, 37);
	Home(b, lightly, false);
	EXPECT_EQ(villager::ArrivesHome(b), 1u);
	EXPECT_EQ(Top(b), 38u);
	EXPECT_NE(V(b).flags & Villager::k_FlagAtHome, 0);
	// hungry (food 0.4 < 0.5): in, no repair
	_needsBuilders = true;
	const auto c = MakeVillager({60.0f, 60.0f}, 37);
	Home(c, abode, false);
	V(c).food = 0.4f;
	EXPECT_EQ(villager::ArrivesHome(c), 1u);
	EXPECT_EQ(Top(c), 38u);
	// hurt (life 0.2) at a functional damaged home (0.8): in, no repair
	const auto functional = MakeAbode({100.0f, 60.0f}, 0.8f);
	const auto d = MakeVillager({100.0f, 60.0f}, 37);
	Home(d, functional, false);
	V(d).life = 0.2f;
	EXPECT_EQ(villager::ArrivesHome(d), 1u);
	EXPECT_EQ(Top(d), 38u);
	// built and at full life: in, no site asked for
	const auto full = MakeAbode({120.0f, 60.0f}, 1.0f);
	const auto f = MakeVillager({120.0f, 60.0f}, 37);
	Home(f, full, false);
	const auto sites = std::count(_calls.begin(), _calls.end(), "add site");
	EXPECT_EQ(villager::ArrivesHome(f), 1u);
	EXPECT_EQ(Top(f), 38u);
	EXPECT_EQ(std::count(_calls.begin(), _calls.end(), "add site"), sites);
}
