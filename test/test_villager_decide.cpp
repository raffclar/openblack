/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The villager's decision (163) and its idle states (docs/bw1-notes/villagers.md): the desires,
// CheckSatisfyOwnDesire, the civic check, SetupNothingToDo's branches, GetChillOutPos, the congregation point, the
// clear-area searches, 245, 246, 163, 36 and 209, with a fake state table in the Locator and scripted draws (the fixture
// of test_villager_core.cpp).

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>

#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/gtc/constants.hpp>
#include <gtest/gtest.h>

#include "Common/EventManager.h"
#include "Common/GameRandomTesting.h"
#include "Common/RandomNumberManager.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/ForcedNothingRoll.h"
#include "ECS/Components/Life.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Events/VillagerDecideEvents.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/VillagerChildFactory.h"
#include "ECS/Systems/Implementations/VillagerDiscipleJobs.h"
#include "ECS/Systems/Implementations/VillagerRules.h"
#include "ECS/Systems/Implementations/VillagerWorldQueries.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/TownQueries.h"
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
	struct Row
	{
		std::optional<uint32_t> entry;
		std::optional<uint32_t> exit;
	};
	mutable std::map<uint32_t, Row> rows;
	mutable std::vector<std::string> calls;

	void Update() override {}
	[[nodiscard]] VillagerStates VillagerGetState(const LivingAction& action, Index index) const override
	{
		return static_cast<VillagerStates>(action.states.at(static_cast<size_t>(index)));
	}
	void VillagerSetState(LivingAction&, Index, VillagerStates, bool) const override {}
	uint32_t VillagerCallState(LivingAction& action, Index index) const override
	{
		calls.push_back("state " + std::to_string(action.states.at(static_cast<size_t>(index))));
		return 0;
	}
	uint32_t VillagerCallEntry(LivingAction&, VillagerStates row, VillagerStates, VillagerStates) const override
	{
		calls.push_back("entry " + std::to_string(static_cast<uint32_t>(row)));
		return rows[static_cast<uint32_t>(row)].entry.value_or(1);
	}
	uint32_t VillagerCallExit(LivingAction&, VillagerStates row, VillagerStates next) const override
	{
		calls.push_back("exit " + std::to_string(static_cast<uint32_t>(row)) + " " +
		                std::to_string(static_cast<uint32_t>(next)));
		return rows[static_cast<uint32_t>(row)].exit.value_or(1);
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

glm::ivec2 M(float x, float z)
{
	return tq::ToMapCoords({x, z});
}

class VillagerDecideTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::EmplaceMapAndVillagerDefaults();
		auto info = std::make_unique<InfoConstants>();
		auto& v = info->villager.at(0);
		v.hungryForFood = 0.5f;
		v.processChecksEvery = 8;
		v.damageThresholdToGoHome = 0.3f;
		v.pauseForASecondChance = 0.01f;
		v.ownDesireThreshold = 0.3f;
		v.initialChillOutTime = 500;
		v.subsequentChillOutTime = 100;
		v.minWoodToShowGraphic = 50;
		v.minFoodToShowGraphic = 100;
		v.grownUpAge = 13;
		v.sex = SexType::Female;
		v.life = 1.0f;
		v.moveState = LivingStates::LivingMoveToPos;
		// no row pauses (no GameFloatRand from SetTopState): the draws are the decision's only
		for (auto& row : info->villagerStateTable)
		{
			row.animation = 385;
			row.field0x4 = -1;
			row.field0x8 = 1.0f;
			row.field0xf0 = 1;
		}
		auto& table = info->villagerStateTable;
		for (const uint32_t s : {36u, 37u, 85u, 114u, 163u, 209u, 242u, 245u, 246u, 252u})
		{
			table.at(s).isFinalState = 1;
		}
		table.at(1).field0x14 = 1; // MOVE_TO_POS moves
		info->town.maxDistanceFromCongreationPosThatPeopleChillOut = 50.0f;
		info->town.maxDistanceFromHouseThatPeopleChillOut = 10.0f;
		info->town.gameTurnsAfterEmergencyVillagersReact = 100;
		info->abode.at(0).thresholdForStopBeingFunctional = 0.75f;
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
		// the map cells: the entities with a Transform in the cell; the radius of each from _radius
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
		cellObjects.get2DRadius = [this](entt::entity e) {
			const auto it = _radius.find(e);
			return it != _radius.end() ? it->second : 0.0f;
		};
		Locator::townCellObjects::emplace<test::FakeTownCellObjects>(cellObjects);
		// A fresh event manager per test, so the handler never outlives the fixture it captures
		Locator::events::emplace<EventManager>().AddHandler<ecs::events::VillagerDecideStep>(
		    [this](const ecs::events::VillagerDecideStep& e) { _log.emplace_back(e.step); });
	}

	void TearDown() override
	{
		Locator::events::reset();
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
			    const auto v = _ints.front();
			    _ints.pop_front();
			    return v;
		    },
		    [this](float x) {
			    _draws.push_back("F" + std::to_string(x).substr(0, 3));
			    if (_floats.empty())
			    {
				    return 0.0f;
			    }
			    const auto v = _floats.front();
			    _floats.pop_front();
			    return v;
		    });
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
		auto& action = registry.Assign<LivingAction>(e, S(top), static_cast<uint16_t>(0));
		action.states.at(1) = static_cast<uint8_t>(final);
		registry.Assign<Transform>(e, glm::vec3(at.x, 0.0f, at.y), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<WallHug>(e, glm::vec2(), glm::vec2(), 0.0f, 0.1f);
		registry.Assign<Mobile>(e);
		return e;
	}

	static entt::entity MakeTown(glm::vec2 at = {50.0f, 50.0f},
	                             std::optional<glm::vec2> congregation = glm::vec2(100.0f, 100.0f))
	{
		auto& registry = Reg();
		const auto town = registry.Create();
		auto& t = registry.Assign<Town>(town, 1u);
		registry.Assign<Transform>(town, glm::vec3(at.x, 0.0f, at.y), glm::mat3(1.0f), glm::vec3(1.0f));
		if (congregation)
		{
			t.congregationPos = M(congregation->x, congregation->y);
		}
		return town;
	}

	static entt::entity MakeAbode(glm::vec2 at, float life = 1.0f, AbodeNumber type = AbodeNumber::A)
	{
		auto& registry = Reg();
		const auto abode = registry.Create();
		registry.Assign<Abode>(abode, type, 1u, 0u, 0u);
		registry.Assign<Transform>(abode, glm::vec3(at.x, 0.0f, at.y), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<Life>(abode, life);
		return abode;
	}

	static LivingAction& Action(entt::entity e) { return Reg().Get<LivingAction>(e); }
	static Villager& V(entt::entity e) { return Reg().Get<Villager>(e); }
	static uint32_t Top(entt::entity e) { return Action(e).states.at(0); }
	static uint32_t Final(entt::entity e) { return Action(e).states.at(1); }
	static glm::vec2 Goal(entt::entity e) { return Reg().Get<WallHug>(e).goal; }

	std::deque<uint32_t> _ints;
	std::deque<float> _floats;
	std::vector<std::string> _draws;
	std::vector<std::string> _log;
	std::map<entt::entity, float> _radius;
};
} // namespace

