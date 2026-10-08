/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The villager core (docs/bw1-notes/villagers.md): the constructor's draws and water rule, CREATED, SetTopState's
// return codes and pause, SetState and the town's modifiers, the periodic checks and the life's wear, with a fake state
// table in the Locator and scripted draws.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdlib>

#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Common/GameRandomTesting.h"
#include "Common/RandomNumberManager.h"
#include "ECS/AnimalAI.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/CarriedByParticleSystem.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Poisoned.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/LivingPhysics.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHeld.h"
#include "ECS/Systems/Implementations/VillagerChildFactory.h"
#include "ECS/Systems/Implementations/VillagerDiscipleJobs.h"
#include "ECS/Systems/Implementations/VillagerRules.h"
#include "ECS/Systems/Implementations/VillagerWorldQueries.h"
#include "ECS/Systems/Implementations/VillagerWorshipCheck.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerScript.h"
#include "ECS/Villager/VillagerTrace.h"
#include "ECS/VillagerAnimations.h"
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
	struct Row
	{
		std::optional<uint32_t> entry;
		std::optional<uint32_t> exit;
		std::function<uint32_t(LivingAction&)> state;
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
		const auto s = action.states.at(static_cast<size_t>(index));
		calls.push_back("state " + std::to_string(s));
		const auto& row = rows[s];
		return row.state ? row.state(action) : 0;
	}
	uint32_t VillagerCallEntry(LivingAction&, VillagerStates row, VillagerStates final, VillagerStates next) const override
	{
		calls.push_back("entry " + std::to_string(static_cast<uint32_t>(row)) + " " +
		                std::to_string(static_cast<uint32_t>(final)) + " " + std::to_string(static_cast<uint32_t>(next)));
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

class VillagerCoreTest: public ::testing::Test
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
		v.grownUpAge = 13;
		v.sex = SexType::Female;
		v.life = 1.0f;
		v.moveState = LivingStates::LivingMoveToPos; // 1 in every villager row of info.dat
		// the rows of info.dat the tests use; every row: no out-of clip
		for (auto& row : info->villagerStateTable)
		{
			row.animation = 385;
			row.field0x4 = -1;
			row.field0x8 = 1.0f;
			row.field0xf0 = 1;
		}
		auto& table = info->villagerStateTable;
		// 0 INVALID: checks 1, goes home when hurt 1
		table.at(0).field0xe4 = 1;
		table.at(0).field0xe8 = 1;
		// 1 MOVE_TO_POS: moving, pause 1, drain 2e-6
		table.at(1).field0x14 = 1;
		table.at(1).canPauseForASecond = 1;
		table.at(1).field0xf8 = 2e-6f;
		// 10 FLYING / 24 IN_HAND: final, not kept as PREVIOUS, no checks
		for (const uint32_t s : {10u, 24u})
		{
			table.at(s).isFinalState = 1;
			table.at(s).keepsPreviousState = 1;
		}
		// 16 DROWNING: final, not PREVIOUS
		table.at(16).isFinalState = 1;
		table.at(16).keepsPreviousState = 1;
		// 19 GOTO_FOOD_REACTION: final, desire 0, checks 1, goes home 1
		table.at(19).isFinalState = 1;
		table.at(19).field0x4 = 0;
		table.at(19).field0xe4 = 1;
		table.at(19).field0xe8 = 1;
		// 36 GO_HOME: final, desire 8 (1), pause 1, checks 1
		table.at(36).isFinalState = 1;
		table.at(36).field0x4 = 8;
		table.at(36).canPauseForASecond = 1;
		table.at(36).field0xe4 = 1;
		// 85 CREATED / 163 DECIDE: final, no checks
		table.at(85).isFinalState = 1;
		table.at(163).isFinalState = 1;
		// 248 GO_HOME_FROM_WORSHIP: final, checks 1
		table.at(248).isFinalState = 1;
		table.at(248).field0xe4 = 1;
		// 219 ON_FIRE: final
		table.at(219).isFinalState = 1;
		// 239 PAUSE: not final, no pause, no checks, speed group 4
		table.at(239).speedIndex = 4;
		Locator::infoConstants::reset(info.release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		Locator::rng::emplace<TestRng>();
		Locator::livingActionSystem::emplace<FakeStateTable>();
		game_clock::SetTurn(100);
		// The villager services the code under test reaches: the game's, unless faked here
		Locator::villagerRules::emplace<ecs::systems::VillagerRules>();
		Locator::villagerWorldQueries::emplace<ecs::systems::VillagerWorldQueries>();
		Locator::villagerWorshipCheck::emplace<ecs::systems::VillagerWorshipCheck>();
		Locator::villagerChildFactory::emplace<ecs::systems::VillagerChildFactory>();
		Locator::villagerDiscipleJobs::emplace<ecs::systems::VillagerDiscipleJobs>();
		SetDraws({}, {});
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

	static entt::entity MakeVillager(uint32_t top = 163, uint32_t final = 0)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto e = registry.Create();
		auto& v = registry.Assign<Villager>(e);
		v.life = 1.0f;
		v.town = entt::null;
		v.abode = entt::null;
		v.food = 1.0f;
		auto& action = registry.Assign<LivingAction>(e, S(top), static_cast<uint16_t>(0));
		action.states.at(1) = static_cast<uint8_t>(final);
		return e;
	}

	static entt::entity MakeTown()
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto town = registry.Create();
		registry.Assign<Town>(town, 1u);
		return town;
	}

	static LivingAction& Action(entt::entity e) { return Locator::entitiesRegistry::value().Get<LivingAction>(e); }
	static Villager& V(entt::entity e) { return Locator::entitiesRegistry::value().Get<Villager>(e); }
	static uint32_t Top(entt::entity e) { return Action(e).states.at(0); }
	static uint32_t Final(entt::entity e) { return Action(e).states.at(1); }

	std::deque<uint32_t> _ints;
	std::deque<float> _floats;
	std::vector<std::string> _draws;
};
} // namespace

