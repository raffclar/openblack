/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The villagers' reactions 7 REACT_TO_FOOD and 12 REACT_TO_WOOD and the shared reaction slot
// (docs/bw1-notes/villagers.md): the pure layer, the effects::reactions additions (SetReactionDoneWhen, the standard
// turns, the same-type flag), villager::IsAvailableForReaction, the slot's whole life (the score, the records,
// AddReaction, ProcessReaction, the same-type refusal, ShutDown) and the states 19-22. The fixture of
// test_villager_build.cpp (a fake state table in the Locator, the turn and the draws scripted). The reactions are made
// with no map: CreateReaction does not spread them, the test hands them to the villager handler itself.

#define LOCATOR_IMPLEMENTATIONS

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "Common/GameRandomTesting.h"
#include "Common/RandomNumberManager.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/ReactionRecords.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerReaction.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/VillagerChildFactory.h"
#include "ECS/Systems/Implementations/VillagerDiscipleJobs.h"
#include "ECS/Systems/Implementations/VillagerFire.h"
#include "ECS/Systems/Implementations/VillagerReactions.h"
#include "ECS/Systems/Implementations/VillagerResourceReactions.h"
#include "ECS/Systems/Implementations/VillagerRules.h"
#include "ECS/Systems/Implementations/VillagerShield.h"
#include "ECS/Systems/Implementations/VillagerStores.h"
#include "ECS/Systems/Implementations/VillagerTeleport.h"
#include "ECS/Systems/Implementations/VillagerWorldQueries.h"
#include "ECS/Systems/Implementations/VillagerWorshipCheck.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerMourning.h"
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
namespace vr = openblack::ecs::villager_reactions;
namespace vrr = openblack::ecs::villager_resource_reactions;
namespace reactions = openblack::ecs::effects::reactions;
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

class VillagerReactionsTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::EmplaceMapAndVillagerDefaults();
		auto info = std::make_unique<InfoConstants>();
		auto& v = info->villager.at(0);
		v.hungryForFood = 0.5f;
		v.processChecksEvery = 8;
		v.pauseForASecondChance = 0.01f;
		v.grownUpAge = 13;
		v.life = 1.0f;
		v.moveState = LivingStates::LivingMoveToPos;
		v.speedGroup.speedDefault = static_cast<SpeedState>(100);
		v.maxFoodCarried = 150;
		v.maxWoodCarried = 250;
		v.lifeWhenCrawlsWounded = 0.15f;
		v.damageThresholdToGoHome = 0.3f;
		v.maxDistCarryFoodPit = 100.0f;
		reinterpret_cast<uint32_t*>(&v.isReacting)[7] = 1;
		reinterpret_cast<uint32_t*>(&v.isReacting)[12] = 1;
		// ReactionInfo 7 / 12
		for (const auto [t, priority] : {std::pair {7u, 60u}, std::pair {12u, 55u}})
		{
			auto& r = info->reaction.at(t);
			r.priority = priority;
			r.numGameTurnsForNormalThingsToReact = 2000;
			r.numGameTurnsForNormalThingsBeforeReactingAgain = 0;
			r.whetherItImpresses = 1;
			r.whetherReactionFinishesIfInitiatorInHand = 1;
			r.maxReactionDistance = 35.0f;
			r.howImportantIsDistance = 1.0f;
			r.maxDistanceToRunAwayFromObject = 100.0f;
		}
		info->townDesire.at(0).desireTriggersVillagerAction = 0.01f;
		info->townDesire.at(1).desireTriggersVillagerAction = 0.01f;
		for (auto& row : info->villagerStateTable)
		{
			row.field0x0 = 385;
			row.field0x4 = -1;
			row.field0x8 = 1.0f;
			row.field0xf0 = 1;
		}
		auto& table = info->villagerStateTable;
		for (const uint32_t s : {19u, 20u, 21u, 22u, 23u, 31u, 32u, 117u, 163u, 215u})
		{
			table.at(s).isFinalState = 1;
			table.at(s).field0xec = 1; // available for reactions
		}
		for (const uint32_t s : {19u, 20u, 21u, 22u})
		{
			table.at(s).field0xb8 = 1; // reactive
		}
		// info.dat's rows: 19..23 available (field0xa8 = 1) with food /
		// wood interest 0.01 (field0xc8 / field0xcc); 163 available with 0.2 / 0.2
		for (const uint32_t s : {19u, 20u, 21u, 22u, 23u})
		{
			table.at(s).field0xa8 = 1;
			table.at(s).field0xc8 = 0.01f;
			table.at(s).field0xcc = 0.01f;
		}
		table.at(163).field0xa8 = 1;   // GetVillagerAvailableState & 1
		table.at(163).field0x20 = 163; // the resume state
		table.at(163).field0xc8 = 0.2f;
		table.at(163).field0xcc = 0.2f;
		// 215 REACT_TO_FIRE: 4 (not available: bit 0 clear), interest 0.01 / 0.01
		table.at(215).field0xa8 = 4;
		table.at(215).field0xc8 = 0.01f;
		table.at(215).field0xcc = 0.01f;
		table.at(1).field0x14 = 1;
		Locator::infoConstants::reset(info.release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		Locator::rng::emplace<TestRng>();
		Locator::livingActionSystem::emplace<FakeStateTable>();
		reactions::Clear();
		vr::Register();
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
	}

	void TearDown() override
	{
		ClearMiracleMaps();
		reactions::Clear();
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

	static void ClearMiracleMaps()
	{
		ecs::villager_fire::Clear();
		ecs::villager_teleport::Clear();
		ecs::villager_shield::Clear();
		ecs::villager_mourning::Clear();
	}

	static entt::entity MakeVillager(glm::vec2 at = {100.0f, 130.0f}, uint32_t top = 163)
	{
		auto& registry = Reg();
		const auto e = registry.Create();
		auto& v = registry.Assign<Villager>(e);
		v.life = 1.0f;
		v.town = entt::null;
		v.abode = entt::null;
		v.food = 1.0f;
		v.lastCheckTurn = 1000;
		registry.Assign<LivingAction>(e, S(top), static_cast<uint16_t>(0));
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

	/// A log on the ground (a DeadTree, not in the physics)
	static entt::entity MakeLog(glm::vec2 at)
	{
		auto& registry = Reg();
		const auto e = registry.Create();
		registry.Assign<Transform>(e, glm::vec3(at.x, 0.0f, at.y), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<DeadTree>(e, TreeInfo::Pine);
		return e;
	}

	static void Spread(entt::entity villager, uint32_t id, float d)
	{
		const auto* r = reactions::Find(id);
		ASSERT_NE(r, nullptr);
		const auto copy = *r;
		vr::HandleReaction(villager, copy, d);
	}

	static LivingAction& Action(entt::entity e) { return Reg().Get<LivingAction>(e); }
	static Villager& V(entt::entity e) { return Reg().Get<Villager>(e); }
	static uint32_t Top(entt::entity e) { return Action(e).states.at(0); }
	static uint32_t Final(entt::entity e) { return Action(e).states.at(1); }
	static uint32_t Previous(entt::entity e) { return Action(e).states.at(2); }
};
} // namespace

// ---- the pure layer ----------------------------------------------------------------------------------------------

TEST(VillagerReactionsPure, Scores)
{
	// IsInterestedInFoodObject: boost x frac x want x s x d + interact, > 0.25 (strict)
	EXPECT_EQ(vrr::FoodInterestScore(1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f), 1.0f);
	EXPECT_EQ(vrr::FoodInterestScore(2.0f, 0.5f, 0.5f, 0.5f, 1.0f, 0.0f), 0.25f);
	EXPECT_EQ(vrr::FoodInterestScore(1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f), 0.0f); // a full villager
	// the town term (GetDesire / GetRawDesire x frac x s x d + interact) and the drop-off (> 0.1)
	EXPECT_FLOAT_EQ(vrr::TownInterestScore(0.5f, 0.2f, 0.5f, 1.0f, 0.0f), 0.05f);
	EXPECT_FLOAT_EQ(vrr::FoodDropOffScore(1.0f, 0.5f, 0.2f), 0.1f);
	EXPECT_EQ(vrr::CapacityFraction(75, 150), 0.5f);
	EXPECT_LT(vrr::CapacityFraction(-20, 150), 0.0f);
	// 22's falling log: ftol((1000 / ms) x 100) turns
	EXPECT_EQ(vrr::ReactionFallLimit(100), 1000u);
	EXPECT_EQ(vrr::ReactionFallLimit(50), 2000u);
	// 47..50 and 52
	for (const uint32_t s : {47u, 48u, 49u, 50u, 52u})
	{
		EXPECT_TRUE(vrr::IsForesterWorkState(S(s))) << s;
	}
	for (const uint32_t s : {46u, 51u, 53u, 163u})
	{
		EXPECT_FALSE(vrr::IsForesterWorkState(S(s))) << s;
	}
}

// ---- effects::reactions additions --------------------------------------------------------------------------------

TEST_F(VillagerReactionsTest, StandardTurnsAndFlags)
{
	// ftol((1 + 0.5 x (R - d) / R x distImp) x 2000), R = 45
	EXPECT_EQ(reactions::StandardTurnsToReact(7, 0.0f), 3000u);
	EXPECT_EQ(reactions::StandardTurnsToReact(7, 35.0f), 2222u);
	EXPECT_EQ(reactions::StandardTurnsBeforeReactingAgain(12, 10.0f), 0u);
	// the table's same-type switch flag
	for (const uint8_t t : {10, 28, 35})
	{
		EXPECT_TRUE(reactions::SameTypeSwitch(t)) << static_cast<int>(t);
	}
	for (const uint8_t t : {7, 12, 13, 20, 23})
	{
		EXPECT_FALSE(reactions::SameTypeSwitch(t)) << static_cast<int>(t);
	}
}

TEST_F(VillagerReactionsTest, SetReactionDoneWhen)
{
	const auto v = MakeVillager();
	reactions::SetReactionDoneWhen(v, 7, 100);
	EXPECT_EQ(reactions::RecordTurn(v, 7), 100u);
	reactions::SetReactionDoneWhen(v, 7, 200); // refreshed, not added
	EXPECT_EQ(reactions::RecordTurn(v, 7), 200u);
	EXPECT_EQ(Reg().Get<ReactionRecords>(v).count, 1u);
	reactions::SetReactionDoneWhen(v, 12, 300);
	reactions::SetReactionDoneWhen(v, 13, 400);
	// a fourth: the oldest (the head, 7) goes; no 1800-turn pruning
	reactions::SetReactionDoneWhen(v, 20, 5000);
	EXPECT_EQ(reactions::RecordTurn(v, 7), 0u);
	EXPECT_EQ(reactions::RecordTurn(v, 12), 300u);
	EXPECT_EQ(reactions::RecordTurn(v, 20), 5000u);
}

TEST_F(VillagerReactionsTest, IsAvailableForReaction)
{
	const auto v = MakeVillager();
	EXPECT_TRUE(villager::IsAvailableForReaction(v, Reaction::ReactToWood));
	// at or below LifeWhenCrawlsWounded: only REACT_TO_FOOD
	V(v).life = 0.15f;
	EXPECT_FALSE(villager::IsAvailableForReaction(v, Reaction::ReactToWood));
	EXPECT_TRUE(villager::IsAvailableForReaction(v, Reaction::ReactToFood));
	V(v).life = 1.0f;
	// at the worship site, or playing football
	V(v).flags = Villager::k_FlagAtWorshipSite;
	EXPECT_FALSE(villager::IsAvailableForReaction(v, Reaction::ReactToFood));
	V(v).flags = Villager::k_FlagFootball;
	EXPECT_FALSE(villager::IsAvailableForReaction(v, Reaction::ReactToFood));
	V(v).flags = 0;
	// TOP 31: a final state with field0xec 1 takes one; TOP 56, not final: GetFinalState is FINAL 0, whose field0xec
	// is 0
	Action(v).states.at(0) = 31;
	EXPECT_TRUE(villager::IsAvailableForReaction(v, Reaction::ReactToFood));
	Action(v).states.at(0) = 56; // not final: FINAL 0, whose field0xec is 0
	EXPECT_FALSE(villager::IsAvailableForReaction(v, Reaction::ReactToFood));
}

// ---- the slot ----------------------------------------------------------------------------------------------------

TEST_F(VillagerReactionsTest, WoodReactionLife)
{
	const auto town = MakeTown();
	const auto v = MakeVillager();
	V(v).town = town;
	const auto log = MakeLog({105.0f, 130.0f});
	const auto id = reactions::CreateReaction(log, Reaction::ReactToWood, PlayerNames::NEUTRAL, false);
	ASSERT_NE(id, 0u);
	// the dead-tree shortcut: priority 55, score ftol(55 x (1 + 0.5 x 30 / 35)) = 78
	EXPECT_EQ(vrr::ReactToWoodPriority(v, id, 0), 55u);
	EXPECT_EQ(vr::ScoreOf(v, id, Reaction::ReactToWood, 5.0f, 0), 78u);
	EXPECT_EQ(vr::ScoreOf(v, id, Reaction::ReactToWood, 36.0f, 0), 0u); // beyond maxReactionDistance
	// the spread: score > 0, Records(12, 0) new -> MarkStarted, SetupReactToWood: AddReaction(r, 21), the slot
	// object = the log
	Spread(v, id, 5.0f);
	EXPECT_EQ(Top(v), 21u);
	EXPECT_EQ(Previous(v), 163u);
	EXPECT_EQ(vr::SlotReaction(v), id);
	EXPECT_EQ(vr::SlotObject(v), log);
	EXPECT_TRUE(vr::IsReacting(v));
	EXPECT_EQ(reactions::RecordTurn(v, 12), 1000u);
	EXPECT_EQ(reactions::Find(id)->turnCreated, 1000u);
	// another log's reaction 12: the same type never switches (its flag is 0)
	const auto log2 = MakeLog({102.0f, 130.0f});
	const auto id2 = reactions::CreateReaction(log2, Reaction::ReactToWood, PlayerNames::NEUTRAL, false);
	Spread(v, id2, 2.0f);
	EXPECT_EQ(vr::SlotReaction(v), id);
	// 21: the walk to the log's working point with FINAL 22
	EXPECT_EQ(vrr::GotoWoodReaction(Action(v)), 1u);
	EXPECT_EQ(Top(v), 1u);
	EXPECT_EQ(Final(v), 22u);
	const auto wp = ecs::object::GetWorkingPos(log, v);
	EXPECT_EQ(Reg().Get<WallHug>(v).goal, tq::ToMetres({wp.x, wp.z}));
	// ProcessReaction: within the standard turns nothing; past them StopReactingAndSetState (the record gets the turn)
	vr::ProcessSlotReaction(v);
	EXPECT_EQ(vr::SlotReaction(v), id);
	game_clock::SetTurn(1000 + 4000);
	vr::ProcessSlotReaction(v);
	EXPECT_FALSE(vr::SlotHeld(v));
	EXPECT_FALSE(Reg().AllOf<VillagerReactionSlot>(v));
	EXPECT_EQ(reactions::RecordTurn(v, 12), 5000u);
	EXPECT_EQ(Top(v), 163u); // PopFromPrevious -> 163's resume
	// ShutDown: the log's reactions removed -> its follower's StopReactingAndSetState at once
	game_clock::SetTurn(6000);
	const auto id3 = reactions::CreateReaction(log, Reaction::ReactToWood, PlayerNames::NEUTRAL, false);
	Spread(v, id3, 5.0f);
	ASSERT_EQ(vr::SlotReaction(v), id3);
	reactions::RemoveAllReactionsInitiatedByObject(log);
	EXPECT_EQ(reactions::Find(id3), nullptr);
	EXPECT_FALSE(vr::SlotHeld(v));
	// the reaction's initiator deleted: ProcessReaction's first test -> StopReacting (no state change)
	game_clock::SetTurn(7000);
	Spread(v, id2, 2.0f);
	ASSERT_EQ(vr::SlotReaction(v), id2);
	Reg().Destroy(log2);
	vr::ProcessSlotReaction(v);
	EXPECT_FALSE(vr::SlotHeld(v));
	EXPECT_EQ(Top(v), 21u);
}

TEST_F(VillagerReactionsTest, WoodReactionGates)
{
	const auto town = MakeTown();
	const auto v = MakeVillager();
	const auto log = MakeLog({105.0f, 130.0f});
	const auto id = reactions::CreateReaction(log, Reaction::ReactToWood, PlayerNames::NEUTRAL, false);
	// no town -> 0
	EXPECT_EQ(vrr::ReactToWoodPriority(v, id, 0), 0u);
	V(v).town = town;
	EXPECT_EQ(vrr::ReactToWoodPriority(v, id, 0), 55u);
	// capacity 0 -> 0; 125 held is no longer below the capacity: the town term (raw wood desire 0) -> 0
	V(v).resourceHeld.at(1) = 250;
	EXPECT_FALSE(vrr::IsInterestedInWoodObject(v, log));
	V(v).resourceHeld.at(1) = 124;
	EXPECT_TRUE(vrr::IsInterestedInWoodObject(v, log));
	V(v).resourceHeld.at(1) = 125;
	EXPECT_FALSE(vrr::IsInterestedInWoodObject(v, log));
	V(v).resourceHeld.at(1) = 0;
	// at or below DamageThresholdToGoHome (0.3) -> 0
	V(v).life = 0.3f;
	EXPECT_FALSE(vrr::IsInterestedInWoodObject(v, log));
	V(v).life = 1.0f;
	// a villager not available for reactions is skipped by the spread
	Action(v).states.at(0) = 56;
	Spread(v, id, 5.0f);
	EXPECT_FALSE(vr::SlotHeld(v));
	EXPECT_EQ(reactions::RecordTurn(v, 12), 0u);
}

TEST_F(VillagerReactionsTest, MiracleTypeWithoutSlotIsUnchanged)
{
	// a REACT_TO_MAGIC_SHIELD whose initiator is no SpellShield: the shield's own gates refuse it, nothing is held
	const auto v = MakeVillager();
	const auto thing = MakeLog({101.0f, 130.0f});
	const auto id = reactions::CreateReaction(thing, Reaction::ReactToMagicShield, PlayerNames::NEUTRAL, false);
	if (id != 0)
	{
		Spread(v, id, 1.0f);
	}
	EXPECT_FALSE(vr::SlotHeld(v));
	EXPECT_FALSE(vr::IsReacting(v));
	EXPECT_EQ(Top(v), 163u);
	// StopReacting with nothing held changes nothing
	vr::StopReacting(v);
	EXPECT_FALSE(Reg().AllOf<VillagerReactionSlot>(v));
}

// ---- the states --------------------------------------------------------------------------------------------------

TEST_F(VillagerReactionsTest, ArrivesAtFoodReaction)
{
	const auto town = MakeTown();
	const auto v = MakeVillager({100.0f, 130.0f}, 20);
	V(v).town = town;
	// not a PileFood (a plain object): nothing taken; nothing held -> 163
	const auto thing = MakeLog({101.0f, 130.0f});
	const auto id = reactions::CreateReaction(thing, Reaction::ReactToFood, PlayerNames::NEUTRAL, false);
	Reg().Assign<VillagerReactionSlot>(v, VillagerReactionSlot {id, Reaction::ReactToFood, thing, 1});
	EXPECT_EQ(vrr::ArrivesAtFoodReaction(Action(v)), 1u);
	EXPECT_EQ(Top(v), 163u);
	EXPECT_EQ(V(v).resourceHeld.at(0), 0);
	// food held and hungry -> 117 EAT_FOOD
	Reg().AssignOrReplace<VillagerReactionSlot>(v, VillagerReactionSlot {id, Reaction::ReactToFood, thing, 1});
	Action(v).states.at(0) = 20;
	V(v).resourceHeld.at(0) = 60;
	V(v).food = 0.2f;
	EXPECT_EQ(vrr::ArrivesAtFoodReaction(Action(v)), 1u);
	EXPECT_EQ(Top(v), 117u);
	// the object gone: StopReactingAndSetState
	Reg().AssignOrReplace<VillagerReactionSlot>(v, VillagerReactionSlot {id, Reaction::ReactToFood, thing, 1});
	Reg().Destroy(thing);
	Action(v).states.at(0) = 20;
	EXPECT_EQ(vrr::ArrivesAtFoodReaction(Action(v)), 1u);
	EXPECT_FALSE(Reg().AllOf<VillagerReactionSlot>(v));
}

TEST_F(VillagerReactionsTest, WoodStatesObjectGone)
{
	const auto v = MakeVillager({100.0f, 130.0f}, 22);
	const auto log = MakeLog({101.0f, 130.0f});
	const auto id = reactions::CreateReaction(log, Reaction::ReactToWood, PlayerNames::NEUTRAL, false);
	Reg().Assign<VillagerReactionSlot>(v, VillagerReactionSlot {id, Reaction::ReactToWood, log, 1});
	Reg().Destroy(log);
	// 22: SetTopState(163) (ExitReaction stops the reaction then: 163 is not reactive)
	EXPECT_EQ(vrr::ArrivesAtWoodReaction(Action(v)), 1u);
	EXPECT_EQ(Top(v), 163u);
	// 21: StopReactingAndSetState
	Action(v).states.at(0) = 21;
	EXPECT_EQ(vrr::GotoWoodReaction(Action(v)), 1u);
	EXPECT_FALSE(Reg().AllOf<VillagerReactionSlot>(v));
}

// ---- the miracle reactions stay identical ----------------------------------------------------------------------

namespace
{
/// What a reaction left on a villager
struct Outcome
{
	uint32_t top {0};
	uint32_t finalState {0};
	uint32_t previous {0};
	bool reacting {false};
	bool slot {false};
	uint32_t record {0};
};
} // namespace

TEST_F(VillagerReactionsTest, MiracleTypesWithoutSlotGoStraightToTheirCode)
{
	// with no slot reaction held the dispatcher runs the miracle ApplyReaction and nothing else (the only path before
	// the slot): the same reaction through vr::HandleReaction and through the miracle function itself, each in a fresh
	// world, leaves the same states, records and maps (fire, teleport, shield, mourning)
	using Apply = void (*)(entt::entity, const reactions::Reaction&);
	const std::array<std::pair<Reaction, Apply>, 4> types {{
	    {Reaction::ReactToFire, &ecs::villager_fire::ApplyReaction},
	    {Reaction::ReactToTeleport, &ecs::villager_teleport::ApplyReaction},
	    {Reaction::ReactToMagicShield, &ecs::villager_shield::ApplyReaction},
	    {Reaction::ReactToDeath, &ecs::villager_mourning::ApplyReaction},
	}};
	const auto run = [](Reaction type, Apply direct) {
		ClearMiracleMaps();
		reactions::Clear();
		Locator::entitiesRegistry::reset();
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		const auto v = MakeVillager();
		const auto initiator = MakeLog({101.0f, 130.0f});
		const auto id = reactions::CreateReaction(initiator, type, PlayerNames::NEUTRAL, false);
		if (const auto* r = reactions::Find(id); r != nullptr)
		{
			const auto copy = *r;
			if (direct != nullptr)
			{
				direct(v, copy);
			}
			else
			{
				vr::HandleReaction(v, copy, 1.0f);
			}
		}
		return Outcome {Top(v),
		                Final(v),
		                Previous(v),
		                vr::IsReacting(v),
		                Reg().AllOf<VillagerReactionSlot>(v),
		                reactions::RecordTurn(v, static_cast<uint8_t>(type))};
	};
	for (const auto& [type, direct] : types)
	{
		const auto viaDispatcher = run(type, nullptr);
		const auto viaMiracle = run(type, direct);
		const auto t = static_cast<int>(type);
		EXPECT_EQ(viaDispatcher.top, viaMiracle.top) << t;
		EXPECT_EQ(viaDispatcher.finalState, viaMiracle.finalState) << t;
		EXPECT_EQ(viaDispatcher.previous, viaMiracle.previous) << t;
		EXPECT_EQ(viaDispatcher.reacting, viaMiracle.reacting) << t;
		EXPECT_EQ(viaDispatcher.record, viaMiracle.record) << t;
		EXPECT_FALSE(viaDispatcher.slot) << t;
		EXPECT_FALSE(viaMiracle.slot) << t;
	}
}

TEST_F(VillagerReactionsTest, MiracleTypeMeetingSlotHolder)
{
	const auto town = MakeTown();
	const auto v = MakeVillager();
	V(v).town = town;
	const auto log = MakeLog({105.0f, 130.0f});
	const auto wood = reactions::CreateReaction(log, Reaction::ReactToWood, PlayerNames::NEUTRAL, false);
	Spread(v, wood, 5.0f);
	ASSERT_EQ(vr::SlotReaction(v), wood);
	const auto before = Reg().Get<const VillagerReactionSlot>(v);
	const auto thing = MakeLog({101.0f, 130.0f});
	for (const auto type :
	     {Reaction::ReactToFire, Reaction::ReactToTeleport, Reaction::ReactToMagicShield, Reaction::ReactToDeath})
	{
		const auto id = reactions::CreateReaction(thing, type, PlayerNames::NEUTRAL, false);
		if (id == 0)
		{
			continue;
		}
		// a miracle reaction that does not take the villager (its own priority refuses a plain object): the slot is
		// kept as it was and no miracle map holds the villager
		Spread(v, id, 1.0f);
		const auto t = static_cast<int>(type);
		ASSERT_TRUE(Reg().AllOf<VillagerReactionSlot>(v)) << t;
		const auto& after = Reg().Get<const VillagerReactionSlot>(v);
		EXPECT_EQ(after.reaction, before.reaction) << t;
		EXPECT_EQ(after.object, before.object) << t;
		EXPECT_EQ(after.joinOrder, before.joinOrder) << t;
		EXPECT_EQ(Top(v), 21u) << t;
		EXPECT_FALSE(ecs::villager_fire::IsReacting(v)) << t;
		EXPECT_FALSE(ecs::villager_teleport::IsReacting(v)) << t;
		EXPECT_FALSE(ecs::villager_shield::IsReacting(v)) << t;
		EXPECT_FALSE(ecs::villager_mourning::IsReacting(v)) << t;
	}
	// IsAvailableForReaction(type) first, for every type: a wounded slot holder (life at
	// LifeWhenCrawlsWounded) is refused anything but REACT_TO_FOOD before the switch rule is looked at
	V(v).life = 0.15f;
	EXPECT_FALSE(villager::IsAvailableForReaction(v, Reaction::ReactToFire));
	const auto fire = reactions::CreateReaction(thing, Reaction::ReactToFire, PlayerNames::NEUTRAL, false);
	if (fire != 0)
	{
		Spread(v, fire, 1.0f);
	}
	EXPECT_EQ(vr::SlotReaction(v), wood);
	EXPECT_FALSE(ecs::villager_fire::IsReacting(v));
}

TEST_F(VillagerReactionsTest, FoodToWoodSwitchAfterTenSeconds)
{
	const auto town = MakeTown();
	const auto v = MakeVillager({100.0f, 130.0f}, 19);
	V(v).town = town;
	// it follows a food reaction (the slot as SetupReactToFood leaves it) done at turn 905, 10 turns a second
	const auto pile = MakeLog({110.0f, 130.0f});
	const auto food = reactions::CreateReaction(pile, Reaction::ReactToFood, PlayerNames::NEUTRAL, false);
	ASSERT_NE(food, 0u);
	Reg().Assign<VillagerReactionSlot>(v, VillagerReactionSlot {food, Reaction::ReactToFood, pile, 1});
	reactions::SetReactionDoneWhen(v, 7, 905);
	const auto log = MakeLog({105.0f, 130.0f});
	const auto wood = reactions::CreateReaction(log, Reaction::ReactToWood, PlayerNames::NEUTRAL, false);
	ASSERT_NE(wood, 0u);
	// (1000 - 905) / 10 = 9 s < max(10, cur / new x 20 - 10) = 10 -> kept
	Spread(v, wood, 5.0f);
	EXPECT_EQ(vr::SlotReaction(v), food);
	EXPECT_EQ(Top(v), 19u);
	// 10 s -> the switch: SetReactionDoneWhen(12, now) and the wood setup (AddReaction(r, 21))
	game_clock::SetTurn(1005);
	Spread(v, wood, 5.0f);
	EXPECT_EQ(vr::SlotReaction(v), wood);
	EXPECT_EQ(vr::SlotObject(v), log);
	EXPECT_EQ(Top(v), 21u);
	EXPECT_EQ(reactions::RecordTurn(v, 12), 1005u);
	EXPECT_EQ(reactions::RecordTurn(v, 7), 905u); // the switch unlinks with no record refresh
}

TEST_F(VillagerReactionsTest, ShutDownWithTwoFollowers)
{
	const auto town = MakeTown();
	const auto older = MakeVillager({100.0f, 130.0f});
	const auto newer = MakeVillager({101.0f, 130.0f});
	V(older).town = town;
	V(newer).town = town;
	const auto log = MakeLog({105.0f, 130.0f});
	const auto id = reactions::CreateReaction(log, Reaction::ReactToWood, PlayerNames::NEUTRAL, false);
	Spread(older, id, 5.0f);
	Spread(newer, id, 4.0f);
	ASSERT_EQ(vr::SlotReaction(older), id);
	ASSERT_EQ(vr::SlotReaction(newer), id);
	// the head of the followers is the newest (AddReaction links at the head); ShutDown stops the
	// head first, then the next one, until none follows
	EXPECT_GT(Reg().Get<const VillagerReactionSlot>(newer).joinOrder, Reg().Get<const VillagerReactionSlot>(older).joinOrder);
	reactions::RemoveAllReactionsInitiatedByObject(log);
	EXPECT_EQ(reactions::Find(id), nullptr);
	for (const auto v : {older, newer})
	{
		EXPECT_FALSE(vr::SlotHeld(v));
		EXPECT_FALSE(Reg().AllOf<VillagerReactionSlot>(v));
		EXPECT_EQ(Top(v), 163u); // StopReactingAndSetState: PopFromPrevious -> 163's resume
	}
}

TEST_F(VillagerReactionsTest, AddReactionFromAMiracleMap)
{
	const auto town = MakeTown();
	const auto v = MakeVillager();
	V(v).town = town;
	// the villager follows a fire reaction (VillagerFire's map: SetupReactToFire stores 163 and goes to 215)
	const auto thing = MakeLog({101.0f, 130.0f});
	const auto fire = reactions::CreateReaction(thing, Reaction::ReactToFire, PlayerNames::NEUTRAL, false);
	ASSERT_NE(fire, 0u);
	ecs::villager_fire::SetupReactToFire(v, thing, fire);
	ASSERT_TRUE(ecs::villager_fire::IsReacting(v));
	ASSERT_EQ(Top(v), 215u);
	ASSERT_EQ(Previous(v), 163u);
	// a wood reaction above it (the fire scores 0 for an object with no fire, and it has no fire record: past 10 s):
	// the switch, then AddReaction: no StorePreviousState (the previous state is held), SetTopState(21), the fire map
	// dropped, the slot. In 215 the villager is not available (field0xa8 = 4), so the dead-tree shortcut is off and the
	// log is wanted only through the town: raw wood desire 10 x capacity 1 x interest 0.01 x the distance term > the
	// trigger 0.01
	Reg().Get<Town>(town).desire.raw.at(1) = 10.0f;
	const auto log = MakeLog({105.0f, 130.0f});
	const auto wood = reactions::CreateReaction(log, Reaction::ReactToWood, PlayerNames::NEUTRAL, false);
	Spread(v, wood, 5.0f);
	EXPECT_FALSE(ecs::villager_fire::IsReacting(v));
	EXPECT_EQ(vr::SlotReaction(v), wood);
	EXPECT_EQ(vr::SlotObject(v), log);
	EXPECT_EQ(Top(v), 21u);
	EXPECT_EQ(Previous(v), 163u);
	EXPECT_EQ(reactions::RecordTurn(v, 12), 1000u);
}