TEST_F(VillagerDecideTest, Desires)
{
	auto a = MakeVillager();
	EXPECT_FLOAT_EQ(villager::GetLifeDesireFromLife(a, 1.0f), 0.0f);
	EXPECT_FLOAT_EQ(villager::GetLifeDesireFromLife(a, 0.3f), 1.0f);
	EXPECT_FLOAT_EQ(villager::GetLifeDesireFromLife(a, 0.65f), 0.75f);
	// the trigger: food 0.5 (hungry) -> f 0.875, life 1 -> l 0
	V(a).food = 0.5f;
	EXPECT_FLOAT_EQ(villager::GetOwnDesiresTrigger(a), 0.875f);
	// after a tap on its abode: 0
	V(a).flags = Villager::k_FlagAfterTapOnAbode;
	EXPECT_FLOAT_EQ(villager::GetOwnDesiresTrigger(a), 0.0f);
	// a child with nothing: at least 0.11
	auto c = MakeVillager();
	V(c).flags = Villager::k_FlagChild;
	EXPECT_FLOAT_EQ(villager::GetOwnDesiresTrigger(c), 0.11f);
	// above 1: 1 (f 1, l 0.7: 1 + 0.35)
	auto d = MakeVillager();
	V(d).food = 0.0f;
	V(d).life = 0.3f;
	EXPECT_FLOAT_EQ(villager::GetOwnDesiresTrigger(d), 1.0f);
}