TEST_F(VillagerCoreTest, ConstructorFood)
{
	// 0.3 + 0.5 < 1 -> a second draw kept unclamped: 0.55 + 0.5 = 1.05
	const auto& info = Locator::infoConstants::value().villager.at(0);
	auto a = MakeVillager(0);
	SetDraws({}, {0.3f, 0.55f});
	villager::Construct(a, info, 25, 100, false, {});
	EXPECT_FLOAT_EQ(V(a).food, 1.05f);
	// a first draw of 0.55 -> 1.0, one draw
	auto b = MakeVillager(0);
	SetDraws({}, {0.55f});
	villager::Construct(b, info, 25, 100, false, {});
	EXPECT_FLOAT_EQ(V(b).food, 1.0f);
	EXPECT_EQ(std::count(_draws.begin(), _draws.end(), "F0.6"), 1);
}

TEST_F(VillagerCoreTest, ConstructorLastCheckTurn)
{
	const auto& info = Locator::infoConstants::value().villager.at(0);
	// turn 100, GameRand 3 (< 100) then 5 -> 95
	auto a = MakeVillager(0);
	SetDraws({3, 5, 0}, {0.6f});
	villager::Construct(a, info, 25, 100, false, {});
	EXPECT_EQ(V(a).lastCheckTurn, 95u);
	// turn 2, GameRand 5 (not < 2) -> 2 - 2 = 0, one draw
	auto b = MakeVillager(0);
	SetDraws({5, 0}, {0.6f});
	villager::Construct(b, info, 25, 2, false, {});
	EXPECT_EQ(V(b).lastCheckTurn, 0u);
	EXPECT_EQ(std::count(_draws.begin(), _draws.end(), "R8"), 1);
}

TEST_F(VillagerCoreTest, ConstructorCounterStateAndWater)
{
	const auto& info = Locator::infoConstants::value().villager.at(0);
	auto a = MakeVillager(0);
	SetDraws({0, 0, 0}, {0.6f});
	villager::Construct(a, info, 25, 100, false, {});
	EXPECT_EQ(Action(a).turnsUntilStateChange, 1);
	EXPECT_EQ(Top(a), 85u);
	EXPECT_EQ(Final(a), 0u);
	EXPECT_EQ(Action(a).turnsSinceStateChange, 0);
	// in the water: 16 DROWNING with the same counter, and no entry function
	auto b = MakeVillager(0);
	SetDraws({0, 0, 41}, {0.6f});
	Table().calls.clear();
	villager::Construct(b, info, 25, 100, true, {});
	EXPECT_EQ(Top(b), 16u);
	EXPECT_EQ(Action(b).turnsUntilStateChange, 42);
	EXPECT_TRUE(Table().calls.empty());
	// adult: max(age, 18); child flag and birth turn
	auto c = MakeVillager(0);
	villager::Construct(c, info, 5, 100, false, {});
	EXPECT_NE(V(c).flags & Villager::k_FlagChild, 0);
	EXPECT_EQ(V(c).birthTurn, 100 - 5 * 1500);
	auto d = MakeVillager(0);
	villager::Construct(d, info, 14, 100, false, {});
	EXPECT_EQ(V(d).flags & Villager::k_FlagChild, 0);
	EXPECT_EQ(V(d).birthTurn, 100 - 18 * 1500);
}

TEST_F(VillagerCoreTest, ConstructorDrawOrder)
{
	// Create's GameRand(10), SetAge's scale, food (1 or 2), lastCheck (1 or 2), the counter
	const auto& info = Locator::infoConstants::value().villager.at(0);
	auto a = MakeVillager(0);
	SetDraws({0, 3, 5, 0}, {0.1f, 0.2f});
	villager::RollSpecialVillager();
	villager::Construct(a, info, 25, 100, false, [this](uint32_t) { _draws.push_back("scale"); });
	const std::vector<std::string> expected = {"R10", "scale", "F0.6", "F0.6", "R8", "R8", "R500"};
	EXPECT_EQ(_draws, expected);
}

