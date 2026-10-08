/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The disciples (docs/bw1-notes/villagers.md): the disciple table, SetVillagerDisciple (the range, the
// flags, the disciple type), DiscipleDecideWhatToDo's dispatch (per disciple type, and the fallback) with scripted job
// results, and the state 221 DISCIPLE_NOTHING_TO_DO (its counter, FindDisciplePrayerPos with and without a town centre,
// the entry). A fake state table in the Locator and scripted draws (the fixture of test_villager_decide.cpp). Every
// expected value is the original's.

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
#include "ECS/Systems/Implementations/VillagerWorldQueries.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerDecide.h"
#include "ECS/Villager/VillagerDisciple.h"
#include "ECS/Villager/VillagerFarmer.h"
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

/// A state table that records the calls; every entry and exit accepts (no function = 1)
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

constexpr auto D(int32_t n)
{
	return static_cast<VillagerDisciple>(n);
}

class VillagerDiscipleTest: public ::testing::Test
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
		v.minWoodToShowGraphic = 50;
		v.minFoodToShowGraphic = 100;
		v.grownUpAge = 13;
		v.sex = SexType::Male;
		v.life = 1.0f;
		v.moveState = LivingStates::LivingMoveToPos;
		// no row pauses (no GameFloatRand from SetTopState): the draws are the disciple's only
		for (auto& row : info->villagerStateTable)
		{
			row.animation = 385;
			row.field0x4 = -1;
			row.field0x8 = 1.0f;
			row.field0xf0 = 1;
		}
		auto& table = info->villagerStateTable;
		for (const uint32_t s : {31u, 58u, 163u, 221u, 244u})
		{
			table.at(s).isFinalState = 1;
		}
		table.at(1).field0x14 = 1; // MOVE_TO_POS moves
		Locator::infoConstants::reset(info.release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		Locator::rng::emplace<TestRng>();
		Locator::livingActionSystem::emplace<FakeStateTable>();
		// a filler, so that no villager or town is entity 0
		Reg().Create();
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
		cellObjects.objectsInCell = [](glm::ivec2) { return std::vector<entt::entity> {}; };
		cellObjects.get2DRadius = [this](entt::entity e) {
			const auto it = _radius.find(e);
			return it != _radius.end() ? it->second : 0.0f;
		};
		Locator::townCellObjects::emplace<test::FakeTownCellObjects>(cellObjects);
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

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	static entt::entity MakeVillager(glm::vec2 at = {100.0f, 50.0f}, uint32_t top = 163, uint32_t final = 0)
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

	/// A disciple of `type` (the disciple flag set, the disciple type = type), as SetVillagerDisciple leaves it
	static entt::entity MakeDisciple(uint8_t type, glm::vec2 at = {100.0f, 50.0f}, uint32_t top = 163, uint32_t final = 0)
	{
		const auto e = MakeVillager(at, top, final);
		V(e).flags = Villager::k_FlagDisciple;
		V(e).discipleType = type;
		return e;
	}

	static entt::entity MakeTown(glm::vec2 at = {50.0f, 50.0f})
	{
		auto& registry = Reg();
		const auto town = registry.Create();
		registry.Assign<Town>(town, 1u);
		registry.Assign<Transform>(town, glm::vec3(at.x, 0.0f, at.y), glm::mat3(1.0f), glm::vec3(1.0f));
		return town;
	}

	/// The town's centre, an object with a position and a 2D radius
	entt::entity MakeCentre(entt::entity town, glm::vec2 at, float radius)
	{
		auto& registry = Reg();
		const auto centre = registry.Create();
		registry.Assign<Transform>(centre, glm::vec3(at.x, 0.0f, at.y), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Get<Town>(town).centre = centre;
		_radius[centre] = radius;
		return centre;
	}

	static void Join(entt::entity e, entt::entity town) { V(e).town = town; }
	static LivingAction& Action(entt::entity e) { return Reg().Get<LivingAction>(e); }
	static Villager& V(entt::entity e) { return Reg().Get<Villager>(e); }
	static uint32_t Top(entt::entity e) { return Action(e).states.at(0); }
	static uint32_t Final(entt::entity e) { return Action(e).states.at(1); }
	static glm::vec2 Goal(entt::entity e) { return Reg().Get<WallHug>(e).goal; }

	std::deque<uint32_t> _ints;
	std::deque<float> _floats;
	std::vector<std::string> _draws;
	std::map<entt::entity, float> _radius;
};
} // namespace

// ---- the disciple table ---------------------------------------------------------------------------------------------

TEST_F(VillagerDiscipleTest, InfosAreTheExesTable)
{
	// read from the original: the 7 fields of the 13 records
	const std::vector<villager::DiscipleInfo> expected = {
	    {0, 0, 0, 0, 0, -1, 0},    {0, 1, 1, 1, 0, 0, 1},     {0x31, 1, 1, 1, 1, 1, 0},  {0, 1, 1, 1, 0, 0, 0},
	    {0, 1, 1, 1, 1, 9, 1},     {0xFE, 0, 1, 1, 0, 8, 1},  {0, 1, 1, 1, 0, -1, 0},    {0xE2, 0, 1, 0, 0, -1, 0},
	    {0, 1, 1, 1, 1, -1, 1},    {0xA3, 0, 1, 1, 0, -1, 0}, {0xA3, 0, 1, 0, 0, -1, 0}, {0x3A, 0, 0, 0, 0, -1, 0},
	    {0xF4, 0, 0, 0, 0, -1, 0},
	};
	ASSERT_EQ(expected.size(), villager::k_DiscipleInfoCount);
	for (uint8_t d = 0; d < villager::k_DiscipleInfoCount; ++d)
	{
		const auto& info = villager::GetDiscipleInfo(d);
		EXPECT_EQ(info.startState, expected[d].startState) << +d;
		EXPECT_EQ(info.createsJobReaction, expected[d].createsJobReaction) << +d;
		EXPECT_EQ(info.marker, expected[d].marker) << +d;
		EXPECT_EQ(info.heldAtJob, expected[d].heldAtJob) << +d;
		EXPECT_EQ(info.fetchesWood, expected[d].fetchesWood) << +d;
		EXPECT_EQ(info.townDesire, expected[d].townDesire) << +d;
		EXPECT_EQ(info.movesIntoTown, expected[d].movesIntoTown) << +d;
	}
	// the accessors: heldAtJob == 1 in CheckEveryTime (DiscipleIgnoresNeeds), fetchesWood (DiscipleFetchesWood)
	for (uint8_t d = 0; d < villager::k_DiscipleInfoCount; ++d)
	{
		EXPECT_EQ(villager::DiscipleIgnoresNeeds(d), expected[d].heldAtJob == 1) << +d;
		EXPECT_EQ(villager::DiscipleFetchesWood(d), expected[d].fetchesWood != 0) << +d;
	}
	EXPECT_EQ(villager::DiscipleStartState(12), VillagerStates::ScriptInCrowd);
	EXPECT_EQ(villager::DiscipleStartState(2), VillagerStates::ForesterArrivesAtForest);
	EXPECT_EQ(villager::DiscipleTownDesire(4), 9);
	// (guard) past the table: record 0
	EXPECT_FALSE(villager::DiscipleIgnoresNeeds(13));
	EXPECT_EQ(villager::DiscipleTownDesire(200), -1);
}

// ---- SetVillagerDisciple --------------------------------------------------------------------------------------------

TEST_F(VillagerDiscipleTest, SetVillagerDiscipleSetsFlagsAndType)
{
	const auto e = MakeVillager();
	// a follower with another flag (at home), which stays
	V(e).flags = static_cast<uint16_t>(Villager::k_FlagDiscipleFollower | Villager::k_FlagAtHome);
	// the follower flag cleared and the disciple flag set; the disciple type = it; 1
	EXPECT_EQ(villager::SetVillagerDisciple(e, entt::null, VillagerDisciple::Farmer, 0), 1u);
	EXPECT_EQ(V(e).flags, Villager::k_FlagDisciple | Villager::k_FlagAtHome);
	EXPECT_EQ(V(e).discipleType, 1);
	// another disciple: the same
	EXPECT_EQ(villager::SetVillagerDisciple(e, e, VillagerDisciple::FromVortex, 5), 1u);
	EXPECT_EQ(V(e).flags, Villager::k_FlagDisciple | Villager::k_FlagAtHome);
	EXPECT_EQ(V(e).discipleType, 12);
}

TEST_F(VillagerDiscipleTest, SetVillagerDiscipleZeroClears)
{
	const auto e = MakeVillager();
	V(e).flags = static_cast<uint16_t>(Villager::k_FlagDisciple | Villager::k_FlagDiscipleFollower | Villager::k_FlagChild);
	V(e).discipleType = 4;
	// the disciple and follower flags cleared, the disciple type = 0; 1
	EXPECT_EQ(villager::SetVillagerDisciple(e, entt::null, VillagerDisciple::None, 0), 1u);
	EXPECT_EQ(V(e).flags, Villager::k_FlagChild);
	EXPECT_EQ(V(e).discipleType, 0);
}

TEST_F(VillagerDiscipleTest, SetVillagerDiscipleOutOfRangeDoesNothing)
{
	const auto e = MakeVillager();
	V(e).flags = Villager::k_FlagDisciple;
	V(e).discipleType = 3;
	// < 0 or >= 13 -> 0, nothing changed
	EXPECT_EQ(villager::SetVillagerDisciple(e, entt::null, D(-1), 0), 0u);
	EXPECT_EQ(villager::SetVillagerDisciple(e, entt::null, D(13), 0), 0u);
	EXPECT_EQ(V(e).flags, Villager::k_FlagDisciple);
	EXPECT_EQ(V(e).discipleType, 3);
}

// ---- DiscipleDecideWhatToDo -----------------------------------------------------------------------------------------

TEST_F(VillagerDiscipleTest, Case7ReturnsZeroAtOnce)
{
	// 0, even with a town and a centre (the fallback would walk)
	const auto town = MakeTown();
	MakeCentre(town, {60.0f, 50.0f}, 2.0f);
	const auto e = MakeDisciple(7);
	Join(e, town);
	EXPECT_EQ(villager::DiscipleDecideWhatToDo(e), 0u);
	EXPECT_EQ(Top(e), 163u);
	EXPECT_TRUE(_draws.empty());
	EXPECT_EQ(V(e).discipleType, 7);
}

TEST_F(VillagerDiscipleTest, Case11GoesToWorship)
{
	// SetTopState(58); 1; the disciple stays
	const auto e = MakeDisciple(11);
	EXPECT_EQ(villager::DiscipleDecideWhatToDo(e), 1u);
	EXPECT_EQ(Top(e), 58u);
	EXPECT_EQ(V(e).discipleType, 11);
	EXPECT_NE(V(e).flags & Villager::k_FlagDisciple, 0);
}

TEST_F(VillagerDiscipleTest, Case12StartsItsStateAndStopsBeingADisciple)
{
	// SetTopState(the start state of record 12 = 244), SetVillagerDisciple(0, 0, 0); 1
	const auto e = MakeDisciple(12);
	EXPECT_EQ(villager::DiscipleDecideWhatToDo(e), 1u);
	EXPECT_EQ(Top(e), 244u);
	EXPECT_EQ(V(e).discipleType, 0);
	EXPECT_EQ(V(e).flags & (Villager::k_FlagDisciple | Villager::k_FlagDiscipleFollower), 0);
}

TEST_F(VillagerDiscipleTest, Case10ClearsTheDiscipleThenFallsBack)
{
	// SetVillagerDisciple(this, 0, 0) first; CheckMoveHouse (pending) not 1 -> the fallback, whose disciple flag test
	// then fails: 0, no walk
	const auto town = MakeTown();
	MakeCentre(town, {60.0f, 50.0f}, 2.0f);
	const auto e = MakeDisciple(10);
	Join(e, town);
	EXPECT_EQ(villager::DiscipleDecideWhatToDo(e), 0u);
	EXPECT_EQ(V(e).discipleType, 0);
	EXPECT_EQ(V(e).flags & Villager::k_FlagDisciple, 0);
	EXPECT_EQ(Top(e), 163u);
}

TEST_F(VillagerDiscipleTest, FallbackWalksAHeldDiscipleToItsPrayerPos)
{
	// case 6 is the fallback: final not 221, a disciple, heldAtJob == 1 -> SetDiscipleNothingToDo: turns until the
	// state change = 0, SetupMoveToWithHug(FindDisciplePrayerPos, 221); 1
	const auto town = MakeTown({50.0f, 50.0f});
	const auto e = MakeDisciple(6);
	Join(e, town);
	Action(e).turnsUntilStateChange = 77;
	EXPECT_EQ(villager::DiscipleDecideWhatToDo(e), 1u);
	EXPECT_EQ(Top(e), 1u);     // MOVE_TO_POS
	EXPECT_EQ(Final(e), 221u); // DISCIPLE_NOTHING_TO_DO
	EXPECT_EQ(Action(e).turnsUntilStateChange, 0);
	// no centre: the town's position
	const auto goal = tq::ToMetres(tq::ToMapCoords({50.0f, 50.0f}));
	EXPECT_FLOAT_EQ(Goal(e).x, goal.x);
	EXPECT_FLOAT_EQ(Goal(e).y, goal.y);
}

TEST_F(VillagerDiscipleTest, FallbackAnswersZero)
{
	const auto town = MakeTown();
	// no town: FindDisciplePrayerPos 0 -> 0
	const auto homeless = MakeDisciple(6);
	EXPECT_EQ(villager::DiscipleDecideWhatToDo(homeless), 0u);
	EXPECT_EQ(Top(homeless), 163u);
	// the final state already 221 (TOP 1 is not a final state, so the final state is FINAL) -> 0
	const auto praying = MakeDisciple(6, {100.0f, 50.0f}, 1, 221);
	Join(praying, town);
	EXPECT_EQ(villager::DiscipleDecideWhatToDo(praying), 0u);
	EXPECT_EQ(Top(praying), 1u);
	// a follower only (disciple 0: out of the table's 1..12) -> the fallback, without the disciple flag -> 0
	const auto follower = MakeVillager();
	Join(follower, town);
	V(follower).flags = Villager::k_FlagDiscipleFollower;
	EXPECT_EQ(villager::DiscipleDecideWhatToDo(follower), 0u);
	// the disciple flag with disciple 0 (out of 1..12): the fallback, record 0's heldAtJob == 0 -> 0
	const auto odd = MakeDisciple(0);
	Join(odd, town);
	EXPECT_EQ(villager::DiscipleDecideWhatToDo(odd), 0u);
	EXPECT_TRUE(_draws.empty());
}

TEST_F(VillagerDiscipleTest, JobCasesGoThroughTheirTests)
{
	// the jobs' results scripted; no town, so the fallback answers 0
	std::vector<uint32_t> asked;
	uint32_t result = 0;
	test::FakeVillagerDiscipleJobs jobs;
	jobs.discipleJob = [&](entt::entity, uint8_t d) {
		asked.push_back(d);
		return result;
	};
	Locator::villagerDiscipleJobs::emplace<test::FakeVillagerDiscipleJobs>(jobs);
	for (const uint8_t d : std::vector<uint8_t> {2, 3, 4, 5, 8, 9})
	{
		const auto e = MakeDisciple(d);
		// == 1 -> 1 (case 8 takes any non-zero result)
		result = 1;
		EXPECT_EQ(villager::DiscipleDecideWhatToDo(e), 1u) << +d;
		// 2: only case 8 takes any non-zero result; the others fall back (0 here)
		result = 2;
		EXPECT_EQ(villager::DiscipleDecideWhatToDo(e), d == 8 ? 1u : 0u) << +d;
		// 0 -> the fallback
		result = 0;
		EXPECT_EQ(villager::DiscipleDecideWhatToDo(e), 0u) << +d;
	}
	EXPECT_EQ(asked, (std::vector<uint32_t> {2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 5, 8, 8, 8, 9, 9, 9}));
}

TEST_F(VillagerDiscipleTest, Case1HarvestFieldWithFoodDropsItOff)
{
	// a town, FindBestField finds one, GetFieldActivity == 2 and the food held > minFoodToShowGraphic (100)
	// -> SetTopState(31); 1
	const auto town = MakeTown();
	const auto field = Reg().Create();
	Reg().Assign<Transform>(field, glm::vec3(110.0f, 0.0f, 50.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	test::FakeVillagerFields ops;
	ops.townFields = [field](entt::entity) { return std::vector<entt::entity> {field}; };
	ops.getDesireToBeFarmed = [](entt::entity) { return 1.0f; };
	ops.getFieldActivity = [](entt::entity) { return 2; };
	Locator::villagerFields::emplace<test::FakeVillagerFields>(ops);
	const auto e = MakeDisciple(1);
	Join(e, town);
	V(e).resourceHeld.at(0) = 101;
	EXPECT_EQ(villager::DiscipleDecideWhatToDo(e), 1u);
	EXPECT_EQ(Top(e), 31u);
	// no field in the town: FindBestField null -> the fallback (no centre: the town's position)
	ops.townFields = [](entt::entity) { return std::vector<entt::entity> {}; };
	Locator::villagerFields::emplace<test::FakeVillagerFields>(ops);
	const auto idle = MakeDisciple(1);
	Join(idle, town);
	EXPECT_EQ(villager::DiscipleDecideWhatToDo(idle), 1u);
	EXPECT_EQ(Final(idle), 221u);
}

// ---- FindDisciplePrayerPos ------------------------------------------------------------------------------------------

TEST_F(VillagerDiscipleTest, PrayerPosWithoutTownOrCentre)
{
	const auto e = MakeDisciple(1);
	glm::ivec2 out(7, 9);
	// no town -> 0, out untouched
	EXPECT_EQ(villager::FindDisciplePrayerPos(e, out), 0u);
	EXPECT_EQ(out, glm::ivec2(7, 9));
	// no centre -> the town's position; 1; no draw
	const auto town = MakeTown({40.0f, 30.0f});
	Join(e, town);
	EXPECT_EQ(villager::FindDisciplePrayerPos(e, out), 1u);
	EXPECT_EQ(out, tq::ToMapCoords({40.0f, 30.0f}));
	EXPECT_TRUE(_draws.empty());
}

TEST_F(VillagerDiscipleTest, PrayerPosAroundTheCentreOnMySide)
{
	const auto town = MakeTown();
	const auto centre = MakeCentre(town, {60.0f, 50.0f}, 2.0f);
	const auto e = MakeDisciple(1, {100.0f, 50.0f});
	Join(e, town);
	const auto at = tq::PosOf(centre);
	const auto me = tq::PosOf(e);
	const float toMe = tq::Get3DAngleFromXZ(at, me);
	// GameFloatRand(pi / 2) = pi / 4 cancels the - pi / 4: straight at me; GameFloatRand(4) = 1 + the radius 2 -> 3 m
	// from the centre
	SetDraws({}, {glm::quarter_pi<float>(), 1.0f});
	glm::ivec2 out(0);
	EXPECT_EQ(villager::FindDisciplePrayerPos(e, out), 1u);
	EXPECT_EQ(_draws, (std::vector<std::string> {"F1.5", "F4.0"}));
	EXPECT_EQ(out, at + tq::GetPosFromAngle(toMe, 3.0f));
	EXPECT_NEAR(tq::GetDistanceInMetres(at, out), 3.0f, 0.01f);
	EXPECT_NEAR(tq::GetDistanceInMetres(me, out), 37.0f, 0.05f);
	// both draws 0: - pi / 4 off my side, the radius alone
	SetDraws({}, {0.0f, 0.0f});
	EXPECT_EQ(villager::FindDisciplePrayerPos(e, out), 1u);
	EXPECT_EQ(out, at + tq::GetPosFromAngle(toMe - glm::quarter_pi<float>(), 2.0f));
	EXPECT_NEAR(tq::GetDistanceInMetres(at, out), 2.0f, 0.01f);
}

// ---- 221 DISCIPLE_NOTHING_TO_DO -------------------------------------------------------------------------------------

TEST_F(VillagerDiscipleTest, NothingToDoCountsDown)
{
	const auto town = MakeTown();
	const auto e = MakeDisciple(7, {100.0f, 50.0f}, 221);
	Join(e, town);
	// the turns until the state change go down; still > 0 -> 1, no draw
	Action(e).turnsUntilStateChange = 5;
	EXPECT_EQ(villager::DiscipleNothingToDo(Action(e)), 1u);
	EXPECT_EQ(Action(e).turnsUntilStateChange, 4);
	EXPECT_TRUE(_draws.empty());
	EXPECT_EQ(Top(e), 221u);
}

TEST_F(VillagerDiscipleTest, NothingToDoPulseRestartsTheCounter)
{
	// the town's build pulse -> the turns until the state change = GameRand(10), then the decrement
	const auto town = MakeTown();
	Reg().Get<Town>(town).buildPulse = 1;
	const auto e = MakeDisciple(7, {100.0f, 50.0f}, 221);
	Join(e, town);
	Action(e).turnsUntilStateChange = 200;
	SetDraws({7}, {});
	EXPECT_EQ(villager::DiscipleNothingToDo(Action(e)), 1u);
	EXPECT_EQ(_draws, (std::vector<std::string> {"R10"}));
	EXPECT_EQ(Action(e).turnsUntilStateChange, 6);
}

TEST_F(VillagerDiscipleTest, NothingToDoAtZeroAsksAgain)
{
	// at 0 (the clip done: no clip in the tests): DiscipleDecideWhatToDo; 0 (disciple 7) -> the turns until the state
	// change = 300
	const auto town = MakeTown();
	const auto e = MakeDisciple(7, {100.0f, 50.0f}, 221);
	Join(e, town);
	Action(e).turnsUntilStateChange = 1;
	EXPECT_EQ(villager::DiscipleNothingToDo(Action(e)), 1u);
	EXPECT_EQ(Action(e).turnsUntilStateChange, 300);
	EXPECT_EQ(Top(e), 221u);
	// a pulse that draws 0: the counter goes below 0 (a signed 16-bit -1, not above 0) and it asks at once
	Reg().Get<Town>(town).buildPulse = 1;
	SetDraws({0}, {});
	EXPECT_EQ(villager::DiscipleNothingToDo(Action(e)), 1u);
	EXPECT_EQ(Action(e).turnsUntilStateChange, 300);
	// something found (disciple 11: SetTopState(58)): the turns until the state change are not set to 300
	const auto worshipper = MakeDisciple(11, {100.0f, 50.0f}, 221);
	Join(worshipper, town);
	Reg().Get<Town>(town).buildPulse = 0;
	Action(worshipper).turnsUntilStateChange = 1;
	EXPECT_EQ(villager::DiscipleNothingToDo(Action(worshipper)), 1u);
	EXPECT_EQ(Top(worshipper), 58u);
	EXPECT_NE(Action(worshipper).turnsUntilStateChange, 300);
}

TEST_F(VillagerDiscipleTest, EnterFacesTheCentre)
{
	// a town with a centre -> facing the centre = LookAtPos(the centre's position, 2); 1
	const auto town = MakeTown();
	const auto centre = MakeCentre(town, {100.0f, 90.0f}, 2.0f);
	const auto e = MakeDisciple(1, {100.0f, 50.0f}, 221);
	Join(e, town);
	const auto twin = MakeVillager({100.0f, 50.0f});
	villager::LookAtPos(twin, tq::PosOf(centre), 2);
	EXPECT_EQ(villager::EnterDiscipleNothingToDo(Action(e), S(0), S(221)), 1u);
	EXPECT_EQ(villager::GetGameAngle(e), villager::GetGameAngle(twin));
	EXPECT_NE(villager::GetGameAngle(e), 0);
	// no centre: no turn; 1
	const auto plain = MakeTown();
	const auto f = MakeDisciple(1, {100.0f, 50.0f}, 221);
	Join(f, plain);
	EXPECT_EQ(villager::EnterDiscipleNothingToDo(Action(f), S(0), S(221)), 1u);
	EXPECT_EQ(villager::GetGameAngle(f), 0);
}