TEST_F(VillagerDecideTest, CheckSatisfyOwnDesire)
{
	const auto town = MakeTown();
	const auto abode = MakeAbode({60.0f, 60.0f});
	// food 0.95, life 1: nothing
	auto a = MakeVillager();
	V(a).food = 0.95f;
	EXPECT_EQ(villager::CheckSatisfyOwnDesire(a, 0.3f), 0u);
	EXPECT_EQ(Top(a), 163u);
	EXPECT_TRUE(Table().calls.empty());
	// food 0.5, life 1: food (ChangeStateToFindFoodToEat; the fixture's dinner of 0 needs nothing, so it
	// eats what it carries at once: 117 EAT_FOOD), not sleep
	auto b = MakeVillager();
	V(b).food = 0.5f;
	V(b).abode = abode;
	EXPECT_EQ(villager::CheckSatisfyOwnDesire(b, 0.3f), 1u);
	EXPECT_EQ(Top(b), 117u);
	// food 0.95, life 0.5, with an abode: sleep -> 36
	auto c = MakeVillager();
	V(c).food = 0.95f;
	V(c).life = 0.5f;
	V(c).abode = abode;
	V(c).town = town;
	EXPECT_EQ(villager::CheckSatisfyOwnDesire(c, 0.3f), 1u);
	EXPECT_EQ(Top(c), 36u);
	// food 0.5 (dF 0.575), life 0.5 (dL 0.618): the larger (sleep) first -> 36
	auto d = MakeVillager();
	V(d).food = 0.5f;
	V(d).life = 0.5f;
	V(d).abode = abode;
	EXPECT_EQ(villager::CheckSatisfyOwnDesire(d, 0.3f), 1u);
	EXPECT_EQ(Top(d), 36u);
	// after a tap on its abode, with life >= 0.3: CheckSatisfySleep 0
	auto e = MakeVillager();
	V(e).life = 0.5f;
	V(e).abode = abode;
	V(e).flags = Villager::k_FlagAfterTapOnAbode;
	EXPECT_EQ(villager::CheckSatisfySleep(e), 0u);
	EXPECT_EQ(Top(e), 163u);
}

TEST_F(VillagerDecideTest, CheckNeededForCivic)
{
	auto a = MakeVillager();
	V(a).flags = Villager::k_FlagAfterTapOnAbode;
	EXPECT_EQ(villager::CheckNeededForCivic(a), 0u);
	EXPECT_EQ(V(a).flags & Villager::k_FlagAfterTapOnAbode, Villager::k_FlagAfterTapOnAbode); // no town: kept
	V(a).town = MakeTown();
	EXPECT_EQ(villager::CheckNeededForCivic(a), 0u);
	EXPECT_EQ(V(a).flags & Villager::k_FlagAfterTapOnAbode, 0); // with a town: cleared
}

TEST_F(VillagerDecideTest, CheckNeededForTownDesireSleep)
{
	// CheckNeededForTownDesire -> TownDesire's CheckVillagerNeededForTownDesire with Sleep (16) first in the town's
	// order 1 (night) and an abode: CheckSatisfySleep -> 36 (36 takes it in)
	const auto town = MakeTown();
	const auto abode = MakeAbode({60.0f, 60.0f});
	auto& t = Reg().Get<Town>(town);
	t.desire.sorted.at(0) = {0.0f, 1.0f, 16};
	auto a = MakeVillager();
	V(a).town = town;
	V(a).abode = abode;
	EXPECT_EQ(villager::CheckNeededForTownDesire(a), 1u);
	EXPECT_EQ(Top(a), 36u);
	// by day (Sleep 0 first: not above the trigger 0.001): the share-out cuts and nothing changes
	t.desire.sorted.at(0) = {0.0f, 0.0f, 16};
	auto b = MakeVillager();
	V(b).town = town;
	V(b).abode = abode;
	EXPECT_EQ(villager::CheckNeededForTownDesire(b), 0u);
	EXPECT_EQ(Top(b), 163u);
}