TEST_F(VillagerCoreTest, CreatedCountsDown)
{
	auto a = MakeVillager(85);
	Action(a).turnsUntilStateChange = 2;
	villager::VillagerCreated(Action(a));
	villager::VillagerCreated(Action(a));
	EXPECT_EQ(Top(a), 85u);
	villager::VillagerCreated(Action(a));
	EXPECT_EQ(Top(a), 163u);
	EXPECT_EQ(Action(a).turnsUntilStateChange, 0);
}

TEST_F(VillagerCoreTest, ExitRefused)
{
	auto a = MakeVillager(163);
	Action(a).turnsSinceStateChange = 7;
	Table().rows[163].exit = 0;
	EXPECT_EQ(villager::SetTopState(a, S(163)), villager::k_ExitRefused);
	EXPECT_EQ(Top(a), 163u);
	EXPECT_EQ(Action(a).turnsSinceStateChange, 7);
	EXPECT_EQ(Table().Count("entry 163 163 163"), 0u);
}

TEST_F(VillagerCoreTest, ExitRefusedByFinal)
{
	// TOP 1 (not final, no exit), FINAL 36 whose exit refuses
	auto a = MakeVillager(1, 36);
	Table().rows[36].exit = 0;
	SetDraws({}, {0.9f}); // 163 has no pause anyway
	EXPECT_EQ(villager::SetTopState(a, S(163)), villager::k_ExitRefused);
	EXPECT_EQ(Top(a), 1u);
	EXPECT_EQ(Final(a), 36u);
	// TOP == FINAL (36 is final): its exit runs once
	auto b = MakeVillager(36);
	Table().rows[36].exit = 1;
	Table().calls.clear();
	villager::SetTopState(b, S(163));
	EXPECT_EQ(Table().Count("exit 36 163"), 1u);
}

TEST_F(VillagerCoreTest, EntryRefused)
{
	auto a = MakeVillager(163);
	Table().rows[19].entry = 0;
	EXPECT_EQ(villager::SetTopState(a, S(19)), villager::k_EntryRefused);
	EXPECT_EQ(Top(a), 163u); // CallEntryStateFunction(163) after the refusal
	EXPECT_EQ(Final(a), 0u);
	EXPECT_EQ(Table().Count("entry 163 163 163"), 1u);
}

TEST_F(VillagerCoreTest, EntrySetsItself)
{
	auto a = MakeVillager(163);
	Table().rows[19].entry = 0x23;
	EXPECT_EQ(villager::SetTopState(a, S(19)), villager::k_Done);
	EXPECT_EQ(Top(a), 163u);
}

TEST_F(VillagerCoreTest, SettingTopClearsFinalWithTheTown)
{
	auto a = MakeVillager(1, 36);
	V(a).town = MakeTown();
	villager::SetState(a, Index::Top, S(163));
	EXPECT_EQ(Final(a), 0u);
	const auto& town = Locator::entitiesRegistry::value().Get<Town>(V(a).town);
	EXPECT_FLOAT_EQ(town.desire.doingNow.at(8), -1.0f);
	EXPECT_FLOAT_EQ(town.desire.doingNowCount.at(8), -1.0f);
}

TEST_F(VillagerCoreTest, PreviousRule)
{
	auto a = MakeVillager(163);
	V(a).town = MakeTown();
	villager::SetState(a, Index::Previous, S(24));
	EXPECT_EQ(Action(a).states.at(2), 0u);
	villager::SetState(a, Index::Previous, S(36));
	EXPECT_EQ(Action(a).states.at(2), 36u);
	const auto& town = Locator::entitiesRegistry::value().Get<Town>(V(a).town);
	EXPECT_FLOAT_EQ(town.desire.doingNowCount.at(8), 1.0f); // the original's quirk: PREVIOUS counts too
}

TEST_F(VillagerCoreTest, AdjustTownModifier)
{
	auto a = MakeVillager(163);
	villager::AdjustTownModifier(a, S(36), true); // no town: nothing
	V(a).town = MakeTown();
	const auto& town = Locator::entitiesRegistry::value().Get<Town>(V(a).town);
	villager::AdjustTownModifier(a, S(163), true); // desire -1: nothing
	EXPECT_FLOAT_EQ(town.desire.doingNowCount.at(8), 0.0f);
	villager::AdjustTownModifier(a, S(36), true);
	EXPECT_FLOAT_EQ(town.desire.doingNow.at(8), 1.0f);
	EXPECT_FLOAT_EQ(town.desire.doingNowCount.at(8), 1.0f);
	villager::AdjustTownModifier(a, S(36), false);
	EXPECT_FLOAT_EQ(town.desire.doingNow.at(8), 0.0f);
	EXPECT_FLOAT_EQ(town.desire.doingNowCount.at(8), 0.0f);
}

