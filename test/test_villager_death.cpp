/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A villager's death (docs/bw1-notes/villagers.md, section Death):
// the pure layer (the help table, the dying time, the clips, the dead tick, the soul, the alignment, the mourning
// turns), VillagerDead's guards, order and counters, the states 14 / 15, CannotExitState, the mourning states and the
// orphans, with the fixture of test_villager_food.cpp (a fake state table in the Locator and scripted draws). There is
// no landscape here: MapCoords::IsWater is true everywhere (ecs::sea_cells), so DEAD makes no soul in these tests.

#define LOCATOR_IMPLEMENTATIONS

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/AllMeshes.h"
#include "Common/EventManager.h"
#include "Common/GameRandomTesting.h"
#include "Common/RandomNumberManager.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDeaths.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerLastInteraction.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Events/CreatureMimicEvents.h"
#include "ECS/Events/VillagerDeathEvents.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHeld.h"
#include "ECS/Systems/Implementations/VillagerChildFactory.h"
#include "ECS/Systems/Implementations/VillagerDiscipleJobs.h"
#include "ECS/Systems/Implementations/VillagerReactions.h"
#include "ECS/Systems/Implementations/VillagerRules.h"
#include "ECS/Systems/Implementations/VillagerStores.h"
#include "ECS/Systems/Implementations/VillagerWorldQueries.h"
#include "ECS/Systems/Implementations/VillagerWorshipCheck.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Town/TownVillagers.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerDeath.h"
#include "ECS/Villager/VillagerMourning.h"
#include "ECS/Villager/VillagerResources.h"
#include "ECS/Villager/VillagerSoul.h"
#include "ECS/VillagerDrowning.h"
#include "Enums.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "Resources/ResourcesInterface.h"
#include "support/TestServices.h"
#include "support/VillagerFakes.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace villager = openblack::ecs::villager;
namespace mourning = openblack::ecs::villager_mourning;
namespace soul = openblack::ecs::villager_soul;
using Index = LivingAction::Index;