TEST_F(VillagerDecideTest, SetupNothingToDoBranches)
{
	const auto town = MakeTown();
	const auto home = MakeAbode({60.0f, 60.0f});
	const auto ruin = MakeAbode({70.0f, 60.0f}, 0.5f); // life 0.5 <= 0.75: not functional
	const auto make = [&](entt::entity abode, entt::entity t) {
		auto v = MakeVillager();
		V(v).abode = abode;
		V(v).town = t;
		return v;
	};
	// r 0, functional abode -> 36, no GameRand(100)
	auto a = make(home, town);
	SetDraws({0}, {});
	EXPECT_EQ(villager::SetupNothingToDo(a), 1u);
	EXPECT_EQ(Top(a), 36u);
	EXPECT_EQ(_draws, std::vector<std::string>({"R9"}));
	// r 0, abode not functional: r100 9 -> 36, r100 10 -> 245
	auto b = make(ruin, town);
	SetDraws({0, 9}, {});
	villager::SetupNothingToDo(b);
	EXPECT_EQ(Top(b), 36u);
	EXPECT_EQ(_draws, std::vector<std::string>({"R9", "R100"}));
	auto c = make(ruin, town);
	SetDraws({0, 10}, {});
	villager::SetupNothingToDo(c);
	EXPECT_EQ(Top(c), 245u);
	// r 0, no abode, a town, r100 50 -> walk with FINAL 246
	auto d = make(entt::null, town);
	SetDraws({0, 50}, {});
	EXPECT_EQ(villager::SetupNothingToDo(d), 1u);
	EXPECT_EQ(Top(d), 1u);
	EXPECT_EQ(Final(d), 246u);
	// r 3 -> 245; r 3 without an abode -> 246; r 4 without a town -> 36; r 8 -> 246
	auto e = make(home, town);
	SetDraws({3}, {});
	villager::SetupNothingToDo(e);
	EXPECT_EQ(Top(e), 245u);
	auto f = make(entt::null, town);
	SetDraws({3}, {});
	villager::SetupNothingToDo(f);
	EXPECT_EQ(Top(f), 1u);
	EXPECT_EQ(Final(f), 246u);
	auto g = make(home, entt::null);
	SetDraws({4}, {});
	EXPECT_EQ(villager::SetupNothingToDo(g), 1u);
	EXPECT_EQ(Top(g), 36u);
	auto h = make(home, town);
	SetDraws({8}, {});
	villager::SetupNothingToDo(h);
	EXPECT_EQ(Top(h), 1u);
	EXPECT_EQ(Final(h), 246u);
	// the forced roll (OPENBLACK_TEST_VILLAGER_NOTHING): the draw is still made
	auto k = make(home, town);
	Reg().Assign<ForcedNothingRoll>(k, 3u);
	SetDraws({8}, {});
	villager::SetupNothingToDo(k);
	EXPECT_EQ(Top(k), 245u);
	EXPECT_EQ(_draws, std::vector<std::string>({"R9"}));
}

TEST_F(VillagerDecideTest, GetChillOutPos)
{
	// congregation (100, 100), me (100, 130), R = 0.1 x 50 = 5
	auto a = MakeVillager({100.0f, 130.0f});
	V(a).town = MakeTown();
	SetDraws({}, {0.0f, 0.0f});
	auto found = villager::GetChillOutPos(a);
	ASSERT_TRUE(found.has_value());
	auto out = *found;
	EXPECT_EQ(_draws, std::vector<std::string>({"F0.7", "F45."}));
	// my side (+z, 90 degrees) - 22.5 degrees, 5 m
	auto m = tq::ToMetres(out);
	const float a1 = glm::half_pi<float>() - 0.39269909f;
	EXPECT_NEAR(m.x, 100.0f + 5.0f * std::cos(a1), 0.01f);
	EXPECT_NEAR(m.y, 100.0f + 5.0f * std::sin(a1), 0.01f);
	// draws pi / 4 and 45: + 22.5 degrees, 50 m
	SetDraws({}, {glm::quarter_pi<float>(), 45.0f});
	found = villager::GetChillOutPos(a);
	ASSERT_TRUE(found.has_value());
	out = *found;
	m = tq::ToMetres(out);
	const float a2 = glm::half_pi<float>() + 0.39269909f;
	EXPECT_NEAR(m.x, 100.0f + 50.0f * std::cos(a2), 0.01f);
	EXPECT_NEAR(m.y, 100.0f + 50.0f * std::sin(a2), 0.01f);
	// no town: 0
	auto b = MakeVillager();
	EXPECT_FALSE(villager::GetChillOutPos(b).has_value());
}