TEST_F(VillagerCoreTest, PauseForASecondChance)
{
	// life 0.5: x = 0.5, x^3 = 0.125: rand - 0.0625 < 0.01
	auto a = MakeVillager(163);
	V(a).life = 0.5f;
	SetDraws({}, {0.0724f});
	EXPECT_EQ(villager::SetTopState(a, S(36)), villager::k_Done);
	EXPECT_EQ(Top(a), 239u);
	EXPECT_EQ(Final(a), 36u);
	EXPECT_EQ(Table().Count("entry 36 163 36"), 1u); // the entry of s runs when the pause starts
	auto b = MakeVillager(163);
	V(b).life = 0.5f;
	SetDraws({}, {0.0726f});
	villager::SetTopState(b, S(36));
	EXPECT_EQ(Top(b), 36u);
	// poisoned: x = 1 - 0.25, threshold 0.01 + 0.2109
	auto c = MakeVillager(163);
	V(c).life = 0.5f;
	Locator::entitiesRegistry::value().Assign<Poisoned>(c);
	SetDraws({}, {0.2208f});
	villager::SetTopState(c, S(36));
	EXPECT_EQ(Top(c), 239u);
	auto d = MakeVillager(163);
	V(d).life = 0.5f;
	Locator::entitiesRegistry::value().Assign<Poisoned>(d);
	SetDraws({}, {0.2210f});
	villager::SetTopState(d, S(36));
	EXPECT_EQ(Top(d), 36u);
	// TOP 239 or a state without the pause flag: no draw
	auto e = MakeVillager(239, 36);
	SetDraws({}, {});
	villager::SetTopState(e, S(36));
	auto f = MakeVillager(163);
	villager::SetTopState(f, S(19));
	EXPECT_TRUE(_draws.empty());
}

TEST_F(VillagerCoreTest, PauseEndsInTheFinalState)
{
	auto a = MakeVillager(163);
	V(a).life = 0.5f;
	SetDraws({}, {0.0f});
	villager::SetTopState(a, S(36));
	ASSERT_EQ(Top(a), 239u);
	_draws.clear();
	// no clip resources in the tests: the clip is over
	villager::PauseForASecond(Action(a));
	EXPECT_EQ(Top(a), 36u);
	EXPECT_EQ(Final(a), 0u);
	EXPECT_EQ(Table().Count("entry 36 163 36"), 1u);
	EXPECT_EQ(Table().Count("entry 36 36 36"), 1u); // the entry again, told the final state of the pause
	EXPECT_TRUE(_draws.empty());                    // TOP was 239: no second pause
}

TEST_F(VillagerCoreTest, PeriodicChecksEveryNineTurns)
{
	auto a = MakeVillager(36);
	V(a).lastCheckTurn = 95;
	std::vector<uint32_t> checks;
	for (uint32_t turn = 96; turn <= 125; ++turn)
	{
		const auto before = V(a).lastCheckTurn;
		villager::CheckEveryTime(a, turn);
		if (V(a).lastCheckTurn != before)
		{
			checks.push_back(turn);
		}
	}
	const std::vector<uint32_t> expected = {104, 113, 122};
	EXPECT_EQ(checks, expected);
	// a state without checks (163): none
	auto b = MakeVillager(163);
	V(b).lastCheckTurn = 95;
	for (uint32_t turn = 96; turn <= 125; ++turn)
	{
		villager::CheckEveryTime(b, turn);
	}
	EXPECT_EQ(V(b).lastCheckTurn, 95u);
}

TEST_F(VillagerCoreTest, LifeWear)
{
	auto a = MakeVillager(1, 36);
	V(a).lastCheckTurn = 100;
	villager::CheckEveryTime(a, 100);
	EXPECT_FLOAT_EQ(V(a).life, 1.0f - 2e-6f);
	auto b = MakeVillager(36);
	villager::CheckEveryTime(b, 100);
	EXPECT_FLOAT_EQ(V(b).life, 1.0f);
}

TEST_F(VillagerCoreTest, DeathReasons)
{
	auto a = MakeVillager(36);
	V(a).life = 0.0f;
	Table().calls.clear();
	villager::ProcessState(a, 100);
	EXPECT_EQ(villager::GetDeathReason(a), DeathReason::Exhaustion);
	EXPECT_EQ(Table().Count("state 36"), 0u); // the death left 36 (SetDying: TOP 14) before CallState
	auto b = MakeVillager(36);
	V(b).life = 0.0f;
	V(b).flags = Villager::k_FlagAtWorshipSite;
	villager::CheckEveryTime(b, 100);
	EXPECT_EQ(villager::GetDeathReason(b), DeathReason::Chant);
	// the bit as the worship code keeps it today: WorshipVillager::atSite
	auto d = MakeVillager(36);
	V(d).life = 0.0f;
	Locator::entitiesRegistry::value().Assign<WorshipVillager>(d).atSite = true;
	villager::CheckEveryTime(d, 100);
	EXPECT_EQ(villager::GetDeathReason(d), DeathReason::Chant);
	// GetFinalState 248 GO_HOME_FROM_WORSHIP
	auto c = MakeVillager(248);
	V(c).life = 0.0f;
	villager::CheckEveryTime(c, 100);
	EXPECT_EQ(villager::GetDeathReason(c), DeathReason::Chant);
}