namespace
{
class TestRng final: public RandomNumberManagerInterface
{
	std::mt19937 _generator {1};
	std::mt19937& Generator() override { return _generator; }
	std::optional<std::reference_wrapper<std::mutex>> LockAccess() override { return std::nullopt; }
};

/// A state table where each row records its calls and accepts every entry and exit
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

class VillagerDeathTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::EmplaceMapAndVillagerDefaults();
		// VillagerDrowning logs to the "game" logger without a null test (as test_water.cpp / test_take_resource.cpp)
		if (spdlog::get("game") == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("game");
		}
		auto info = std::make_unique<InfoConstants>();
		auto& v = info->villager.at(0);
		v.processChecksEvery = 8;
		v.grownUpAge = 13;
		v.sex = SexType::Male;
		v.life = 1.0f;
		v.moveState = LivingStates::LivingMoveToPos;
		v.speedGroup.speedDefault = static_cast<SpeedState>(100);
		v.dyingTimeWithoutGraveyard = 600;
		v.dyingTimeWithGraveyard = 120;
		v.isReacting.isReactingToDeath = 1;
		for (auto& row : info->villagerStateTable)
		{
			row.animation = 385;
			row.field0x4 = -1;
			row.field0x8 = 1.0f;
			row.field0xf0 = 1;
		}
		auto& table = info->villagerStateTable;
		for (const uint32_t s : {13u, 14u, 15u, 36u, 131u, 163u, 205u, 206u, 207u, 208u})
		{
			table.at(s).isFinalState = 1;
		}
		table.at(163).field0xa8 = 1;            // IsVillagerAvailable
		table.at(163).availableForReaction = 1; // IsAvailableForReaction
		// the player info's dealthReason
		info->player.dealthReason = {0.0f, -0.85f, -0.9f, -0.75f, -0.5f, -0.1f, -0.9f, -1.0f, -0.6f, 0.0f};
		info->town.populationUnderWhichHelpSpritesWarn = 5;
		auto& death = info->reaction.at(static_cast<size_t>(Reaction::ReactToDeath));
		death.priority = 100;
		death.maxReactionDistance = 60.0f;
		death.maxDistanceToRunAwayFromObject = 4.0f;
		death.whetherReactionFinishesIfInitiatorInHand = 1;
		death.numGameTurnsForNormalThingsBeforeReactingAgain = 200;
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
		game_random::testing::SetGameRand([this](uint32_t) { return _rand; }, [this](float) { return _floatRand; });
		// A fresh event manager per test: its handlers capture this fixture
		Locator::events::emplace<EventManager>();
		auto& events = Locator::events::value();
		events.AddHandler<ecs::events::VillagerDeathHelp>([this](const ecs::events::VillagerDeathHelp& event) {
			const auto& help = event.help;
			_help.killingPeople |= help.killingPeople;
			_help.deathInVillage |= help.deathInVillage;
			_help.worshippersDying |= help.worshippersDying;
			_help.losingVillagers |= help.losingVillagers;
			_help.lowOnPeople |= help.lowOnPeople;
		});
		events.AddHandler<ecs::events::VillagerDeadEffects>(
		    [this](const ecs::events::VillagerDeadEffects& event) { _effects.push_back(event.effects); });
		test::FakeVillagerStores stores(std::make_shared<ecs::systems::VillagerStores>());
		stores.temporaryStore = [](entt::entity, const map_coords::MapCoords& from, ResourceType) {
			return ecs::town_stores::TemporaryStore {entt::null, from};
		};
		Locator::villagerStores::emplace<test::FakeVillagerStores>(stores);
		ecs::effects::reactions::Clear();
		mourning::Clear();
	}

	void TearDown() override
	{
		mourning::Clear();
		ecs::effects::reactions::Clear();
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

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	static entt::entity MakeVillager(glm::vec2 at = {100.0f, 130.0f}, uint32_t top = 163)
	{
		auto& registry = Reg();
		const auto e = registry.Create();
		ecs::object_index::Assign(e);
		auto& v = registry.Assign<Villager>(e);
		v.life = 1.0f;
		v.town = entt::null;
		v.abode = entt::null;
		v.food = 1.0f;
		v.lastCheckTurn = 1000;
		v.birthTurn = villager::BirthTurnForAge(30, 1000);
		registry.Assign<LivingAction>(e, S(top), static_cast<uint16_t>(0));
		registry.Assign<Transform>(e, glm::vec3(at.x, 0.0f, at.y), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<WallHug>(e, glm::vec2(), glm::vec2(), 0.0f, 0.1f);
		registry.Assign<Mobile>(e);
		registry.Assign<Mesh>(e, static_cast<entt::id_type>(1), static_cast<int8_t>(0), static_cast<int8_t>(0));
		return e;
	}

	static entt::entity MakeTown(PlayerNames owner, uint32_t id = 1)
	{
		auto& registry = Reg();
		const auto town = registry.Create();
		auto& t = registry.Assign<Town>(town, id);
		t.owner = owner;
		registry.Assign<Tribe>(town, Tribe::CELTIC);
		registry.Assign<Transform>(town, glm::vec3(50.0f, 0.0f, 50.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Context().towns[id] = town;
		return town;
	}

	static LivingAction& Action(entt::entity e) { return Reg().Get<LivingAction>(e); }
	static Villager& V(entt::entity e) { return Reg().Get<Villager>(e); }
	static uint32_t Top(entt::entity e) { return Action(e).states.at(0); }
	static uint32_t Final(entt::entity e) { return Action(e).states.at(1); }

	/// LookAtPos until it faces `at` (the mourning states then see it at once)
	static void Face(entt::entity e, entt::entity at)
	{
		for (int i = 0; i < 32 && villager::LookAtPos(e, ecs::town_queries::PosOf(at), 1) == 0; ++i)
		{
		}
	}

	uint32_t _rand {0};
	float _floatRand {0.0f};
	villager::DeathHelp _help;
	std::vector<villager::DeadEffects> _effects;
};
} // namespace

// ---- the pure layer ----------------------------------------------------------------------------------------------

TEST(VillagerDeathPure, HelpTable)
{
	using R = DeathReason;
	// the killer local: table A (SPELL, PLAYER_INTERACTION, DROWN)
	EXPECT_TRUE(villager::HelpFor(R::Spell, true, false, false, false, 0, 5).killingPeople);
	EXPECT_TRUE(villager::HelpFor(R::PlayerInteraction, true, false, false, false, 0, 5).killingPeople);
	EXPECT_TRUE(villager::HelpFor(R::PlayerInteractionDrown, true, false, false, false, 0, 5).killingPeople);
	EXPECT_FALSE(villager::HelpFor(R::Starving, true, true, true, false, 0, 5).killingPeople);
	// ... and then no "death in village", even when the owner is local too
	EXPECT_FALSE(villager::HelpFor(R::Starving, true, true, true, false, 0, 5).deathInVillage);
	// the owner local: table C
	for (const auto r : {R::Starving, R::Spell, R::Animal, R::Chant, R::Sacrifice, R::Exhaustion})
	{
		EXPECT_TRUE(villager::HelpFor(r, false, true, false, false, 0, 5).deathInVillage) << static_cast<int>(r);
	}
	for (const auto r : {R::None, R::PlayerInteraction, R::PlayerInteractionDrown, R::OldAge})
	{
		EXPECT_FALSE(villager::HelpFor(r, false, true, false, false, 0, 5).deathInVillage) << static_cast<int>(r);
	}
	// the town part: CHANT -> worshippers dying (owner local), whatever the adults
	EXPECT_TRUE(villager::HelpFor(R::Chant, false, true, false, true, 50, 5).worshippersDying);
	EXPECT_FALSE(villager::HelpFor(R::Chant, false, true, false, true, 50, 5).losingVillagers);
	EXPECT_FALSE(villager::HelpFor(R::Chant, false, false, false, true, 2, 5).lowOnPeople);
	// adults above the threshold: B, the killer not the owner, the owner local
	EXPECT_TRUE(villager::HelpFor(R::Animal, false, true, false, true, 6, 5).losingVillagers);
	EXPECT_FALSE(villager::HelpFor(R::Animal, false, true, true, true, 6, 5).losingVillagers);
	EXPECT_FALSE(villager::HelpFor(R::Starving, false, true, false, true, 6, 5).losingVillagers);
	// at or below it: low on people without the B test
	EXPECT_TRUE(villager::HelpFor(R::Starving, false, true, false, true, 5, 5).lowOnPeople);
	EXPECT_FALSE(villager::HelpFor(R::Starving, false, false, false, true, 5, 5).lowOnPeople);
	// no town: nothing of the town part
	const auto none = villager::HelpFor(R::Starving, false, true, false, false, 0, 5);
	EXPECT_FALSE(none.lowOnPeople || none.losingVillagers || none.worshippersDying);
}

TEST(VillagerDeathPure, DyingTimeAndClips)
{
	GVillagerInfo info {};
	info.dyingTimeWithoutGraveyard = 600;
	info.dyingTimeWithGraveyard = 120;
	EXPECT_EQ(villager::DyingTime(false, info), 600u);
	EXPECT_EQ(villager::DyingTime(true, info), 120u);
	EXPECT_EQ(villager::DyingClip(true, 2), 283);
	EXPECT_EQ(villager::DyingClip(false, 2), 246);
	for (const uint8_t lt : {0, 1, 3})
	{
		EXPECT_EQ(villager::DyingClip(false, lt), 253);
		EXPECT_EQ(villager::DeadClip(false, lt), 243);
	}
	EXPECT_EQ(villager::DeadClip(true, 0), 249);
	EXPECT_EQ(villager::DeadClip(false, 2), 246);
}

TEST(VillagerDeathPure, DeadTick)
{
	// 2 -> 1 -> 0 -> vanish at the third call (the counter wraps)
	auto t = ecs::living::DeadTick(2, false, DeathReason::OldAge);
	EXPECT_EQ(t.counter, 1u);
	EXPECT_FALSE(t.vanish);
	t = ecs::living::DeadTick(t.counter, false, DeathReason::OldAge);
	EXPECT_EQ(t.counter, 0u);
	EXPECT_FALSE(t.vanish);
	t = ecs::living::DeadTick(t.counter, false, DeathReason::OldAge);
	EXPECT_TRUE(t.vanish);
	// controlled by a script: the counter stays, no vanish; SACRIFICE vanishes at once either way
	t = ecs::living::DeadTick(0, true, DeathReason::OldAge);
	EXPECT_EQ(t.counter, 0u);
	EXPECT_FALSE(t.vanish);
	EXPECT_TRUE(ecs::living::DeadTick(50, true, DeathReason::Sacrifice).vanish);
	EXPECT_TRUE(ecs::living::DeadTick(50, false, DeathReason::Sacrifice).vanish);
}

TEST(VillagerDeathPure, Soul)
{
	EXPECT_EQ(soul::SoulClip(false, false, 49.9f), 244);
	EXPECT_EQ(soul::SoulClip(false, false, 50.0f), 245);
	EXPECT_EQ(soul::SoulClip(true, false, 10.0f), 247);
	EXPECT_EQ(soul::SoulClip(true, false, 90.0f), 248);
	EXPECT_EQ(soul::SoulClip(false, true, 90.0f), 244);
	EXPECT_EQ(soul::SoulClip(true, true, 90.0f), 247);
	EXPECT_EQ(soul::SoulAlpha(0, 2000), 105);
	EXPECT_EQ(soul::SoulAlpha(1500, 2000), 105);
	// 250 ms into the last 500: (1 - 250 x 0.002) x 105 = 52.5 -> 52
	EXPECT_EQ(soul::SoulAlpha(1750, 2000), 52);
	// 100 ms in: (1 - 0.2f) x 105 in float steps (24-bit x87) = 84 (83.99999 in double would give 83)
	EXPECT_EQ(soul::SoulAlpha(1600, 2000), 84);
	EXPECT_FALSE(soul::SoulExpired(1890, 2000));
	EXPECT_TRUE(soul::SoulExpired(1891, 2000));
}

TEST(VillagerDeathPure, AlignmentAndMourningTurns)
{
	GPlayerInfo info {};
	info.dealthReason = {0.0f, -0.85f, -0.9f, -0.75f, -0.5f, -0.1f, -0.9f, -1.0f, -0.6f, 0.0f};
	EXPECT_FLOAT_EQ(ecs::effects::alignment::DeathAlignmentChange(info, DeathReason::Starving, false, false), -0.85f);
	EXPECT_FLOAT_EQ(ecs::effects::alignment::DeathAlignmentChange(info, DeathReason::Starving, true, false), -1.7f);
	EXPECT_FLOAT_EQ(ecs::effects::alignment::DeathAlignmentChange(info, DeathReason::Starving, false, true), -0.425f);
	EXPECT_FLOAT_EQ(ecs::effects::alignment::DeathAlignmentChange(info, DeathReason::OldAge, true, false), 0.0f);
	EXPECT_EQ(mourning::PointTurns(0.0f), 20);
	EXPECT_EQ(mourning::PointTurns(19.99f), 219);
	EXPECT_EQ(mourning::MournTurns(0.0f), 40);
	EXPECT_EQ(mourning::MournTurns(2.95f), 69);
}

// ---- VillagerDead ------------------------------------------------------------------------------------------------

TEST_F(VillagerDeathTest, Guards)
{
	// already dead: status 1, or TOP 15
	auto a = MakeVillager();
	V(a).status = Villager::k_StatusDead;
	villager::VillagerDead(a, DeathReason::Starving, std::nullopt, 0.0f, 1);
	EXPECT_EQ(Top(a), 163u);
	EXPECT_EQ(V(a).deathReason, DeathReason::None);
	auto b = MakeVillager({100.0f, 130.0f}, 15);
	villager::VillagerDead(b, DeathReason::Starving, std::nullopt, 0.0f, 1);
	EXPECT_EQ(V(b).status, 0u);
	EXPECT_EQ(V(b).deathReason, DeathReason::None);
}

TEST_F(VillagerDeathTest, OrderAndFields)
{
	auto a = MakeVillager();
	villager::VillagerDead(a, DeathReason::Starving, std::nullopt, 0.5f, 1);
	EXPECT_EQ(V(a).status, static_cast<uint16_t>(Villager::k_StatusDead | Villager::k_StatusLandTypeMask));
	EXPECT_FLOAT_EQ(V(a).life, 0.0f);
	EXPECT_EQ(Top(a), 14u);
	EXPECT_EQ(V(a).deathReason, DeathReason::Starving);
	EXPECT_EQ(Action(a).turnsUntilStateChange, 600u);
	EXPECT_TRUE(villager::IsDead(a));
	EXPECT_TRUE(villager::IsCountedOut(a));
	// with a functional graveyard in its town: 120
	const auto town = MakeTown(PlayerNames::NEUTRAL);
	test::FakeVillagerWorldQueries world(std::make_shared<ecs::systems::VillagerWorldQueries>());
	world.graveyard = [town](entt::entity t) { return t == town ? std::optional<bool>(true) : std::nullopt; };
	Locator::villagerWorldQueries::emplace<test::FakeVillagerWorldQueries>(world);
	auto b = MakeVillager();
	V(b).town = town;
	villager::VillagerDead(b, DeathReason::OldAge, std::nullopt, 1.0f, 1);
	EXPECT_EQ(Action(b).turnsUntilStateChange, 120u);
	// SACRIFICE: the counter 0
	auto c = MakeVillager();
	villager::VillagerDead(c, DeathReason::Sacrifice, PlayerNames::PLAYER_ONE, 1.0f, 1);
	EXPECT_EQ(Action(c).turnsUntilStateChange, 0u);
	EXPECT_EQ(villager::GetDeathReason(c), DeathReason::Sacrifice);
}

TEST_F(VillagerDeathTest, OwnerKillerAndCounters)
{
	const auto town = MakeTown(PlayerNames::PLAYER_TWO);
	Reg().Get<Town>(town).buildPulsePrevious = 1;
	auto a = MakeVillager();
	V(a).town = town;
	villager::VillagerDead(a, DeathReason::Starving, std::nullopt, 0.0f, 1);
	// the owner's alignment, the town's counters (killer none -> the neutral player's slot), the pulse
	EXPECT_FLOAT_EQ(ecs::effects::alignment::Of(PlayerNames::PLAYER_TWO).pending, -0.85f);
	const auto& deaths = Reg().Get<TownDeaths>(town);
	EXPECT_EQ(deaths.byReason.at(1), 1u);
	EXPECT_EQ(deaths.byPlayer.at(static_cast<size_t>(PlayerNames::NEUTRAL)), 1u);
	EXPECT_EQ(deaths.count, 1u);
	EXPECT_EQ(deaths.total38, 1u);
	EXPECT_EQ(deaths.total5C.at(0), 1u);
	EXPECT_EQ(deaths.lastDeathTurn, 1000u);
	EXPECT_EQ(Reg().Get<Town>(town).buildPulse, 1u);
	EXPECT_EQ(Reg().Get<Town>(town).buildPulsePrevious, 0u);
	// out of its town
	EXPECT_TRUE(V(a).town == entt::null);
	EXPECT_FALSE(villager::GetPlayerOf(a).has_value());
	// a child of the town killed by player one: twice the change, its slot
	auto b = MakeVillager();
	V(b).town = town;
	V(b).flags = Villager::k_FlagChild;
	villager::VillagerDead(b, DeathReason::Starving, PlayerNames::PLAYER_ONE, 0.0f, 1);
	EXPECT_FLOAT_EQ(ecs::effects::alignment::Of(PlayerNames::PLAYER_TWO).pending, -0.85f - 1.7f);
	EXPECT_EQ(Reg().Get<TownDeaths>(town).byPlayer.at(0), 1u);
	// no town: no alignment change, no counters
	auto c = MakeVillager();
	villager::VillagerDead(c, DeathReason::Starving, std::nullopt, 0.0f, 1);
	EXPECT_FLOAT_EQ(ecs::effects::alignment::Of(PlayerNames::NEUTRAL).pending, 0.0f);
}

TEST_F(VillagerDeathTest, ReportsToGuidance)
{
	// a villager of the local player's small town killed by the local player with the hand
	const auto town = MakeTown(PlayerNames::PLAYER_ONE);
	auto a = MakeVillager();
	V(a).town = town;
	villager::VillagerDead(a, DeathReason::PlayerInteraction, PlayerNames::PLAYER_ONE, 0.0f, 1);
	EXPECT_TRUE(_help.killingPeople);
	EXPECT_FALSE(_help.deathInVillage);
	EXPECT_TRUE(_help.lowOnPeople);
	// a worshipper's death
	_help = {};
	auto b = MakeVillager();
	V(b).town = town;
	villager::VillagerDead(b, DeathReason::Chant, PlayerNames::PLAYER_ONE, 0.0f, 1);
	EXPECT_TRUE(_help.worshippersDying);
	EXPECT_FALSE(_help.lowOnPeople);
}

TEST_F(VillagerDeathTest, WorldPopulation)
{
	auto a = MakeVillager();
	auto b = MakeVillager();
	EXPECT_EQ(magic::players::WorldPopulation(), 2u);
	villager::VillagerDead(a, DeathReason::OldAge, std::nullopt, 1.0f, 1);
	EXPECT_EQ(magic::players::WorldPopulation(), 1u);
	// SetDying again (row 13) does not count it out twice
	villager::SetDying(a);
	EXPECT_EQ(magic::players::WorldPopulation(), 1u);
	// Delete of a live one: gone (the villager's removal takes off one not counted out); no death reason
	villager::Delete(b);
	EXPECT_FALSE(Reg().Valid(b));
	EXPECT_EQ(magic::players::WorldPopulation(), 0u);
	// Delete of the counted-out corpse: no change
	villager::Delete(a);
	EXPECT_EQ(magic::players::WorldPopulation(), 0u);
}

// ---- the states --------------------------------------------------------------------------------------------------

TEST_F(VillagerDeathTest, Dying)
{
	// reason 7: straight to 15
	auto a = MakeVillager({100.0f, 130.0f}, 14);
	V(a).deathReason = DeathReason::Sacrifice;
	villager::Dying(Action(a));
	EXPECT_EQ(Top(a), 15u);
	// else 23 WAIT_FOR_ANIMATION with FINAL 15, and one REACT_TO_DEATH
	auto b = MakeVillager({100.0f, 130.0f}, 14);
	V(b).deathReason = DeathReason::OldAge;
	villager::Dying(Action(b));
	EXPECT_EQ(Top(b), 23u);
	EXPECT_EQ(Final(b), 15u);
	EXPECT_NE(ecs::effects::reactions::GetReactionOfTypeInitiatedBy(b, Reaction::ReactToDeath), 0u);
	// at home: no reaction
	auto c = MakeVillager({100.0f, 130.0f}, 14);
	V(c).flags = Villager::k_FlagAtHome;
	villager::Dying(Action(c));
	EXPECT_EQ(ecs::effects::reactions::GetReactionOfTypeInitiatedBy(c, Reaction::ReactToDeath), 0u);
}

TEST_F(VillagerDeathTest, Dead)
{
	const auto skeleton = resources::HashIdentifier(MeshId::PersonSkeletonMale);
	// the first DEAD turn: smoke and the skeleton (in the water here: no soul)
	auto a = MakeVillager({100.0f, 130.0f}, 15);
	Action(a).turnsUntilStateChange = 2;
	EXPECT_EQ(villager::Dead(Action(a)), 1u);
	ASSERT_EQ(_effects.size(), 1u);
	EXPECT_TRUE(_effects.at(0).smoke);
	EXPECT_TRUE(_effects.at(0).skeleton);
	EXPECT_FALSE(_effects.at(0).soul);
	EXPECT_EQ(Reg().Get<Mesh>(a).id, skeleton);
	EXPECT_EQ(Action(a).turnsUntilStateChange, 1u);
	// the next turns: nothing more until the vanish (the third call), then a puff and it is gone
	EXPECT_EQ(villager::Dead(Action(a)), 1u);
	EXPECT_EQ(_effects.size(), 1u);
	EXPECT_EQ(villager::Dead(Action(a)), 5u);
	EXPECT_EQ(_effects.size(), 2u);
	EXPECT_FALSE(Reg().Valid(a));
	// controlled by a script: no smoke, no skeleton, the counter stays
	auto b = MakeVillager({100.0f, 130.0f}, 15);
	ecs::script_held::SetControlledByScript(b, true);
	Action(b).turnsUntilStateChange = 0;
	EXPECT_EQ(villager::Dead(Action(b)), 1u);
	EXPECT_EQ(_effects.size(), 2u);
	EXPECT_NE(Reg().Get<Mesh>(b).id, skeleton);
	EXPECT_EQ(Action(b).turnsUntilStateChange, 0u);
	// ... except SACRIFICE, which vanishes at once
	V(b).deathReason = DeathReason::Sacrifice;
	EXPECT_EQ(villager::Dead(Action(b)), 5u);
}

TEST_F(VillagerDeathTest, CannotExitState)
{
	auto a = MakeVillager({100.0f, 130.0f}, 15);
	EXPECT_EQ(villager::CannotExitState(Action(a), S(24)), 1u);
	EXPECT_EQ(villager::CannotExitState(Action(a), S(10)), 1u);
	EXPECT_EQ(villager::CannotExitState(Action(a), S(15)), 1u);
	EXPECT_EQ(villager::CannotExitState(Action(a), S(163)), 0u);
	EXPECT_EQ(villager::CannotExitState(Action(a), S(13)), 0u);
	EXPECT_EQ(villager::CannotExitState(Action(a), S(14)), 0u);
}

TEST_F(VillagerDeathTest, DeleteDependants)
{
	// alive: SET_DYING through the exits, out of the vagrants
	auto a = MakeVillager({100.0f, 130.0f}, 163);
	auto& table = static_cast<FakeStateTable&>(Locator::livingActionSystem::value());
	table.calls.clear();
	villager::DeleteDependants(a);
	EXPECT_EQ(Top(a), 13u);
	EXPECT_NE(std::find(table.calls.begin(), table.calls.end(), "exit 163 13"), table.calls.end());
	// in a town (no abode): out of it
	const auto town = MakeTown(PlayerNames::NEUTRAL);
	auto b = MakeVillager();
	V(b).town = town;
	villager::DeleteDependants(b);
	EXPECT_TRUE(V(b).town == entt::null);
}

// ---- the mourning ------------------------------------------------------------------------------------------------

TEST_F(VillagerDeathTest, Orphans)
{
	const auto town = MakeTown(PlayerNames::NEUTRAL);
	auto mother = MakeVillager();
	V(mother).town = town;
	auto child = MakeVillager();
	V(child).town = town;
	V(child).mother = mother;
	ecs::town_villagers::AddToHomelessList(town, child);
	mourning::FindChildrenAndOrphanThem(mother);
	EXPECT_EQ(Top(child), 131u);
	EXPECT_TRUE(V(child).mother == entt::null);
	// not her child: nothing
	auto other = MakeVillager();
	EXPECT_EQ(mourning::MakeChildOrphaned(other, mother), 0u);
	EXPECT_EQ(Top(other), 163u);
}

TEST_F(VillagerDeathTest, MourningPriorityAndSetup)
{
	auto dead = MakeVillager({100.0f, 130.0f}, 15);
	const auto reaction = ecs::effects::reactions::CreateReaction(dead, Reaction::ReactToDeath, PlayerNames::NEUTRAL, false);
	ASSERT_NE(reaction, 0u);
	auto a = MakeVillager({103.0f, 130.0f});
	EXPECT_EQ(mourning::ReactToDeathPriority(a, reaction), 100u);
	// the dead one itself: 0
	EXPECT_EQ(mourning::ReactToDeathPriority(dead, reaction), 0u);
	// a villager of a town with a functional graveyard: 0
	const auto town = MakeTown(PlayerNames::NEUTRAL);
	test::FakeVillagerWorldQueries world(std::make_shared<ecs::systems::VillagerWorldQueries>());
	world.graveyard = [town](entt::entity t) { return t == town ? std::optional<bool>(true) : std::nullopt; };
	Locator::villagerWorldQueries::emplace<test::FakeVillagerWorldQueries>(world);
	auto g = MakeVillager();
	V(g).town = town;
	EXPECT_EQ(mourning::ReactToDeathPriority(g, reaction), 0u);
	// roll 0 -> 205 with the counter 0; roll 1 -> 206
	_rand = 0;
	Action(a).turnsUntilStateChange = 77;
	mourning::SetupReactToDeath(a, dead, reaction);
	EXPECT_EQ(Top(a), 205u);
	EXPECT_EQ(Action(a).turnsUntilStateChange, 0u);
	EXPECT_TRUE(mourning::IsReacting(a));
	EXPECT_EQ(mourning::ReactionObject(a), dead);
	_rand = 1;
	auto b = MakeVillager({97.0f, 130.0f});
	mourning::SetupReactToDeath(b, dead, reaction);
	EXPECT_EQ(Top(b), 206u);
	// ten took it: the eleventh gets 0
	for (int i = 0; i < 8; ++i)
	{
		mourning::SetupReactToDeath(MakeVillager({100.0f, 133.0f}), dead, reaction);
	}
	EXPECT_EQ(mourning::ReactToDeathPriority(MakeVillager({100.0f, 127.0f}), reaction), 0u);
}

TEST_F(VillagerDeathTest, MourningStates)
{
	auto dead = MakeVillager({100.0f, 130.0f}, 15);
	const auto reaction = ecs::effects::reactions::CreateReaction(dead, Reaction::ReactToDeath, PlayerNames::NEUTRAL, false);
	auto a = MakeVillager({103.0f, 130.0f});
	_rand = 0;
	mourning::SetupReactToDeath(a, dead, reaction);
	Face(a, dead);
	// 205: GameFloatRand(20) = 0 -> 20 facing turns, the 21st goes to 206
	_floatRand = 0.0f;
	for (int i = 0; i < 20; ++i)
	{
		mourning::PointAtDeadPerson(Action(a));
	}
	EXPECT_EQ(Top(a), 205u);
	mourning::PointAtDeadPerson(Action(a));
	EXPECT_EQ(Top(a), 206u);
	// 206 within 1.2 R (3 m < 4.8 m): 207
	mourning::GoTowardsDeadPerson(Action(a));
	EXPECT_EQ(Top(a), 207u);
	// 207 facing: 208, counter 0
	Action(a).turnsUntilStateChange = 9;
	mourning::LookAtDeadPerson(Action(a));
	EXPECT_EQ(Top(a), 208u);
	EXPECT_EQ(Action(a).turnsUntilStateChange, 0u);
	// 208: GameFloatRand(3) = 0 -> 40 turns, then StopReactingAndSetState
	for (int i = 0; i < 40; ++i)
	{
		mourning::MournDeadPerson(Action(a));
	}
	EXPECT_TRUE(mourning::IsReacting(a));
	mourning::MournDeadPerson(Action(a));
	EXPECT_FALSE(mourning::IsReacting(a));
	// 206 far away: a walk of the whole metres of d - R towards the dead one (30.5 - 4 -> 26 m)
	auto b = MakeVillager({130.5f, 130.0f});
	_rand = 1;
	mourning::SetupReactToDeath(b, dead, reaction);
	mourning::GoTowardsDeadPerson(Action(b));
	EXPECT_EQ(Top(b), 1u); // MOVE_TO_POS
	EXPECT_EQ(Final(b), 206u);
	EXPECT_NEAR(Reg().Get<WallHug>(b).goal.x, 104.5f, 0.1f);
	// the dead one gone: the validate pops
	villager::Delete(dead);
	EXPECT_FALSE(mourning::ReactionValidate(Action(b)));
}

// ---- the takers of a REACT_TO_DEATH, the deletion, the water --------------------------------------------------------

TEST_F(VillagerDeathTest, MourningTakersCountTheCurrentOnes)
{
	auto dead = MakeVillager({100.0f, 130.0f}, 15);
	const auto reaction = ecs::effects::reactions::CreateReaction(dead, Reaction::ReactToDeath, PlayerNames::NEUTRAL, false);
	ASSERT_NE(reaction, 0u);
	std::vector<entt::entity> mourners;
	for (int i = 0; i < 10; ++i)
	{
		mourners.push_back(MakeVillager({100.0f, 133.0f}));
		mourning::SetupReactToDeath(mourners.back(), dead, reaction);
	}
	// villager_reactions::IsReacting knows the mourning (a villager has one reaction slot)
	EXPECT_TRUE(ecs::villager_reactions::IsReacting(mourners.front()));
	const auto next = MakeVillager({100.0f, 127.0f});
	EXPECT_EQ(mourning::ReactToDeathPriority(next, reaction), 0u);
	// StopReacting takes one off the reaction's takers: one stops (through villager_reactions), the next one may take
	// it
	ecs::villager_reactions::StopReacting(mourners.front());
	EXPECT_FALSE(mourning::IsReacting(mourners.front()));
	EXPECT_FALSE(ecs::villager_reactions::IsReacting(mourners.front()));
	EXPECT_EQ(mourning::ReactToDeathPriority(next, reaction), 100u);
	// a second stop of the same villager changes nothing
	ecs::villager_reactions::StopReacting(mourners.front());
	mourning::SetupReactToDeath(next, dead, reaction);
	EXPECT_EQ(mourning::ReactToDeathPriority(MakeVillager({100.0f, 127.0f}), reaction), 0u);
	// villager::Delete stops the reaction (ecs::ToBeDeleted's villager branch)
	villager::Delete(mourners.at(1));
	EXPECT_FALSE(mourning::IsReacting(mourners.at(1)));
	EXPECT_EQ(mourning::ReactToDeathPriority(MakeVillager({100.0f, 127.0f}), reaction), 100u);
}

TEST_F(VillagerDeathTest, DeleteDependantsOfAWoman)
{
	// a mother (sex 1 in her info): her children (mother == her) go to 131 and lose her
	const_cast<GVillagerInfo&>(Locator::infoConstants::value().villager.at(0)).sex = SexType::Female;
	const auto town = MakeTown(PlayerNames::NEUTRAL);
	auto mother = MakeVillager();
	V(mother).town = town;
	auto child = MakeVillager();
	V(child).town = town;
	V(child).mother = mother;
	ecs::town_villagers::AddToHomelessList(town, child);
	villager::DeleteDependants(mother);
	EXPECT_EQ(Top(mother), 13u);
	EXPECT_TRUE(V(mother).town == entt::null);
	EXPECT_EQ(Top(child), 131u);
	EXPECT_TRUE(V(child).mother == entt::null);
}

TEST_F(VillagerDeathTest, HasSunk)
{
	// HasSunk: the status bit -> SetTopState(14) and dyingTimeWithoutGraveyard
	auto a = MakeVillager({100.0f, 130.0f}, 15);
	V(a).status = Villager::k_StatusDead;
	EXPECT_TRUE(ecs::HasSunk(a));
	EXPECT_EQ(Top(a), 14u);
	EXPECT_EQ(Action(a).turnsUntilStateChange, 600u);
	// TOP 15 without the bit (villager::IsDead would say dead): not the corpse branch
	auto b = MakeVillager({100.0f, 130.0f}, 15);
	EXPECT_TRUE(ecs::HasSunk(b));
	EXPECT_NE(Top(b), 14u);
	// not available (the final state 14 DYING): not sunk, nothing changes
	auto c = MakeVillager({100.0f, 130.0f}, 14);
	V(c).status = Villager::k_StatusDead;
	Action(c).turnsUntilStateChange = 7;
	EXPECT_FALSE(ecs::HasSunk(c));
	EXPECT_EQ(Top(c), 14u);
	EXPECT_EQ(Action(c).turnsUntilStateChange, 7u);
}

TEST_F(VillagerDeathTest, HasSunkTellsTheCreatureOfWhoThrewIt)
{
	// a villager that can still be reached reports the player whose hand last dropped it, before the rest of its sinking;
	// one that cannot, or that no hand dropped, reports nothing
	std::vector<ecs::events::PlayerDeedForMimic> deeds;
	Locator::events::value().AddHandler<ecs::events::PlayerDeedForMimic>(
	    [&deeds](const ecs::events::PlayerDeedForMimic& event) { deeds.push_back(event); });
	auto thrown = MakeVillager({100.0f, 130.0f}, 15);
	Reg().AssignState<VillagerLastInteraction>(thrown, PlayerNames::PLAYER_ONE);
	EXPECT_TRUE(ecs::HasSunk(thrown));
	ASSERT_EQ(deeds.size(), 1u);
	EXPECT_EQ(deeds.front().player, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(deeds.front().deed, creature_watching::Deed::ThrowInTheSea);
	EXPECT_EQ(deeds.front().object, thrown);
	EXPECT_FALSE(deeds.front().magic.has_value());
	deeds.clear();
	auto unreachable = MakeVillager({100.0f, 130.0f}, 14);
	V(unreachable).status = Villager::k_StatusDead;
	Reg().AssignState<VillagerLastInteraction>(unreachable, PlayerNames::PLAYER_ONE);
	EXPECT_FALSE(ecs::HasSunk(unreachable));
	auto walked = MakeVillager({100.0f, 130.0f}, 15);
	EXPECT_TRUE(ecs::HasSunk(walked));
	EXPECT_TRUE(deeds.empty());
}

TEST_F(VillagerDeathTest, EndPhysicsInWater)
{
	// the end of the physics in the water at 0 life: dead -> SetTopState(14), the counter 600
	auto a = MakeVillager({100.0f, 130.0f}, 15);
	V(a).status = Villager::k_StatusDead;
	V(a).life = 0.0f;
	ecs::VillagerEndPhysicsInWater(a);
	EXPECT_EQ(Top(a), 14u);
	EXPECT_EQ(Action(a).turnsUntilStateChange, 600u);
	EXPECT_EQ(V(a).deathReason, DeathReason::None);
	// not dead: VillagerDead(6 PLAYER_INTERACTION_DROWN, ..., 0.01, 1)
	auto b = MakeVillager();
	V(b).life = 0.0f;
	ecs::VillagerEndPhysicsInWater(b);
	EXPECT_EQ(V(b).deathReason, DeathReason::PlayerInteractionDrown);
	EXPECT_EQ(Top(b), 14u);
	EXPECT_TRUE(villager::IsDead(b));
}