TEST_F(VillagerDecideTest, CongregationPos)
{
	// no abode: the town + GetPosFromAngle(GameFloatRand(2 pi), GameFloatRand(10) + 10), the distance drawn first
	const auto t0 = MakeTown({50.0f, 50.0f}, std::nullopt);
	SetDraws({}, {5.0f, 0.0f});
	auto p = tq::ToMetres(tq::GetCongregationPos(t0));
	EXPECT_EQ(_draws, std::vector<std::string>({"F10.", "F6.2"}));
	EXPECT_NEAR(p.x, 65.0f, 0.01f);
	EXPECT_NEAR(p.y, 50.0f, 0.01f);
	// cached: no draws the second time
	SetDraws({}, {});
	p = tq::ToMetres(tq::GetCongregationPos(t0));
	EXPECT_TRUE(_draws.empty());
	EXPECT_NEAR(p.x, 65.0f, 0.01f);
	Reg().Destroy(t0);
	// one abode (and a field-type one that does not count): that abode + the offset
	const auto t1 = MakeTown({50.0f, 50.0f}, std::nullopt);
	MakeAbode({200.0f, 300.0f});
	MakeAbode({400.0f, 300.0f}, 1.0f, AbodeNumber::Field);
	SetDraws({}, {0.0f, glm::half_pi<float>()});
	p = tq::ToMetres(tq::GetCongregationPos(t1));
	EXPECT_NEAR(p.x, 200.0f, 0.01f);
	EXPECT_NEAR(p.y, 310.0f, 0.01f);
	Reg().Destroy(t1);
	std::vector<entt::entity> abodes;
	Reg().Each<const Abode>([&](entt::entity e, const Abode&) { abodes.push_back(e); });
	for (const auto e : abodes)
	{
		Reg().Destroy(e);
	}
	// three abodes on free ground: their average (truncated MapCoords), no draws
	const auto t3 = MakeTown({50.0f, 50.0f}, std::nullopt);
	MakeAbode({100.0f, 100.0f});
	MakeAbode({130.0f, 100.0f});
	MakeAbode({100.0f, 160.0f});
	SetDraws({}, {});
	_radius.clear();
	// the abodes block (Object), but none is in the 4 cells around the average: the first point is clear
	p = tq::ToMetres(tq::GetCongregationPos(t3));
	EXPECT_TRUE(_draws.empty());
	EXPECT_NEAR(p.x, 110.0f, 0.01f);
	EXPECT_NEAR(p.y, 120.0f, 0.01f);
}