TEST_F(VillagerCoreTest, HurtGoesHome)
{
	// switched off (on in the game): the villager stays
	test::FakeVillagerRules rules;
	rules.goHomeEnabled = [] { return false; };
	Locator::villagerRules::emplace<test::FakeVillagerRules>(rules);
	auto off = MakeVillager(1, 0);
	V(off).life = 0.29f;
	V(off).lastCheckTurn = 0;
	villager::CheckEveryTime(off, 100);
	EXPECT_EQ(Top(off), 1u);
	Locator::villagerRules::emplace<ecs::systems::VillagerRules>();
	// TOP 1 (moving) with FINAL 0: the checks are row 0's (checks 1, goes home 1)
	auto a = MakeVillager(1, 0);
	V(a).life = 0.29f;
	V(a).lastCheckTurn = 0;
	SetDraws({}, {0.9f}); // 36 may pause: no
	villager::CheckEveryTime(a, 100);
	EXPECT_EQ(Top(a), 36u);
	// reacting to food (19): only when food > hungryForFood
	auto b = MakeVillager(19);
	V(b).life = 0.29f;
	V(b).food = 0.5f;
	villager::CheckEveryTime(b, 100);
	EXPECT_EQ(Top(b), 19u);
	auto c = MakeVillager(19);
	V(c).life = 0.29f;
	V(c).food = 0.6f;
	SetDraws({}, {0.9f});
	villager::CheckEveryTime(c, 100);
	EXPECT_EQ(Top(c), 36u);
	// downed by a predator: no
	auto d = MakeVillager(1, 0);
	V(d).life = 0.29f;
	Locator::entitiesRegistry::value().Assign<DownedVillager>(d);
	villager::CheckEveryTime(d, 100);
	EXPECT_EQ(Top(d), 1u);
}

TEST_F(VillagerCoreTest, SetupMoveToWithHugKeepsFinal)
{
	auto a = MakeVillager(163);
	Locator::entitiesRegistry::value().Assign<WallHug>(a, glm::vec2(), glm::vec2(), 0.0f, 0.1f);
	EXPECT_EQ(villager::SetupMoveToWithHug(a, glm::vec2(10.0f, 20.0f), S(219)), 1u);
	EXPECT_EQ(Top(a), 1u);
	EXPECT_EQ(Final(a), 219u);
	EXPECT_TRUE(Locator::entitiesRegistry::value().AllOf<MoveStateLinearTag>(a));
	// the entry of d refused: back to 163, no walk
	auto b = MakeVillager(163);
	Locator::entitiesRegistry::value().Assign<WallHug>(b, glm::vec2(), glm::vec2(), 0.0f, 0.1f);
	Table().rows[219].entry = 0;
	EXPECT_EQ(villager::SetupMoveToWithHug(b, glm::vec2(10.0f, 20.0f), S(219)), 0u);
	EXPECT_EQ(Top(b), 163u);
	EXPECT_FALSE(Locator::entitiesRegistry::value().AllOf<MoveStateLinearTag>(b));
}

TEST_F(VillagerCoreTest, Power)
{
	EXPECT_FLOAT_EQ(villager::Power(0.5f), 0.875f);
	EXPECT_FLOAT_EQ(villager::Power(1.1f), 0.0f);
	EXPECT_FLOAT_EQ(villager::Power(0.0f), 1.0f);
}

// ---- the script's villager (ECS/Villager/VillagerScript.h: MOVE_GAME_THING, SET_SCRIPT_STATE / _ULONG, PLAYED) ----

namespace
{
entt::entity MakeWalker(uint32_t top, glm::vec2 at, float speed)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto e = registry.Create();
	auto& v = registry.Assign<Villager>(e);
	v.life = 1.0f;
	v.town = entt::null;
	v.abode = entt::null;
	registry.Assign<LivingAction>(e, static_cast<VillagerStates>(top), static_cast<uint16_t>(7));
	registry.Assign<WallHug>(e, glm::vec2(), glm::vec2(), 0.0f, speed);
	auto& transform = registry.Assign<Transform>(e);
	transform.position = glm::vec3(at.x, 0.0f, at.y);
	// a script's villager: setting a state keeps its speed (the script-controlled flag)
	ecs::script_held::SetControlledByScript(e, true);
	return e;
}
} // namespace

TEST_F(VillagerCoreTest, ScriptAreWeThereIsStrict)
{
	// AreWeThere: d^2 < (speed + r)^2
	const auto a = MakeWalker(163, glm::vec2(0.0f), 0.5f);
	EXPECT_TRUE(villager::AreWeThere(a, glm::vec2(0.4f, 0.0f), 0.0f));
	EXPECT_FALSE(villager::AreWeThere(a, glm::vec2(0.5f, 0.0f), 0.0f));
	EXPECT_TRUE(villager::AreWeThere(a, glm::vec2(0.5f, 0.0f), 0.1f));
}

TEST_F(VillagerCoreTest, ScriptSetupMoveToPosStepsThrough)
{
	// SetupMoveToPos -> SetupMobileMoveToPos: TOP MOVE_TO_POS, FINAL IN_SCRIPT, STEP_THROUGH
	auto& registry = Locator::entitiesRegistry::value();
	const auto a = MakeWalker(163, glm::vec2(0.0f), 0.1f);
	EXPECT_EQ(villager::SetupMoveToPos(a, glm::vec2(10.0f, 0.0f), VillagerStates::InScript), 1u);
	EXPECT_EQ(Top(a), 1u);
	EXPECT_EQ(Final(a), 4u);
	EXPECT_TRUE(registry.AllOf<MoveStateStepThroughTag>(a));
	EXPECT_FALSE(registry.AllOf<MoveStateLinearTag>(a));
	const auto& wallHug = registry.Get<WallHug>(a);
	EXPECT_EQ(wallHug.goal, glm::vec2(10.0f, 0.0f));
	// InitStepsXZ: a = 0 (+x), speed 0.1 m = 655 (ConvertMetersToWholeDistance), x = (cos 0 = 65536 x (655 >> 4)) >> 12
	// = 640 MapCoords = 0.09765625 m, z = sin 0 = 0
	EXPECT_EQ(wallHug.step.x, 0.09765625f);
	EXPECT_EQ(wallHug.step.y, 0.0f);
	// there already: ARRIVED
	const auto b = MakeWalker(163, glm::vec2(5.0f, 5.0f), 0.1f);
	EXPECT_EQ(villager::SetupMoveToPos(b, glm::vec2(5.05f, 5.0f), VillagerStates::InScript), 1u);
	EXPECT_TRUE(registry.AllOf<MoveStateArrivedTag>(b));
	// the exit refused: no walk, 0
	const auto c = MakeWalker(163, glm::vec2(0.0f), 0.1f);
	Table().rows[163].exit = 0;
	EXPECT_EQ(villager::SetupMoveToPos(c, glm::vec2(10.0f, 0.0f), VillagerStates::InScript), 0u);
	EXPECT_FALSE(registry.AllOf<MoveStateStepThroughTag>(c));
}

TEST_F(VillagerCoreTest, ScriptSetScriptStateStopsTheWalk)
{
	// SetScriptState: StorePreviousState, exit, entry, SetAnim(1), turns until the state change = 0
	auto& registry = Locator::entitiesRegistry::value();
	const auto a = MakeWalker(163, glm::vec2(0.0f), 0.1f);
	ASSERT_EQ(villager::SetupMoveToPos(a, glm::vec2(10.0f, 0.0f), VillagerStates::InScript), 1u);
	Table().calls.clear();
	villager::SetScriptState(a, VillagerStates::ScriptPlayAnim);
	EXPECT_EQ(Top(a), 200u);
	EXPECT_EQ(Action(a).turnsUntilStateChange, 0u);
	EXPECT_EQ(Table().Count("exit 1 200"), 1u);
	EXPECT_EQ(Table().Count("entry 200 4 200"), 1u);
	EXPECT_FALSE(registry.AllOf<MoveStateStepThroughTag>(a));
	// in the hand: nothing (IsObjectInMap)
	const auto b = MakeWalker(24, glm::vec2(0.0f), 0.1f);
	villager::SetScriptState(b, VillagerStates::ScriptPlayAnim);
	EXPECT_EQ(Top(b), 24u);
}