TEST_F(VillagerDecideTest, ClearArea)
{
	EXPECT_EQ(tq::GetMapCellSpiralSizeFromRadius(1.2f), 1u);
	EXPECT_EQ(tq::GetMapCellSpiralSizeFromRadius(10.0f), 4u);
	EXPECT_EQ(tq::GetIncrementSpiralSizeFromRadius(130.0f, 3.0f), 7569u);
	EXPECT_EQ(tq::GetIncrementSpiralSizeFromRadius(5.0f, 1.0f), 121u);
	// a blocking object 9 m away (cell (-1, 0) of pos's), radius 0
	auto& registry = Reg();
	const auto block = registry.Create();
	registry.Assign<Transform>(block, glm::vec3(96.0f, 0.0f, 105.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	const auto pos = M(105.0f, 105.0f);
	EXPECT_FALSE(tq::CheckForClearArea(pos, 10.0f, &tq::BlocksTownClearArea, entt::null));
	EXPECT_TRUE(tq::CheckForClearArea(pos, 8.0f, &tq::BlocksTownClearArea, entt::null));
	// excluded: it does not count
	EXPECT_TRUE(tq::CheckForClearArea(pos, 10.0f, &tq::BlocksTownClearArea, block));
	// a Mobile does not block a town's clear area, but is an Object
	registry.Assign<Mobile>(block);
	EXPECT_TRUE(tq::CheckForClearArea(pos, 10.0f, &tq::BlocksTownClearArea, entt::null));
	EXPECT_FALSE(tq::CheckForClearArea(pos, 10.0f, &tq::IsObject, entt::null));
	// the cells are (0, 0) (-1, 0) (-1, -1) (0, -1): not the +x side
	const auto east = registry.Create();
	registry.Assign<Transform>(east, glm::vec3(112.0f, 0.0f, 105.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Remove<Mobile>(block);
	registry.Destroy(block);
	EXPECT_TRUE(tq::CheckForClearArea(pos, 10.0f, &tq::BlocksTownClearArea, entt::null));
	// FindClearArea: blocked at the start, the first clear point of the spiral; none -> no point
	_radius[east] = 10.0f;
	// (all of east's cell is within its radius: the first point of the spiral outside it, 3 m to -x)
	const auto result = tq::FindClearArea(M(112.0f, 105.0f), 30.0f, 3.0f, 1.0f, &tq::IsObject, entt::null);
	ASSERT_TRUE(result.has_value());
	EXPECT_NEAR(tq::ToMetres(*result).x, 109.0f, 0.01f);
	EXPECT_NEAR(tq::ToMetres(*result).y, 105.0f, 0.01f);
	EXPECT_FALSE(tq::FindClearArea(M(112.0f, 105.0f), 1.0f, 1.0f, 1.0f, &tq::IsObject, entt::null).has_value());
}

TEST_F(VillagerDecideTest, GoAndChilloutOutsideHome)
{
	const auto town = MakeTown();
	const auto abode = MakeAbode({60.0f, 60.0f}); // no mesh: the door is its position
	// far (30 m > R 10): a walk with FINAL 245 to GetPosOutside(3, 5, 5): GameFloatRand(2 pi / 3), GameFloatRand(5)
	auto a = MakeVillager({90.0f, 60.0f}, 245);
	V(a).town = town;
	V(a).abode = abode;
	SetDraws({}, {glm::two_pi<float>() / 6.0f, 2.5f});
	villager::GoAndChilloutOutsideHome(Action(a));
	EXPECT_EQ(_draws, std::vector<std::string>({"F2.0", "F5.0"}));
	EXPECT_EQ(Top(a), 1u);
	EXPECT_EQ(Final(a), 245u);
	EXPECT_NEAR(glm::length(Goal(a) - glm::vec2(60.0f, 60.0f)), 7.5f, 0.01f);
	// near and clear: 246, turned one step (at most 0x100) towards T
	auto b = MakeVillager({62.0f, 60.0f}, 245);
	V(b).town = town;
	V(b).abode = abode;
	Reg().Get<WallHug>(b).yAngle = glm::half_pi<float>(); // 0x200; T is at 0 (+x): one step of 0x100 down
	SetDraws({}, {});
	villager::GoAndChilloutOutsideHome(Action(b));
	EXPECT_EQ(Top(b), 246u);
	EXPECT_NEAR(Reg().Get<WallHug>(b).yAngle, 0x100 * 0.0030679617f, 1e-4f);
	// near but blocked (an object on it, radius 2): FindClearArea(5, 1) -> a walk with FINAL 245
	auto c = MakeVillager({60.0f, 63.0f}, 245);
	V(c).town = town;
	V(c).abode = abode;
	const auto stone = Reg().Create();
	Reg().Assign<Transform>(stone, glm::vec3(60.0f, 0.0f, 63.5f), glm::mat3(1.0f), glm::vec3(1.0f));
	_radius[stone] = 2.0f;
	villager::GoAndChilloutOutsideHome(Action(c));
	EXPECT_EQ(Top(c), 1u);
	EXPECT_EQ(Final(c), 245u);
	// no abode: 163
	auto d = MakeVillager({60.0f, 63.0f}, 245);
	V(d).town = town;
	villager::GoAndChilloutOutsideHome(Action(d));
	EXPECT_EQ(Top(d), 163u);
}

TEST_F(VillagerDecideTest, SitAndChillout)
{
	const auto town = MakeTown();
	const auto abode = MakeAbode({60.0f, 60.0f});
	auto a = MakeVillager({62.0f, 60.0f}, 246);
	V(a).town = town;
	V(a).abode = abode;
	EXPECT_EQ(villager::EnterSitAndChillOut(Action(a), S(0), S(246)), 1u);
	EXPECT_EQ(Action(a).turnsUntilStateChange, 500);
	// 500 calls without a check
	for (int i = 0; i < 500; ++i)
	{
		villager::SitAndChillout(Action(a));
	}
	EXPECT_TRUE(_log.empty());
	EXPECT_EQ(Top(a), 246u);
	// the 501st: CheckNeededForSomething, then GameRand(10) != 0 -> the counter 100
	SetDraws({4}, {});
	villager::SitAndChillout(Action(a));
	EXPECT_EQ(_log.front(), "worship");
	EXPECT_EQ(_draws, std::vector<std::string>({"R10"}));
	EXPECT_EQ(Action(a).turnsUntilStateChange, 100);
	EXPECT_EQ(Top(a), 246u);
	// GameRand(10) == 0 -> SetupNothingToDo straight away (no 163)
	Action(a).turnsUntilStateChange = 0;
	Table().calls.clear();
	SetDraws({0, 3}, {});
	villager::SitAndChillout(Action(a));
	EXPECT_EQ(_draws, std::vector<std::string>({"R10", "R9"}));
	EXPECT_EQ(Top(a), 245u);
	EXPECT_EQ(Table().Count("entry 163"), 0u);
	// the town's emergency -> 242
	auto b = MakeVillager({62.0f, 60.0f}, 246);
	V(b).town = town;
	Reg().Get<Town>(town).emergencyStartTurn = 950;
	villager::SitAndChillout(Action(b));
	EXPECT_EQ(Top(b), 242u);
}

TEST_F(VillagerDecideTest, DecideWhatToDo)
{
	const auto town = MakeTown();
	const auto abode = MakeAbode({60.0f, 60.0f});
	// the emergency: 242, without SetTopState(163)
	auto a = MakeVillager();
	V(a).town = town;
	Reg().Get<Town>(town).emergencyStartTurn = 950;
	villager::DecideWhatToDo(Action(a));
	EXPECT_EQ(Top(a), 242u);
	EXPECT_EQ(Table().Count("entry 163"), 0u);
	// the emergency is over after 100 turns (unsigned turn - start < 100)
	Reg().Get<Town>(town).emergencyStartTurn = 900;
	// a child: 114
	auto c = MakeVillager();
	V(c).town = town;
	V(c).flags = Villager::k_FlagChild;
	_log.clear();
	villager::DecideWhatToDo(Action(c));
	EXPECT_EQ(Top(c), 114u);
	EXPECT_EQ(_log, std::vector<std::string>({"child"}));
	// an adult the worship site wants: no SetupNothingToDo
	test::FakeVillagerWorshipCheck goes;
	goes.worshipCheck = [](entt::entity) { return true; };
	Locator::villagerWorshipCheck::emplace<test::FakeVillagerWorshipCheck>(goes);
	auto w = MakeVillager();
	V(w).town = town;
	V(w).abode = abode;
	_log.clear();
	SetDraws({}, {});
	villager::DecideWhatToDo(Action(w));
	EXPECT_EQ(_log, std::vector<std::string>({"something", "worship"}));
	EXPECT_TRUE(_draws.empty());
	test::FakeVillagerWorshipCheck stays;
	stays.worshipCheck = [](entt::entity) { return false; };
	Locator::villagerWorshipCheck::emplace<test::FakeVillagerWorshipCheck>(stays);
	// carrying 51 wood: 31
	auto r = MakeVillager();
	V(r).town = town;
	V(r).abode = abode;
	V(r).resourceHeld.at(1) = 51;
	villager::DecideWhatToDo(Action(r));
	EXPECT_EQ(Top(r), 31u);
	// nothing to do: the order of the checks, then SetupNothingToDo (r 3 -> 245)
	auto n = MakeVillager();
	V(n).town = town;
	V(n).abode = abode;
	_log.clear();
	Table().calls.clear();
	SetDraws({3}, {});
	EXPECT_EQ(villager::DecideWhatToDo(Action(n)), 1u);
	EXPECT_EQ(_log, std::vector<std::string>({"something", "worship", "civic", "own", "resources", "nothing"}));
	EXPECT_EQ(Table().Count("entry 163"), 1u);
	EXPECT_EQ(Top(n), 245u);
}

TEST_F(VillagerDecideTest, GoHome)
{
	const auto abode = MakeAbode({60.0f, 60.0f});
	// with an abode and FINAL != 37: a walk to the door (its position without a mesh) with FINAL 37
	auto a = MakeVillager({90.0f, 60.0f}, 36);
	V(a).abode = abode;
	EXPECT_EQ(villager::GoHomeState(Action(a)), 1u);
	EXPECT_EQ(Top(a), 1u);
	EXPECT_EQ(Final(a), 37u);
	EXPECT_NEAR(Goal(a).x, 60.0f, 0.01f);
	EXPECT_NEAR(Goal(a).y, 60.0f, 0.01f);
	// already going (GetFinalState 37): nothing
	Table().calls.clear();
	EXPECT_EQ(villager::GoHome(a), 1u);
	EXPECT_TRUE(Table().calls.empty());
	// no abode and no town: 130 VAGRANT_START (DoGoingHome)
	auto b = MakeVillager({90.0f, 60.0f}, 36);
	EXPECT_EQ(villager::GoHomeState(Action(b)), 1u);
	EXPECT_EQ(Top(b), 130u);
}

TEST_F(VillagerDecideTest, NothingToDo)
{
	auto a = MakeVillager({90.0f, 60.0f}, 209, 0);
	EXPECT_EQ(villager::NothingToDo(Action(a)), 1u);
	EXPECT_EQ(Top(a), 209u);
	EXPECT_TRUE(Table().calls.empty());
}

TEST_F(VillagerDecideTest, ChildFollowsMother)
{
	const auto town = MakeTown();
	const auto abode = MakeAbode({60.0f, 60.0f});
	// no mother: 5 m from the abode, at GameFloatRand(2 pi), FINAL 114
	auto c = MakeVillager({70.0f, 60.0f}, 114);
	V(c).town = town;
	V(c).abode = abode;
	V(c).flags = Villager::k_FlagChild;
	SetDraws({}, {glm::half_pi<float>()});
	EXPECT_EQ(villager::ChildFollowsMother(Action(c)), 1u);
	EXPECT_EQ(_draws, std::vector<std::string>({"F6.2"}));
	EXPECT_EQ(Top(c), 1u);
	EXPECT_EQ(Final(c), 114u);
	EXPECT_NEAR(Goal(c).x, 60.0f, 0.01f);
	EXPECT_NEAR(Goal(c).y, 65.0f, 0.01f);
	// a mother: around her
	const auto mother = MakeVillager({200.0f, 200.0f});
	auto d = MakeVillager({70.0f, 60.0f}, 114);
	V(d).town = town;
	V(d).abode = abode;
	V(d).mother = mother;
	V(d).flags = Villager::k_FlagChild;
	SetDraws({}, {0.0f});
	villager::ChildFollowsMother(Action(d));
	EXPECT_NEAR(Goal(d).x, 205.0f, 0.01f);
	EXPECT_NEAR(Goal(d).y, 200.0f, 0.01f);
	// a hungry child goes home (CheckChild -> GoHome: a walk with FINAL 37)
	auto e = MakeVillager({70.0f, 60.0f}, 114);
	V(e).town = town;
	V(e).abode = abode;
	V(e).flags = Villager::k_FlagChild;
	V(e).food = 0.4f;
	SetDraws({}, {});
	villager::ChildFollowsMother(Action(e));
	EXPECT_EQ(Final(e), 37u);
	EXPECT_TRUE(_draws.empty());
}