TEST_F(VillagerCoreTest, ScriptPlayAnimCountsDown)
{
	// ScriptPlayAnim and IsScriptAnimationComplete
	const auto a = MakeWalker(200, glm::vec2(0.0f), 0.1f);
	villager::SetScriptAnimation(a, 224, 2);
	EXPECT_EQ(V(a).scriptAnim, 224u);
	EXPECT_EQ(villager::ScriptAnimation(a), 224);
	EXPECT_FALSE(villager::IsScriptAnimationComplete(a)); // TOP 200, 2 left
	villager::ScriptPlayAnim(Action(a));
	EXPECT_EQ(V(a).scriptAnimLoops, 1u);
	EXPECT_EQ(Top(a), 23u); // PlayAnimThenSetState: WAIT_FOR_ANIMATION, then 200 again
	EXPECT_EQ(Final(a), 200u);
	EXPECT_FALSE(villager::IsScriptAnimationComplete(a));
	// the clip ends (no animation here: ready at once): SetTopStateToFinal
	EXPECT_EQ(villager::WaitForAnimation(Action(a)), 0u);
	EXPECT_EQ(Top(a), 200u);
	villager::ScriptPlayAnim(Action(a));
	EXPECT_EQ(V(a).scriptAnimLoops, 0u);
	EXPECT_EQ(Top(a), 23u);
	EXPECT_EQ(Final(a), 4u); // the last time goes back to IN_SCRIPT
	EXPECT_EQ(villager::WaitForAnimation(Action(a)), 0u);
	EXPECT_EQ(Top(a), 4u);
	EXPECT_TRUE(villager::IsScriptAnimationComplete(a));
	// TOP 200 with no times left: complete, and the state does nothing
	const auto b = MakeWalker(200, glm::vec2(0.0f), 0.1f);
	EXPECT_TRUE(villager::IsScriptAnimationComplete(b));
	EXPECT_EQ(villager::ScriptPlayAnim(Action(b)), 1u);
	EXPECT_EQ(Top(b), 200u);
}

TEST_F(VillagerCoreTest, ScriptExitInScript)
{
	// ExitInScript: a script state next -> 1; else ExitNoChangeState
	// the test's own InfoConstants (SetUp), written through the Locator's const view
	auto& table = const_cast<InfoConstants&>(Locator::infoConstants::value()).villagerStateTable;
	table.at(200).isScriptState = 1;
	const auto a = MakeWalker(4, glm::vec2(0.0f), 0.1f);
	EXPECT_EQ(villager::ExitInScript(Action(a), VillagerStates::ScriptPlayAnim), 1u);
	// 24 IN_HAND: IsStateForInterface -> 1
	EXPECT_EQ(villager::ExitInScript(Action(a), VillagerStates::InHand), 1u);
	// 36 GO_HOME (final, another exit than the final state's, not script-interruptable): 0
	EXPECT_EQ(villager::ExitInScript(Action(a), VillagerStates::GoHome), 0u);
	table.at(36).isScriptInterruptableState = 1;
	EXPECT_EQ(villager::ExitInScript(Action(a), VillagerStates::GoHome), 1u);
}

// ---- a knocked resting proxy (PhysicsObject::GameTurnUpdate -> the class's InitialisePhysics) ------------------------

namespace
{
/// The registered ClassHandlers::initialisePhysicsKnocked of the entity's class: true = started (the body is woken)
bool Knock(entt::entity entity)
{
	ecs::living::RegisterPhysicsHandlers();
	const auto& handlers = ecs::physics::PhysicsObjects::Handlers(entity);
	if (!handlers.initialisePhysicsKnocked)
	{
		ADD_FAILURE() << "no initialisePhysicsKnocked handler";
		return false;
	}
	ecs::physics::PhysicsObject po;
	po.entity = entity;
	return handlers.initialisePhysicsKnocked(entity, po);
}

entt::entity MakeAnimal(uint8_t top, uint8_t final, uint8_t previous)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto e = registry.Create();
	registry.Assign<Animal>(e).type = AnimalInfo::Sheep;
	registry.Assign<Transform>(e, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	auto& brain = registry.Assign<AnimalBrain>(e);
	brain.topState = top;
	brain.finalState = final;
	brain.previousState = previous;
	return e;
}
} // namespace

TEST_F(VillagerCoreTest, KnockedVillagerFlies)
{
	// walking (TOP 1 MOVE_TO_POS, FINAL 36 GO_HOME): InitialisePhysics stores the final state as PREVIOUS
	// (StorePreviousState), SetTopState(FLYING) takes -> started
	const auto walking = MakeVillager(1, 36);
	EXPECT_TRUE(Knock(walking));
	EXPECT_EQ(Top(walking), 10u);
	EXPECT_EQ(Action(walking).states.at(2), 36u);
	// deciding (TOP 163, final): PREVIOUS = 163
	const auto deciding = MakeVillager(163);
	EXPECT_TRUE(Knock(deciding));
	EXPECT_EQ(Top(deciding), 10u);
	EXPECT_EQ(Action(deciding).states.at(2), 163u);
}

TEST_F(VillagerCoreTest, KnockedVillagerInHandKeepsPrevious)
{
	// IN_HAND: no StorePreviousState (24 made storable here, so a store would write 24)
	auto info = std::make_unique<InfoConstants>(Locator::infoConstants::value());
	info->villagerStateTable.at(24).keepsPreviousState = 0;
	Locator::infoConstants::reset(info.release());
	const auto held = MakeVillager(24);
	Action(held).states.at(2) = 163;
	EXPECT_TRUE(Knock(held));
	EXPECT_EQ(Top(held), 10u);
	EXPECT_EQ(Action(held).states.at(2), 163u);
}

TEST_F(VillagerCoreTest, KnockedVillagerNotStarted)
{
	// SetTopState(FLYING) refused (its entry function says 0): not started, the body is not woken
	Table().rows[10].entry = 0;
	const auto refused = MakeVillager(163);
	EXPECT_FALSE(Knock(refused));
	EXPECT_EQ(Top(refused), 163u);
	// carried by a particle system: GameTurnUpdate does not call InitialisePhysics at all
	Table().rows[10].entry = 1;
	const auto carried = MakeVillager(1, 36);
	Locator::entitiesRegistry::value().Assign<CarriedByParticleSystem>(carried);
	EXPECT_FALSE(Knock(carried));
	EXPECT_EQ(Top(carried), 1u);
	EXPECT_EQ(Action(carried).states.at(2), 0u);
}

TEST_F(VillagerCoreTest, KnockedAnimalFlies)
{
	using ecs::animal_ai::AnimalState;
	// wandering (32): StorePreviousState keeps its final state, FLYING -> started
	const auto wandering = MakeAnimal(32, 32, 0);
	EXPECT_TRUE(Knock(wandering));
	const auto& brain = Locator::entitiesRegistry::value().Get<const AnimalBrain>(wandering);
	EXPECT_EQ(brain.topState, static_cast<uint8_t>(AnimalState::Flying));
	EXPECT_EQ(brain.previousState, 32u);
	// IN_HAND (24): PREVIOUS is kept
	const auto held = MakeAnimal(24, 24, 33);
	EXPECT_TRUE(Knock(held));
	const auto& heldBrain = Locator::entitiesRegistry::value().Get<const AnimalBrain>(held);
	EXPECT_EQ(heldBrain.topState, static_cast<uint8_t>(AnimalState::Flying));
	EXPECT_EQ(heldBrain.previousState, 33u);
}

TEST_F(VillagerCoreTest, OutOfClipIntoAStateWithNoIntoClip)
{
	// the out-of clip is over and the new state's into function gives no clip (the arrive-at-resource clip choice on
	// entry): the state's own clip starts and the wait ends on that turn. No
	// resources here: every clip lasts 0 ms, so the out-of clip is over on the first turn
	auto& table = const_cast<InfoConstants&>(Locator::infoConstants::value()).villagerStateTable;
	// info.dat: 20 ARRIVES_AT_FOOD_REACTION allows an out-of clip, its clip is 385 P_STAND
	table.at(20).field0xf0 = 0;
	const auto a = MakeVillager(22);
	const auto anim = [a]() -> const SkeletalAnimation& {
		return Locator::entitiesRegistry::value().Get<const SkeletalAnimation>(a);
	};
	// leaving 22 ARRIVES_AT_WOOD_REACTION for 20: the out-of clip 340 PICK_UP_STICKS, flags 0x800 and 0x1000
	EXPECT_EQ(ecs::VillagerCallOutOfAnimation(a, S(20)), 340);
	Action(a).states.at(0) = 20;
	ecs::VillagerSetClip(a, 340, true);
	EXPECT_EQ(anim().transitionFlags, 0x1800);
	Table().calls.clear();
	// the turn the out-of clip ends: the state's clip, the wait flag clear, the state logic still skipped
	EXPECT_EQ(villager::ProcessState(a, 100), 1u);
	EXPECT_EQ(anim().clipIndex, 385);
	EXPECT_EQ(anim().transitionFlags, 0x1000);
	EXPECT_EQ(Action(a).turnsSinceStateChange, 0);
	EXPECT_EQ(Table().Count("state 20"), 0u);
	// the next turn runs the state
	villager::ProcessState(a, 101);
	EXPECT_EQ(Table().Count("state 20"), 1u);
	EXPECT_EQ(anim().clipIndex, 385);
}

namespace
{
/// A trace argument that counts how often it is formatted
struct CountedArgument
{
	int* formatted;
};
} // namespace

template <>
struct fmt::formatter<CountedArgument>: fmt::formatter<int>
{
	auto format(const CountedArgument& argument, fmt::format_context& context) const
	{
		++*argument.formatted;
		return fmt::formatter<int>::format(*argument.formatted, context);
	}
};

TEST(VillagerTrace, NothingIsFormattedForAVillagerNobodyTraces)
{
	if (std::getenv("OPENBLACK_VILLAGER_TRACE") != nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_VILLAGER_TRACE is set";
	}
	int evaluated = 0;
	int formatted = 0;
	const auto argument = [&] {
		++evaluated;
		return CountedArgument {&formatted};
	};
	villager::TraceFormatted(entt::entity {1}, "state {} -> {}", argument(), 3);
	// the arguments are evaluated as they were in the fmt::format call; the text is not made
	EXPECT_EQ(evaluated, 1);
	EXPECT_EQ(formatted, 0);
	// the same argument formats once when the text is made
	EXPECT_EQ(fmt::format("{}", CountedArgument {&formatted}), "1");
}
