/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstring>

#include <algorithm>
#include <memory>
#include <vector>

#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "Common/GameRandom.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/Fire.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/LivingReaction.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/MiracleImpression.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Dances.h"
#include "ECS/PhysicsEntry.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DanceSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/ReactionRules.h"
#include "Magic/ShieldRules.h"
#include "Magic/VillagerReactionRules.h"

#define LOCATOR_IMPLEMENTATIONS
#include "ECS/MapProduction.h"
#include "ECS/Systems/Implementations/FireSystem.h"
#include "ECS/Systems/Implementations/LivingActionSystem.h"
#include "ECS/Systems/Implementations/ReactionSystem.h"
#include "ECS/Systems/Implementations/VillagerFire.h"
#include "ECS/Systems/Implementations/VillagerHome.h"
#include "ECS/Systems/Implementations/VillagerReactions.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
/// Every kind of living thing reacts to every kind of reaction
void ReactToEverything(GLivingInfo& info)
{
	std::memset(&info.isReacting, 1, sizeof(info.isReacting));
}

/// Where the miracles are cast, well inside the land's map
const glm::vec3 k_Here(500.0f, 0.0f, 500.0f);

/// One thing flying, its body a small cube, at a speed
class FlyingThing final: public ecs::systems::DynamicsSystemInterface
{
public:
	FlyingThing(entt::entity object, glm::vec3 velocity)
	{
		entry.entity = object;
		physics::Shape cube;
		for (int i = 0; i < 8; ++i)
		{
			cube.points.emplace_back((i & 1) != 0 ? 0.5f : -0.5f, (i & 2) != 0 ? 0.5f : -0.5f, (i & 4) != 0 ? 0.5f : -0.5f);
		}
		cube.faces = {{0, 1, 3}};
		cube.radius = glm::length(glm::vec3(0.5f));
		entry.body =
		    std::make_unique<physics::Body>(physics::BodySetup {.scale = 1.0f, .halfHeight = 0.5f, .mass = 1.0f}, cube);
		entry.body->velocity = velocity;
	}
	[[nodiscard]] PhysicsEntry* Find(entt::entity object) override { return object == entry.entity ? &entry : nullptr; }

	PhysicsEntry entry;
};
/// Dances of one shared group that takes anyone, made without files; remembers the dances it was asked to end
class OneGroupDances final: public ecs::systems::DanceSystemInterface
{
public:
	entt::entity Create(DanceInfo type, const glm::vec3&, entt::entity owner, uint32_t, bool) override
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto dance = registry.Create();
		auto& data = registry.Assign<Dance>(dance);
		data.type = type;
		data.owner = owner;
		data.groups.all.emplace_back();
		data.groups.shared.push_back(0);
		data.groups.roundLength = 100;
		made.push_back(type);
		return dance;
	}
	void Destroy(entt::entity dance) override
	{
		ended.push_back(dance);
		Locator::entitiesRegistry::value().Destroy(dance);
	}
	void ProcessTurn() override {}

	std::vector<DanceInfo> made;
	std::vector<entt::entity> ended;
};
/// Every draw of the game's random numbers is as low as it can be
class AlwaysLowGameRandom final: public GameRandomInterface
{
public:
	uint32_t GameRand(uint32_t) override { return 0; }
	float GameFloatRand(float) override { return 0.0f; }
	uint32_t LocalRand(int32_t) override { return 0; }
	float LocalFloatRand(float) override { return 0.0f; }
	int32_t CrtRand() override { return 0; }
	void CrtSrand(uint32_t) override {}
	[[nodiscard]] GameRandomSeeds GetSeeds() const override { return {}; }
	void SetSeeds(GameRandomSeeds) override {}
	[[nodiscard]] ParticleRandomStream GetParticleStream() const override { return ParticleRandomStream::None; }
	void SetParticleStream(ParticleRandomStream) override {}
};
} // namespace

/// Villagers of a town and a creature about a miracle, with made-up reaction tables
class Reactions: public ::testing::Test
{
protected:
	void SetUp() override
	{
		for (const auto* name : {"ai", "game", "scripting"})
		{
			if (spdlog::get(name) == nullptr)
			{
				spdlog::create<spdlog::sinks::null_sink_st>(name);
			}
		}
		Locator::entitiesRegistry::emplace<Registry>();
		Locator::entitiesMap::emplace<MapProduction>();
		Locator::livingActionSystem::emplace<ecs::systems::LivingActionSystem>();
		_info = std::make_unique<InfoConstants>();
		for (const auto type :
		     {Reaction::LookAtNiceSpell, Reaction::FleeFromSpell, Reaction::ReactToMagicShield, Reaction::FleeFromObject,
		      Reaction::ReactToHandPickUp, Reaction::ReactToObjectCrushed, Reaction::ReactToNewBuilding})
		{
			auto& reaction = _info->reaction.at(static_cast<size_t>(type));
			reaction.priority = type == Reaction::FleeFromSpell ? 150 : 120;
			reaction.whetherItImpresses = 1;
			reaction.maxReactionDistance = 60.0f;
			reaction.howImportantIsDistance = 0.25f;
			reaction.numGameTurnsForNormalThingsToReact = 80;
			reaction.numGameTurnsForNormalThingsBeforeReactingAgain = 15;
			reaction.numGameTurnsForCreatureToReact = 80;
			reaction.numGameTurnsForCreatureBeforeReactingAgain = 0;
			reaction.defaultReactionImpressiveMultiplier = 1.0f;
			reaction.additionToTownBoredomMultipliers = 0.25f;
			reaction.minDistanceToRunAwayFromObject = 50.0f;
			reaction.maxDistanceToRunAwayFromObject = 100.0f;
		}
		for (auto& villager : _info->villager)
		{
			ReactToEverything(villager);
			// As every kind of villager in the game's data, they walk in the plain walking state
			villager.moveState = LivingStates::LivingMoveToPos;
		}
		for (auto& creature : _info->creature)
		{
			ReactToEverything(creature);
		}
		// Idling villagers may react; reacting is a state of its own
		auto& idle = _info->villagerStateTable.at(static_cast<size_t>(VillagerStates::DecideWhatToDo));
		idle.availableForReaction = 1;
		idle.isFinalState = 1;
		idle.resumeState = static_cast<int>(VillagerStates::DecideWhatToDo);
		for (const auto state : {VillagerStates::FleeingFromObjectReaction, VillagerStates::LookingAtObjectReaction,
		                         VillagerStates::FleeingAndLookingAtObjectReaction})
		{
			_info->villagerStateTable.at(static_cast<size_t>(state)).availableForReaction = 1;
			_info->villagerStateTable.at(static_cast<size_t>(state)).isReactionState = 1;
		}
		_info->town.populationForUnmodifiedBelief = 30.0f;
		_info->belief.defaultBoredomOfMe = -0.02f;
		Locator::infoConstants::emplace(*_info);

		auto& registry = Locator::entitiesRegistry::value();
		_town = registry.Create();
		for (const float x : {0.0f, 5.0f, 200.0f})
		{
			const auto villager = registry.Create();
			registry.Assign<Transform>(villager, k_Here + glm::vec3(x, 0.0f, 0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
			registry.Assign<Mobile>(villager);
			auto& component = registry.Assign<Villager>(villager);
			component.town = _town;
			component.life = 1.0f;
			registry.Assign<WallHug>(villager, WallHug {.goal = glm::vec2(0.0f), .yAngle = 0.0f, .speed = 1.0f});
			registry.Assign<LivingAction>(villager, VillagerStates::DecideWhatToDo, static_cast<uint16_t>(0));
			_villagers.push_back(villager);
		}
		_creature = registry.Create();
		registry.Assign<Transform>(_creature, k_Here, glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<Creature>(_creature).owner = PlayerNames::PLAYER_ONE;
		Locator::entitiesMap::value().Sync();
	}
	void TearDown() override
	{
		Locator::infoConstants::reset();
		Locator::livingActionSystem::reset();
		Locator::entitiesMap::reset();
		Locator::entitiesRegistry::reset();
	}

	[[nodiscard]] static ecs::systems::ReactionSystemInterface::Source Miracle(Reaction type = Reaction::LookAtNiceSpell)
	{
		return {.initiator = static_cast<entt::entity>(1000),
		        .type = type,
		        .player = PlayerNames::PLAYER_ONE,
		        .position = k_Here,
		        .impressiveValue = 2.0f,
		        .power = 1.0f,
		        .balance = ecs::systems::ReactionSystemInterface::Balance::Miracles};
	}

	[[nodiscard]] static VillagerStates StateOf(entt::entity villager)
	{
		return static_cast<VillagerStates>(Locator::entitiesRegistry::value().Get<const LivingAction>(villager).states.at(0));
	}

	std::unique_ptr<InfoConstants> _info;
	entt::entity _town {entt::null};
	std::vector<entt::entity> _villagers;
	entt::entity _creature {entt::null};
	ecs::systems::ReactionSystem _reactions;
};

TEST(ReactionRules, MoreUrgentCloserAndToFleeingMore)
{
	using namespace openblack::magic;
	// The kind's priority, the caster taking no notice of its own nice miracle
	EXPECT_EQ(KindPriority(Reaction::LookAtNiceSpell, 120, 0.0f, false), 120u);
	EXPECT_EQ(KindPriority(Reaction::LookAtNiceSpell, 120, 0.0f, true), 0u);
	// Fleeing gains up to a hundred within the urgent reach, by whole steps
	EXPECT_EQ(KindPriority(Reaction::FleeFromSpell, 150, k_FleeUrgencyUnits / 2.0f, false), 200u);
	EXPECT_EQ(KindPriority(Reaction::FleeFromSpell, 150, k_FleeUrgencyUnits, false), 150u);
	EXPECT_EQ(KindPriority(Reaction::FleeFromSpell, 150, 0.0f, true), 0u);
	EXPECT_FLOAT_EQ(FastMapDistance({0.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 4.0f}), 12.0f * k_MapUnitsPerMetre);
	// The distance then counts as for any reaction: close by up to an eighth more
	const villager_reaction::Distance reach {.maxDistance = 60.0f, .importance = 0.25f};
	EXPECT_EQ(villager_reaction::Priority(120, true, reach, 0.0f), 135u);
	EXPECT_EQ(villager_reaction::Priority(120, true, reach, 60.0f), 120u);
	EXPECT_EQ(villager_reaction::Priority(120, true, reach, 61.0f), 0u);
}

TEST(ReactionRules, ADifferentMoreUrgentKindTakesOverOnlyAfterAWhile)
{
	using namespace openblack::magic;
	EXPECT_FALSE(ChangesReaction(Reaction::LookAtNiceSpell, 120, 200, 9));
	EXPECT_TRUE(ChangesReaction(Reaction::LookAtNiceSpell, 120, 200, 10));
	EXPECT_FALSE(ChangesReaction(Reaction::LookAtNiceSpell, 200, 200, 20));
	// Reacting to the hand picking something up, a second is enough
	EXPECT_FALSE(ChangesReaction(Reaction::ReactToHandPickUp, 120, 200, 0));
	EXPECT_TRUE(ChangesReaction(Reaction::ReactToHandPickUp, 120, 200, 1));
	EXPECT_FALSE(ChangesReaction(Reaction::LookAtNiceSpell, 120, 200, 1));
}

TEST(ReactionRules, VillagersFleeAwayOrAcrossTheWay)
{
	using namespace openblack::magic;
	const auto still = FleePointFromStill({10.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f});
	EXPECT_FLOAT_EQ(still.x, 20.0f);
	EXPECT_FLOAT_EQ(still.z, 0.0f);
	// Something moving north past a villager to its east: the villager goes on east
	const auto moving = FleePointFromMoving({5.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, 4.0f, 4.0f);
	EXPECT_FLOAT_EQ(moving.x, 5.0f + 10.0f);
	EXPECT_FLOAT_EQ(moving.z, 0.0f);
	EXPECT_TRUE(ComingTowards({0.0f, 0.0f, 10.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}));
	EXPECT_FALSE(ComingTowards({10.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}));
}

TEST_F(Reactions, VillagersInReachTakeUpANiceMiracleAndBelieveOnce)
{
	_reactions.Create(Miracle());
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* impression = registry.TryGet<const TownImpression>(_town);
	ASSERT_NE(impression, nullptr);
	// The two villagers in reach, the first right by it, the other a little away; the third is too far. Each impression
	// is shared by the town's three people against the thirty its belief is unchanged for.
	// It waits as the town's pending belief until the town's turn
	const float belief = impression->belief.pending.at(static_cast<size_t>(PlayerNames::PLAYER_ONE));
	EXPECT_FLOAT_EQ(impression->belief.belief.at(static_cast<size_t>(PlayerNames::PLAYER_ONE)), 0.0f);
	const float share = (30.0f + 0.001f) / (3.0f + 0.001f);
	EXPECT_GT(belief, 2.0f * share);
	EXPECT_LT(belief, 4.0f * share);
	// Each villager's impression tires the town by the belief table's boredom times its share of the town
	EXPECT_FLOAT_EQ(impression->boredom.at(Reaction::LookAtNiceSpell), 1.0f - (2.0f * 0.02f * share));
	EXPECT_EQ(StateOf(_villagers[0]), VillagerStates::LookingAtObjectReaction);
	EXPECT_EQ(StateOf(_villagers[2]), VillagerStates::DecideWhatToDo);
	// Spread again, those reacting already aren't impressed again
	_reactions.ProcessTurn();
	const auto& town = registry.Get<const TownImpression>(_town).belief;
	// Believed at the town's turn, no more than its cap
	EXPECT_FLOAT_EQ(town.belief.at(static_cast<size_t>(PlayerNames::PLAYER_ONE)),
	                std::min(belief, openblack::magic::town_belief::k_DefaultCap));
	EXPECT_FLOAT_EQ(town.pending.at(static_cast<size_t>(PlayerNames::PLAYER_ONE)), 0.0f);
}

TEST_F(Reactions, VillagersStopAfterTheirTimeAndGoBack)
{
	_reactions.Create(Miracle());
	for (int turn = 0; turn < 100; ++turn)
	{
		_reactions.ProcessTurn();
	}
	EXPECT_EQ(StateOf(_villagers[0]), VillagerStates::DecideWhatToDo);
	EXPECT_EQ(Locator::entitiesRegistry::value().Get<const LivingReaction>(_villagers[0]).reaction, 0u);
}

TEST_F(Reactions, AVillagerLettingGoOfAReactionKeepsWhatItDoesAndWaitsItsTimeFromThen)
{
	for (int turn = 0; turn < 5; ++turn)
	{
		_reactions.ProcessTurn();
	}
	_reactions.Create(Miracle());
	auto& registry = Locator::entitiesRegistry::value();
	ASSERT_NE(registry.Get<const LivingReaction>(_villagers[0]).reaction, 0u);
	_reactions.StopReacting(_villagers[0]);
	const auto& state = registry.Get<const LivingReaction>(_villagers[0]);
	EXPECT_EQ(state.reaction, 0u);
	EXPECT_EQ(state.type, Reaction::None);
	// It goes on with what it was doing; only the reaction is let go
	EXPECT_EQ(StateOf(_villagers[0]), VillagerStates::LookingAtObjectReaction);
	// Its wait before taking up the kind again counts from the fifth turn, when it let go
	const auto& memory = registry.Get<const VillagerReactionMemory>(_villagers[0]).memory;
	EXPECT_EQ(memory.LastReacted(static_cast<uint32_t>(Reaction::LookAtNiceSpell)), 5u);
}

TEST_F(Reactions, AVillagerKeepsTheFireReactionItTookUpAndLetsGoOfItLeavingTheFire)
{
	// The fire's reaction is spread as soon as it is made, before the fire knows it: the villager keeps the one it took
	auto& registry = Locator::entitiesRegistry::value();
	auto& stateTable = _info->villagerStateTable;
	stateTable.at(static_cast<size_t>(VillagerStates::ReactToFire)).isReactionState = 1;
	stateTable.at(static_cast<size_t>(VillagerStates::ReactToFire)).availableForReaction = 1;
	Locator::infoConstants::emplace(*_info);
	Locator::reactionSystem::emplace<ecs::systems::ReactionSystem>();
	const auto villager = _villagers[0];
	const auto burning = registry.Create();
	registry.Assign<Transform>(burning, k_Here, glm::mat3(1.0f), glm::vec3(1.0f));
	registry.AssignOrReplace<LivingReaction>(villager, LivingReaction {.reaction = 7, .type = Reaction::ReactToFire});
	villager_fire::SetupReactToFire(villager, burning, 7);
	EXPECT_EQ(registry.Get<const VillagerFireState>(villager).reaction, 7u);
	EXPECT_EQ(StateOf(villager), VillagerStates::ReactToFire);
	// Leaving the fire for something that isn't a reaction, it lets go of the reaction, so it can take up the fire
	// again in its time
	auto& action = registry.Get<LivingAction>(villager);
	villager_fire::ExitReaction(action, VillagerStates::DecideWhatToDo);
	EXPECT_EQ(registry.Get<const VillagerFireState>(villager).reaction, 0u);
	EXPECT_EQ(registry.Get<const LivingReaction>(villager).reaction, 0u);
	Locator::reactionSystem::reset();
}

TEST_F(Reactions, AVillagerGoingFromAFireToFightItKeepsTheReactionItNeeds)
{
	// Going on to fight the fire, or round it, is another reaction state, so it keeps the reaction those states need
	auto& registry = Locator::entitiesRegistry::value();
	_info->villagerStateTable.at(static_cast<size_t>(VillagerStates::MoveAroundFire)).isReactionState = 1;
	Locator::infoConstants::emplace(*_info);
	Locator::reactionSystem::emplace<ecs::systems::ReactionSystem>();
	const auto villager = _villagers[0];
	const auto burning = registry.Create();
	registry.Assign<Transform>(burning, k_Here, glm::mat3(1.0f), glm::vec3(1.0f));
	registry.AssignOrReplace<LivingReaction>(villager, LivingReaction {.reaction = 7, .type = Reaction::ReactToFire});
	villager_fire::SetupReactToFire(villager, burning, 7);
	auto& action = registry.Get<LivingAction>(villager);
	EXPECT_FALSE(villager_fire::ExitReaction(action, VillagerStates::MoveAroundFire));
	EXPECT_EQ(registry.Get<const VillagerFireState>(villager).reaction, 7u);
	EXPECT_EQ(registry.Get<const LivingReaction>(villager).reaction, 7u);
	Locator::reactionSystem::reset();
}

TEST(VillagerFireRules, AFiremanRememberingNothingDecidesWhatToDoRatherThanStandingStill)
{
	EXPECT_EQ(villager_fire::StateToGoBackTo(VillagerStates::InvalidState), VillagerStates::DecideWhatToDo);
	EXPECT_EQ(villager_fire::StateToGoBackTo(VillagerStates::PutOutFireByBeating), VillagerStates::PutOutFireByBeating);
	EXPECT_EQ(villager_fire::StateToGoBackTo(VillagerStates::DecideWhatToDo), VillagerStates::DecideWhatToDo);
}

TEST_F(Reactions, AFiremanWhoseFiresObjectGoesWithNothingRememberedDecidesWhatToDo)
{
	// Beating a fire in its place, the earlier walk having used up what it remembered, when the object whose fire it
	// reacted to goes
	auto& registry = Locator::entitiesRegistry::value();
	Locator::reactionSystem::emplace<ecs::systems::ReactionSystem>();
	const auto villager = _villagers[0];
	const auto gone = registry.Create();
	registry.Destroy(gone);
	auto& action = registry.Get<LivingAction>(villager);
	action.states.at(static_cast<size_t>(LivingAction::Index::Top)) = static_cast<uint8_t>(VillagerStates::PutOutFireByBeating);
	action.states.at(static_cast<size_t>(LivingAction::Index::Previous)) = static_cast<uint8_t>(VillagerStates::InvalidState);
	registry.Assign<VillagerFireState>(villager, VillagerFireState {.fire = entt::null, .reactionTarget = gone, .reaction = 7});
	EXPECT_TRUE(villager_fire::ReactionValidate(action));
	EXPECT_EQ(StateOf(villager), VillagerStates::DecideWhatToDo);
	Locator::reactionSystem::reset();
}

TEST_F(Reactions, AFiremanWalkingToAFireThatGoesLeavesTheBlazeAndLetsGoOfItsReaction)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& stateTable = _info->villagerStateTable;
	for (const auto state : {VillagerStates::PutOutFireByBeating, VillagerStates::MoveAroundFire})
	{
		auto& row = stateTable.at(static_cast<size_t>(state));
		row.isFinalState = 1;
		row.isReactionState = 1;
		row.resumeState = static_cast<int>(state);
	}
	Locator::infoConstants::emplace(*_info);
	Locator::reactionSystem::emplace<ecs::systems::ReactionSystem>();
	Locator::fireSystem::emplace<ecs::systems::FireSystem>();
	const auto villager = _villagers[0];
	const auto burning = registry.Create();
	registry.Assign<Transform>(burning, k_Here, glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Fire>(burning).root = burning;
	registry.Assign<FireGroup>(burning, FireGroup {.members = {burning}, .firemen = {villager}});
	// Walking round the fire to its place to beat it from
	auto& action = registry.Get<LivingAction>(villager);
	action.states = {static_cast<uint8_t>(VillagerStates::MoveToPos), static_cast<uint8_t>(VillagerStates::MoveAroundFire),
	                 static_cast<uint8_t>(VillagerStates::PutOutFireByBeating)};
	registry.Assign<VillagerFireState>(villager, VillagerFireState {.fire = burning, .reactionTarget = burning, .reaction = 7});
	registry.AssignOrReplace<LivingReaction>(villager, LivingReaction {.reaction = 7, .type = Reaction::ReactToFire});
	villager_fire::StopFireFighting(villager);
	EXPECT_TRUE(registry.Get<const FireGroup>(burning).firemen.empty());
	// It walks on, then decides what to do; leaving the fire state it was heading for lets go of the reaction
	EXPECT_EQ(StateOf(villager), VillagerStates::MoveToPos);
	EXPECT_EQ(static_cast<VillagerStates>(action.states.at(static_cast<size_t>(LivingAction::Index::Final))),
	          VillagerStates::DecideWhatToDo);
	EXPECT_EQ(registry.Get<const VillagerFireState>(villager).reaction, 0u);
	EXPECT_EQ(registry.Get<const LivingReaction>(villager).reaction, 0u);
	Locator::fireSystem::reset();
	Locator::reactionSystem::reset();
}

TEST_F(Reactions, AFiremanWhoseFireCoolsOnTheWayIsNotLeftAmongTheBlazesFiremen)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& stateTable = _info->villagerStateTable;
	for (const auto state : {VillagerStates::PutOutFireByBeating, VillagerStates::MoveAroundFire})
	{
		auto& row = stateTable.at(static_cast<size_t>(state));
		row.isFinalState = 1;
		row.isReactionState = 1;
		row.resumeState = static_cast<int>(state);
	}
	Locator::infoConstants::emplace(*_info);
	Locator::reactionSystem::emplace<ecs::systems::ReactionSystem>();
	Locator::fireSystem::emplace<ecs::systems::FireSystem>();
	auto& reactions = Locator::reactionSystem::value();
	const auto villager = _villagers[0];
	const auto burning = registry.Create();
	registry.Assign<Transform>(burning, k_Here, glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Fire>(burning).root = burning;
	registry.Assign<FireGroup>(burning, FireGroup {.members = {burning}, .firemen = {villager}});
	const auto reaction = reactions.Create({.initiator = burning, .type = Reaction::ReactToFire, .position = k_Here});
	// Walking round the fire to its place to beat it from, one of the blaze's firemen
	auto& action = registry.Get<LivingAction>(villager);
	action.states = {static_cast<uint8_t>(VillagerStates::MoveToPos), static_cast<uint8_t>(VillagerStates::MoveAroundFire),
	                 static_cast<uint8_t>(VillagerStates::PutOutFireByBeating)};
	registry.Assign<VillagerFireState>(villager,
	                                   VillagerFireState {.fire = burning, .reactionTarget = burning, .reaction = reaction});
	registry.AssignOrReplace<LivingReaction>(villager, LivingReaction {.reaction = reaction, .type = Reaction::ReactToFire});
	// The fire cools below where the living react to it, so its reaction ends
	reactions.RemoveFrom(burning, Reaction::ReactToFire);
	// The villager goes back the fire's way: the fire states it leaves take it off the firemen, rather than it being left
	// listed while it does something else
	EXPECT_EQ(StateOf(villager), VillagerStates::DecideWhatToDo);
	EXPECT_TRUE(registry.Get<const FireGroup>(burning).firemen.empty());
	EXPECT_EQ(registry.Get<const VillagerFireState>(villager).reaction, 0u);
	EXPECT_EQ(registry.Get<const LivingReaction>(villager).reaction, 0u);
	Locator::fireSystem::reset();
	Locator::reactionSystem::reset();
}

TEST_F(Reactions, AFiremanWithinAStepOfItsPlaceByTheFireGoesOnToBeatIt)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& stateTable = _info->villagerStateTable;
	for (const auto state : {VillagerStates::PutOutFireByBeating, VillagerStates::MoveAroundFire})
	{
		auto& row = stateTable.at(static_cast<size_t>(state));
		row.isFinalState = 1;
		row.isReactionState = 1;
		row.resumeState = static_cast<int>(state);
	}
	Locator::infoConstants::emplace(*_info);
	Locator::reactionSystem::emplace<ecs::systems::ReactionSystem>();
	Locator::fireSystem::emplace<ecs::systems::FireSystem>();
	auto& reactions = Locator::reactionSystem::value();
	const auto villager = _villagers[0];
	const auto burning = registry.Create();
	registry.Assign<Transform>(burning, k_Here + glm::vec3(0.0f, 0.0f, 6.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Fire>(burning).root = burning;
	registry.Assign<FireGroup>(burning, FireGroup {.members = {burning}, .firemen = {villager}});
	const auto reaction = reactions.Create({.initiator = burning, .type = Reaction::ReactToFire, .position = k_Here});
	// Going round the fire to its place to beat it from. The place is kept to the map's grid, so after the walk it
	// stands a hair from it, much less than the 10 cm its walk makes in a turn
	auto& action = registry.Get<LivingAction>(villager);
	action.states = {static_cast<uint8_t>(VillagerStates::MoveAroundFire), static_cast<uint8_t>(VillagerStates::InvalidState),
	                 static_cast<uint8_t>(VillagerStates::PutOutFireByBeating)};
	registry.Assign<VillagerFireState>(villager, VillagerFireState {.fire = burning,
	                                                                .reactionTarget = burning,
	                                                                .reaction = reaction,
	                                                                .walkTarget = {k_Here.x + 0.0005f, k_Here.z + 0.05f}});
	registry.AssignOrReplace<LivingReaction>(villager, LivingReaction {.reaction = reaction, .type = Reaction::ReactToFire});
	EXPECT_EQ(villager_fire::MoveAroundFire(action), 1u);
	// It is there: on to beating the fire, still one of the blaze's firemen, rather than setting off on a walk to where it
	// stands
	EXPECT_EQ(StateOf(villager), VillagerStates::PutOutFireByBeating);
	EXPECT_EQ(static_cast<VillagerStates>(action.states.at(static_cast<size_t>(LivingAction::Index::Previous))),
	          VillagerStates::DecideWhatToDo);
	EXPECT_EQ(registry.Get<const FireGroup>(burning).firemen, std::vector<entt::entity> {villager});
	Locator::fireSystem::reset();
	Locator::reactionSystem::reset();
}

TEST_F(Reactions, GoingIntoANewStateLetsGoOfTheStateItWasWalkingTo)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& living = Locator::livingActionSystem::value();
	auto& action = registry.Get<LivingAction>(_villagers[0]);
	const auto final = [&action] {
		return static_cast<VillagerStates>(action.states.at(static_cast<size_t>(LivingAction::Index::Final)));
	};
	// Setting off on a walk goes into the walk and where it is heading together
	ASSERT_TRUE(
	    living.VillagerSetCurrentAndDestinationState(action, VillagerStates::MoveToPos, VillagerStates::DecideWhatToDo));
	EXPECT_EQ(StateOf(_villagers[0]), VillagerStates::MoveToPos);
	EXPECT_EQ(final(), VillagerStates::DecideWhatToDo);
	// Arriving, it goes into that state and heads for nothing more
	living.VillagerSetState(action, LivingAction::Index::Top, VillagerStates::DecideWhatToDo, false);
	EXPECT_EQ(StateOf(_villagers[0]), VillagerStates::DecideWhatToDo);
	EXPECT_EQ(final(), VillagerStates::InvalidState);
	// Any new state lets go of where it was heading, even one that isn't an end in itself
	ASSERT_TRUE(
	    living.VillagerSetCurrentAndDestinationState(action, VillagerStates::MoveToPos, VillagerStates::DecideWhatToDo));
	living.VillagerSetState(action, LivingAction::Index::Top, VillagerStates::WaitForCounter, false);
	EXPECT_EQ(StateOf(_villagers[0]), VillagerStates::WaitForCounter);
	EXPECT_EQ(final(), VillagerStates::InvalidState);
}

TEST_F(Reactions, AFiremanWalkingAwayFromTheFireIsNoLongerOneOfItsFiremen)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& row = _info->villagerStateTable.at(static_cast<size_t>(VillagerStates::PutOutFireByBeating));
	row.isFinalState = 1;
	row.isReactionState = 1;
	row.resumeState = static_cast<int>(VillagerStates::PutOutFireByBeating);
	Locator::infoConstants::emplace(*_info);
	Locator::reactionSystem::emplace<ecs::systems::ReactionSystem>();
	Locator::fireSystem::emplace<ecs::systems::FireSystem>();
	const auto villager = _villagers[0];
	const auto burning = registry.Create();
	registry.Assign<Transform>(burning, k_Here, glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Fire>(burning).root = burning;
	registry.Assign<FireGroup>(burning, FireGroup {.members = {burning}, .firemen = {villager}});
	// Beating the fire, it is sent off to walk somewhere and then decide what to do
	auto& action = registry.Get<LivingAction>(villager);
	action.states = {static_cast<uint8_t>(VillagerStates::PutOutFireByBeating),
	                 static_cast<uint8_t>(VillagerStates::InvalidState), static_cast<uint8_t>(VillagerStates::InvalidState)};
	registry.Assign<VillagerFireState>(villager, VillagerFireState {.fire = burning, .reactionTarget = burning, .reaction = 7});
	registry.AssignOrReplace<LivingReaction>(villager, LivingReaction {.reaction = 7, .type = Reaction::ReactToFire});
	ASSERT_TRUE(villager_home::SetupMoveTo(action, glm::vec2(600.0f, 600.0f), VillagerStates::DecideWhatToDo));
	// Its fire state hears where it is heading, not that it walks, so it leaves the blaze rather than staying listed
	EXPECT_TRUE(registry.Get<const FireGroup>(burning).firemen.empty());
	EXPECT_EQ(StateOf(villager), VillagerStates::MoveToPos);
	EXPECT_EQ(static_cast<VillagerStates>(action.states.at(static_cast<size_t>(LivingAction::Index::Final))),
	          VillagerStates::DecideWhatToDo);
	EXPECT_EQ(registry.Get<const WallHug>(villager).goal, glm::vec2(600.0f, 600.0f));
	Locator::fireSystem::reset();
	Locator::reactionSystem::reset();
}

TEST_F(Reactions, AVillagerSettingOffOnAWalkNeverStopsForASecondFirst)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& living = Locator::livingActionSystem::value();
	// Every villager that may stop for a second does
	for (const auto state : {VillagerStates::MoveToPos, VillagerStates::DecideWhatToDo})
	{
		_info->villagerStateTable.at(static_cast<size_t>(state)).canPauseForASecond = 1;
	}
	for (auto& villager : _info->villager)
	{
		villager.pauseForASecondChance = 2.0f;
	}
	Locator::infoConstants::emplace(*_info);
	Locator::gameRandom::emplace<AlwaysLowGameRandom>();
	const auto villager = _villagers[0];
	auto& action = registry.Get<LivingAction>(villager);
	const auto final = [&action] {
		return static_cast<VillagerStates>(action.states.at(static_cast<size_t>(LivingAction::Index::Final)));
	};
	// Going into a state plainly, it stops for a second first, then goes on into that state
	action.states.at(static_cast<size_t>(LivingAction::Index::Top)) = static_cast<uint8_t>(VillagerStates::InvalidState);
	living.VillagerSetState(action, LivingAction::Index::Top, VillagerStates::DecideWhatToDo, false);
	EXPECT_EQ(StateOf(villager), VillagerStates::PauseForASecond);
	EXPECT_EQ(final(), VillagerStates::DecideWhatToDo);
	// Setting off on a walk from the pause, it walks at once and keeps where the walk is heading
	ASSERT_TRUE(villager_home::SetupMoveTo(action, glm::vec2(600.0f, 600.0f), VillagerStates::DecideWhatToDo));
	EXPECT_EQ(StateOf(villager), VillagerStates::MoveToPos);
	EXPECT_EQ(final(), VillagerStates::DecideWhatToDo);
	Locator::gameRandom::reset();
}

TEST(ReactionRules, LettingGoOfAKindBringsItUpToNowOnlyWhenRemembered)
{
	openblack::magic::villager_reaction::Memory memory;
	ASSERT_TRUE(memory.MayReactAgain(1, 10, 0));
	memory.Refresh(1, 50);
	EXPECT_EQ(memory.LastReacted(1), 50u);
	// A kind it doesn't remember isn't remembered by letting go of it
	memory.Refresh(2, 60);
	EXPECT_EQ(memory.LastReacted(2), 0u);
}

TEST_F(Reactions, VillagersFleeAFrighteningMiracle)
{
	// A turn in, as a turn of 0 is remembered as not lately
	_reactions.ProcessTurn();
	_reactions.Create(Miracle(Reaction::FleeFromSpell));
	EXPECT_EQ(StateOf(_villagers[0]), VillagerStates::FleeingFromObjectReaction);
	// Remembered with the kinds the shields' reactions see
	const auto& memory = Locator::entitiesRegistry::value().Get<const VillagerReactionMemory>(_villagers[0]).memory;
	EXPECT_NE(memory.LastReacted(static_cast<uint32_t>(Reaction::FleeFromSpell)), 0u);
	// Its going takes everyone off it
	_reactions.RemoveFrom(static_cast<entt::entity>(1000));
	EXPECT_EQ(StateOf(_villagers[0]), VillagerStates::DecideWhatToDo);
}

TEST_F(Reactions, TheCreatureIsImpressedByItsOwnPlayersMiracle)
{
	_reactions.Create(Miracle());
	const auto* impression = Locator::entitiesRegistry::value().TryGet<const CreatureImpression>(_creature);
	ASSERT_NE(impression, nullptr);
	EXPECT_FLOAT_EQ(impression->byOwnPlayer, 2.0f * 4.0f);
	EXPECT_FLOAT_EQ(impression->byOtherCreatures, 0.0f);
}

TEST_F(Reactions, AnotherPlayersMiracleImpressesACreatureNotAtAllWhoeverCastIt)
{
	// Another player's creature cast it: a miracle isn't a creature, so it impresses nothing
	auto& registry = Locator::entitiesRegistry::value();
	const auto other = registry.Create();
	registry.Assign<Creature>(other).owner = PlayerNames::PLAYER_TWO;
	auto source = Miracle(Reaction::FleeFromSpell);
	source.player = PlayerNames::PLAYER_TWO;
	source.casterCreature = other;
	_reactions.Create(source);
	const auto* impression = registry.TryGet<const CreatureImpression>(_creature);
	ASSERT_NE(impression, nullptr);
	EXPECT_FLOAT_EQ(impression->byOwnPlayer, 0.0f);
	EXPECT_FLOAT_EQ(impression->byOtherCreatures, 0.0f);
}

TEST_F(Reactions, AnotherCreatureOfItsPlayerImpressesItAsItsPlayer)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto other = registry.Create();
	registry.Assign<Creature>(other).owner = PlayerNames::PLAYER_ONE;
	auto source = Miracle(Reaction::FleeFromSpell);
	source.casterCreature = other;
	_reactions.Create(source);
	const auto* impression = registry.TryGet<const CreatureImpression>(_creature);
	ASSERT_NE(impression, nullptr);
	EXPECT_GT(impression->byOwnPlayer, 0.0f);
	EXPECT_FLOAT_EQ(impression->byOtherCreatures, 0.0f);
}

TEST_F(Reactions, ACreatureIsNotImpressedByItsOwnMiracle)
{
	auto source = Miracle();
	source.casterCreature = _creature;
	_reactions.Create(source);
	_reactions.ProcessTurn();
	EXPECT_EQ(Locator::entitiesRegistry::value().TryGet<const CreatureImpression>(_creature), nullptr);
}

TEST_F(Reactions, AReactionGoesWithWhatMadeItAndIsFoundWhereItReaches)
{
	const auto id = _reactions.Create(Miracle());
	EXPECT_TRUE(_reactions.HasReaction(static_cast<entt::entity>(1000)));
	EXPECT_EQ(_reactions.ReactionsAt(k_Here + glm::vec3(30.0f, 0.0f, 0.0f)).size(), 1u);
	EXPECT_TRUE(_reactions.ReactionsAt(k_Here + glm::vec3(100.0f, 0.0f, 0.0f)).empty());
	_reactions.Move(id, k_Here + glm::vec3(100.0f, 0.0f, 0.0f), glm::vec3(0.0f), 1.0f);
	EXPECT_EQ(_reactions.ReactionsAt(k_Here + glm::vec3(100.0f, 0.0f, 0.0f)).size(), 1u);
	_reactions.RemoveFrom(static_cast<entt::entity>(1000));
	EXPECT_FALSE(_reactions.HasReaction(static_cast<entt::entity>(1000)));
	EXPECT_TRUE(_reactions.GetReactions().empty());
}

TEST_F(Reactions, TheLandsBalanceScalesHowImpressiveMiraclesAre)
{
	using Balance = ecs::systems::ReactionSystemInterface::Balance;
	_reactions.SetLandBalance(Balance::Miracles, 2.0f);
	_reactions.SetLandBalance(Balance::Things, 3.0f);
	_reactions.Create(Miracle());
	EXPECT_FLOAT_EQ(Locator::entitiesRegistry::value().Get<const CreatureImpression>(_creature).byOwnPlayer,
	                2.0f * 2.0f * 4.0f);
	_reactions.Reset();
	EXPECT_FLOAT_EQ(_reactions.GetLandBalance(Balance::Miracles), 1.0f);
	EXPECT_FLOAT_EQ(_reactions.GetLandBalance(Balance::Things), 1.0f);
}

TEST_F(Reactions, AThingInTheWorldIsScaledByTheThingsBalanceNotTheMiracles)
{
	using Balance = ecs::systems::ReactionSystemInterface::Balance;
	// Land 2 raises both; a thrown rock or a put-down artefact takes the things' balance
	_reactions.SetLandBalance(Balance::Miracles, 2.0f);
	_reactions.SetLandBalance(Balance::Things, 3.0f);
	auto thing = Miracle();
	thing.balance = Balance::Things;
	_reactions.Create(thing);
	EXPECT_FLOAT_EQ(Locator::entitiesRegistry::value().Get<const CreatureImpression>(_creature).byOwnPlayer,
	                3.0f * 2.0f * 4.0f);
}

TEST_F(Reactions, ADancingVillagerNoticesAFlyingThingOnlyWhenItComesAtIt)
{
	auto& reaction = _info->reaction.at(static_cast<size_t>(Reaction::ReactToFlyingObject));
	reaction.priority = 120;
	reaction.whetherItImpresses = 1;
	reaction.maxReactionDistance = 60.0f;
	reaction.numGameTurnsForNormalThingsToReact = 80;
	reaction.numGameTurnsForNormalThingsBeforeReactingAgain = 15;
	reaction.defaultReactionImpressiveMultiplier = 1.0f;
	_info->villagerStateTable.at(static_cast<size_t>(VillagerStates::PointAtFlyingObjectReaction)).isReactionState = 1;
	Locator::infoConstants::emplace(*_info);
	auto& registry = Locator::entitiesRegistry::value();
	// Every villager dances; the thing flies away from the first and at the second
	for (auto* villager : {&_villagers[0], &_villagers[1], &_villagers[2]})
	{
		registry.Assign<Dancer>(*villager);
	}
	const auto rock = registry.Create();
	registry.Assign<Transform>(rock, k_Here + glm::vec3(2.5f, 3.0f, 0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	Locator::dynamicsSystem::emplace<FlyingThing>(rock, glm::vec3(1.0f, -1.0f, 0.0f));
	_reactions.Create({.initiator = rock,
	                   .type = Reaction::ReactToFlyingObject,
	                   .player = PlayerNames::PLAYER_ONE,
	                   .position = k_Here + glm::vec3(2.5f, 3.0f, 0.0f),
	                   .impressiveValue = 1.0f});
	const auto reactingTo = [&registry](entt::entity villager) {
		const auto* state = registry.TryGet<const LivingReaction>(villager);
		return state != nullptr ? state->reaction : 0u;
	};
	_reactions.ProcessTurn();
	EXPECT_EQ(reactingTo(_villagers[0]), 0u);
	EXPECT_NE(reactingTo(_villagers[1]), 0u);
	Locator::dynamicsSystem::reset();
}

TEST_F(Reactions, AShieldIsLeftToTheShieldsForVillagersButACreatureTakesItUp)
{
	_reactions.Create(Miracle(Reaction::ReactToMagicShield));
	const auto& registry = Locator::entitiesRegistry::value();
	EXPECT_EQ(StateOf(_villagers[0]), VillagerStates::DecideWhatToDo);
	EXPECT_EQ(registry.TryGet<const LivingReaction>(_villagers[0]), nullptr);
	EXPECT_EQ(registry.Get<const LivingReaction>(_creature).type, Reaction::ReactToMagicShield);
}

TEST_F(Reactions, ACreatureIgnoresAnotherPlayersShield)
{
	auto source = Miracle(Reaction::ReactToMagicShield);
	source.player = PlayerNames::PLAYER_TWO;
	_reactions.Create(source);
	const auto* state = Locator::entitiesRegistry::value().TryGet<const LivingReaction>(_creature);
	EXPECT_TRUE(state == nullptr || state->reaction == 0);
}

TEST_F(Reactions, VillagersBusyWithAShieldAreLeftToIt)
{
	Locator::entitiesRegistry::value().Assign<VillagerShieldReaction>(_villagers[0]);
	_reactions.Create(Miracle(Reaction::FleeFromSpell));
	EXPECT_EQ(StateOf(_villagers[0]), VillagerStates::DecideWhatToDo);
	EXPECT_EQ(StateOf(_villagers[1]), VillagerStates::FleeingFromObjectReaction);
}

TEST_F(Reactions, OnlyAMateToBeWaitsForAVillagerHeldInTheHandAndOnlyWhileItIsNear)
{
	auto& info = *_info;
	// Women are housewives and men foresters; both of an age to make love
	for (auto& row : info.villager)
	{
		row.villagerNumber = VillagerNumber::Trader;
	}
	info.villager.at(0).villagerNumber = VillagerNumber::Housewife;
	info.villager.at(0).sex = SexType::Female;
	info.villager.at(1).villagerNumber = VillagerNumber::Forester;
	info.villager.at(1).sex = SexType::Male;
	for (const size_t row : {0u, 1u})
	{
		info.villager.at(row).startHavingSexAge = 0;
		info.villager.at(row).stopHavingSexAge = 100;
	}
	// The game weighs this reaction by the breeder's row
	info.reaction.at(static_cast<size_t>(Reaction::ReactToBreeder)).priority = 140;
	auto& inHand = info.reaction.at(static_cast<size_t>(Reaction::ReactToVillagerInHand));
	inHand.maxReactionDistance = 20.0f;
	inHand.numGameTurnsForNormalThingsToReact = 1000;
	inHand.numGameTurnsForCreatureToReact = 1000;
	info.villagerStateTable.at(static_cast<size_t>(VillagerStates::InHand)).isFinalState = 1;
	info.villagerStateTable.at(static_cast<size_t>(VillagerStates::WaitForMate)).isReactionState = 1;
	Locator::infoConstants::emplace(info);

	auto& registry = Locator::entitiesRegistry::value();
	// A man of the town held in the hand right by the first two villagers; the second is a man too
	const auto held = registry.Create();
	registry.Assign<Transform>(held, k_Here, glm::mat3(1.0f), glm::vec3(1.0f));
	auto& man = registry.Assign<Villager>(held);
	man.town = _town;
	man.number = VillagerNumber::Forester;
	registry.Assign<LivingAction>(held, VillagerStates::InHand, static_cast<uint16_t>(0));
	registry.Get<Villager>(_villagers[1]).number = VillagerNumber::Forester;

	const auto id = _reactions.Create(
	    {.initiator = held, .type = Reaction::ReactToVillagerInHand, .player = PlayerNames::PLAYER_ONE, .position = k_Here});
	EXPECT_EQ(StateOf(_villagers[0]), VillagerStates::WaitForMate);
	EXPECT_EQ(StateOf(_villagers[1]), VillagerStates::DecideWhatToDo);
	EXPECT_EQ(StateOf(_villagers[2]), VillagerStates::DecideWhatToDo);
	// The creature takes no notice of it
	const auto* creature = registry.TryGet<const LivingReaction>(_creature);
	EXPECT_TRUE(creature == nullptr || creature->reaction != id);

	// Carried out of her reach, the reaction follows him and she stops waiting
	registry.Get<Transform>(held).position = k_Here + glm::vec3(0.0f, 0.0f, 30.0f);
	_reactions.ProcessTurn();
	ASSERT_TRUE(_reactions.Find(id).has_value());
	EXPECT_EQ(_reactions.Find(id)->source.position, k_Here + glm::vec3(0.0f, 0.0f, 30.0f));
	ecs::systems::villager_reactions::WaitForMate(registry.Get<LivingAction>(_villagers[0]));
	EXPECT_EQ(StateOf(_villagers[0]), VillagerStates::DecideWhatToDo);
	EXPECT_EQ(registry.Get<const LivingReaction>(_villagers[0]).reaction, 0u);
}

TEST(ReactionRules, MostReactionsTimeOutButNotFleeingNorImpressedByMiracles)
{
	using namespace openblack::magic;
	EXPECT_TRUE(TimesOut(Reaction::LookAtNiceSpell));
	EXPECT_TRUE(TimesOut(Reaction::ReactToObjectCrushed));
	EXPECT_FALSE(TimesOut(Reaction::FleeFromSpell));
	EXPECT_FALSE(TimesOut(Reaction::ReactToImpressiveSpell));
	// Its time starts with its stamp: none, never
	EXPECT_FALSE(TimedOut(Reaction::LookAtNiceSpell, std::nullopt, 500, 80));
	EXPECT_FALSE(TimedOut(Reaction::LookAtNiceSpell, 10, 90, 80));
	EXPECT_TRUE(TimedOut(Reaction::LookAtNiceSpell, 10, 91, 80));
	EXPECT_FLOAT_EQ(StartingReach(true, 60.0f), 1.0f);
	EXPECT_FLOAT_EQ(StartingReach(false, 60.0f), 60.0f);
}

TEST(ShieldRules, WhatIsSurelyWithinAShield)
{
	using namespace openblack::magic::shield;
	EXPECT_TRUE(WithinSpiritualShield({0.0f, 0.0f, 0.0f}, 10.0f, {6.0f, 0.0f, 7.9f}));
	EXPECT_FALSE(WithinSpiritualShield({0.0f, 0.0f, 0.0f}, 10.0f, {6.0f, 0.0f, 8.0f}));
	const DomeVolume dome {.radius = 20.0f, .height = 10.0f};
	EXPECT_TRUE(WithinPhysicalShield(dome, 10.0f, 4.9f));
	EXPECT_FALSE(WithinPhysicalShield(dome, 10.0f, 5.0f));
	EXPECT_FALSE(WithinPhysicalShield(dome, 20.0f, 0.0f));
}

// A script makes a thing for the living around it to run from: the villagers near it flee as from a frightening miracle
TEST_F(Reactions, VillagersFleeAThingToFlee)
{
	_reactions.ProcessTurn();
	_reactions.Create({.initiator = _creature,
	                   .type = Reaction::FleeFromObject,
	                   .player = PlayerNames::NEUTRAL,
	                   .position = k_Here,
	                   .playerless = true});
	EXPECT_EQ(StateOf(_villagers[0]), VillagerStates::FleeingFromObjectReaction);
	EXPECT_EQ(StateOf(_villagers[1]), VillagerStates::FleeingFromObjectReaction);
	// Out of its reach
	EXPECT_EQ(StateOf(_villagers[2]), VillagerStates::DecideWhatToDo);
}

// A creature's reaction goes where the creature goes, moving the way it faces
TEST_F(Reactions, ACreaturesReactionFollowsIt)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& locomotion = registry.Assign<CreatureLocomotion>(_creature);
	locomotion.started = true;
	locomotion.heading = 1.0f;
	const auto id = _reactions.Create({.initiator = _creature,
	                                   .type = Reaction::FleeFromObject,
	                                   .player = PlayerNames::NEUTRAL,
	                                   .position = k_Here,
	                                   .playerless = true});
	const glm::vec3 moved = k_Here + glm::vec3(20.0f, 0.0f, -10.0f);
	registry.Get<Transform>(_creature).position = moved;
	_reactions.ProcessTurn();
	const auto reaction = _reactions.Find(id);
	ASSERT_TRUE(reaction.has_value());
	EXPECT_EQ(reaction->source.position, moved);
	const auto facing = creature_locomotion::DirectionOf(1.0f) * 10.0f;
	EXPECT_FLOAT_EQ(reaction->velocity.x, facing.x);
	EXPECT_FLOAT_EQ(reaction->velocity.y, 0.0f);
	EXPECT_FLOAT_EQ(reaction->velocity.z, facing.y);
}

namespace
{
/// A job a villager walks to, and the state it takes up again once a reaction is over
constexpr auto k_Job = VillagerStates::InspectObject;
constexpr auto k_JobAgain = VillagerStates::NothingToDo;

/// Makes the job a state villagers may react from, that resumes in another, plain state
void MakeJob(InfoConstants& info, VillagerStates resumesIn)
{
	auto& job = info.villagerStateTable.at(static_cast<size_t>(k_Job));
	job.isFinalState = 1;
	job.availableForReaction = 1;
	job.resumeState = static_cast<int>(resumesIn);
	// Reactions' states are ends in themselves
	info.villagerStateTable.at(static_cast<size_t>(VillagerStates::FleeingAndLookingAtObjectReaction)).isFinalState = 1;
	info.villagerStateTable.at(static_cast<size_t>(VillagerStates::LookingAtObjectReaction)).isFinalState = 1;
	info.villagerStateTable.at(static_cast<size_t>(k_JobAgain)).isFinalState = 1;
	Locator::infoConstants::emplace(info);
}

VillagerStates SlotOf(entt::entity villager, LivingAction::Index index)
{
	return static_cast<VillagerStates>(
	    Locator::entitiesRegistry::value().Get<const LivingAction>(villager).states.at(static_cast<size_t>(index)));
}

/// The villager walks to its job
void WalkToJob(entt::entity villager)
{
	auto& action = Locator::entitiesRegistry::value().Get<LivingAction>(villager);
	action.states.at(static_cast<size_t>(LivingAction::Index::Top)) = static_cast<uint8_t>(VillagerStates::MoveToPos);
	action.states.at(static_cast<size_t>(LivingAction::Index::Final)) = static_cast<uint8_t>(k_Job);
}
} // namespace

TEST_F(Reactions, AVillagerRemembersTheStateItWasHeadingForAndTakesUpWhereThatResumesOnceTheReactionIsOver)
{
	MakeJob(*_info, k_JobAgain);
	Locator::reactionSystem::emplace<ecs::systems::ReactionSystem>();
	auto& reactions = Locator::reactionSystem::value();
	const auto villager = _villagers[0];
	WalkToJob(villager);
	// What made the miracle stays, so the reaction lasts until the villager's time is over
	auto miracle = Miracle();
	miracle.initiator = Locator::entitiesRegistry::value().Create();
	reactions.Create(miracle);
	ASSERT_EQ(StateOf(villager), VillagerStates::LookingAtObjectReaction);
	// It remembers its job, the state it was walking to, not the walk
	EXPECT_EQ(SlotOf(villager, LivingAction::Index::Previous), k_Job);
	for (int turn = 0; turn < 100; ++turn)
	{
		reactions.ProcessTurn();
	}
	// Its time over, it takes up the state its job resumes in, remembers nothing, and waits its time from now before
	// taking the kind up again
	EXPECT_EQ(StateOf(villager), k_JobAgain);
	EXPECT_EQ(SlotOf(villager, LivingAction::Index::Previous), VillagerStates::InvalidState);
	const auto& registry = Locator::entitiesRegistry::value();
	EXPECT_EQ(registry.Get<const LivingReaction>(villager).reaction, 0u);
	EXPECT_GT(registry.Get<const VillagerReactionMemory>(villager).memory.LastReacted(
	              static_cast<uint32_t>(Reaction::LookAtNiceSpell)),
	          80u);
	Locator::reactionSystem::reset();
}

TEST_F(Reactions, AReactionsEndGoesBackTheSameWayWhenTheReactionIsShutDown)
{
	MakeJob(*_info, k_JobAgain);
	Locator::reactionSystem::emplace<ecs::systems::ReactionSystem>();
	auto& reactions = Locator::reactionSystem::value();
	const auto villager = _villagers[0];
	WalkToJob(villager);
	const auto id = reactions.Create(Miracle());
	ASSERT_EQ(StateOf(villager), VillagerStates::LookingAtObjectReaction);
	reactions.Remove(id);
	EXPECT_EQ(StateOf(villager), k_JobAgain);
	EXPECT_EQ(Locator::entitiesRegistry::value().Get<const LivingReaction>(villager).reaction, 0u);
	Locator::reactionSystem::reset();
}

TEST_F(Reactions, AVillagerWhoseRememberedStateResumesInAReactionDecidesWhatToDo)
{
	MakeJob(*_info, VillagerStates::FleeingAndLookingAtObjectReaction);
	Locator::reactionSystem::emplace<ecs::systems::ReactionSystem>();
	const auto villager = _villagers[0];
	WalkToJob(villager);
	const auto id = Locator::reactionSystem::value().Create(Miracle());
	Locator::reactionSystem::value().Remove(id);
	EXPECT_EQ(StateOf(villager), VillagerStates::DecideWhatToDo);
	Locator::reactionSystem::reset();
}

TEST_F(Reactions, AVillagerLeavingAReactionsStateForAnotherKindLetsGoOfTheReaction)
{
	MakeJob(*_info, k_JobAgain);
	Locator::reactionSystem::emplace<ecs::systems::ReactionSystem>();
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = _villagers[0];
	registry.Assign<WallHugObjectReference>(villager, WallHugObjectReference {.stepsAway = 3});
	const auto id = Locator::reactionSystem::value().Create(Miracle());
	ASSERT_NE(registry.Get<const LivingReaction>(villager).reaction, 0u);
	// Taken off to its job by something else, it lets go of the reaction and forgets the circle it was going round
	Locator::livingActionSystem::value().VillagerChangeTopState(registry.Get<LivingAction>(villager), k_Job);
	EXPECT_EQ(StateOf(villager), k_Job);
	EXPECT_EQ(registry.Get<const LivingReaction>(villager).reaction, 0u);
	EXPECT_FALSE(registry.AllOf<WallHugObjectReference>(villager));
	// So the reaction's end no longer moves it
	Locator::reactionSystem::value().Remove(id);
	EXPECT_EQ(StateOf(villager), k_Job);
	Locator::reactionSystem::reset();
}

TEST_F(Reactions, AVillagerWhoseStateWontLetItGoIsPutStraightIntoDecidingWhatToDo)
{
	MakeJob(*_info, k_JobAgain);
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = _villagers[0];
	auto& action = registry.Get<LivingAction>(villager);
	// A body is let out of death only into the hand or the air
	action.states.at(static_cast<size_t>(LivingAction::Index::Top)) = static_cast<uint8_t>(VillagerStates::Dead);
	action.states.at(static_cast<size_t>(LivingAction::Index::Previous)) = static_cast<uint8_t>(k_Job);
	ecs::systems::villager_reactions::StopReacting(action);
	EXPECT_EQ(StateOf(villager), VillagerStates::DecideWhatToDo);
	EXPECT_EQ(SlotOf(villager, LivingAction::Index::Previous), VillagerStates::InvalidState);
}

TEST_F(Reactions, AReactionVillagersDontActOutYetEndsWithoutMovingThem)
{
	MakeJob(*_info, k_JobAgain);
	auto& unacted = _info->reaction.at(static_cast<size_t>(Reaction::Fainting));
	unacted = _info->reaction.at(static_cast<size_t>(Reaction::LookAtNiceSpell));
	Locator::infoConstants::emplace(*_info);
	Locator::reactionSystem::emplace<ecs::systems::ReactionSystem>();
	const auto villager = _villagers[0];
	WalkToJob(villager);
	const auto id = Locator::reactionSystem::value().Create(Miracle(Reaction::Fainting));
	ASSERT_NE(Locator::entitiesRegistry::value().Get<const LivingReaction>(villager).reaction, 0u);
	Locator::reactionSystem::value().Remove(id);
	EXPECT_EQ(StateOf(villager), VillagerStates::MoveToPos);
	EXPECT_EQ(SlotOf(villager, LivingAction::Index::Final), k_Job);
	EXPECT_EQ(Locator::entitiesRegistry::value().Get<const LivingReaction>(villager).reaction, 0u);
	Locator::reactionSystem::reset();
}

TEST_F(Reactions, VillagersHeedTheHandPickingUpOrSomethingCrushedWithoutTakingItUpOrBelieving)
{
	auto& registry = Locator::entitiesRegistry::value();
	// A few turns in, so that what is remembered is later than nothing
	_reactions.ProcessTurn();
	_reactions.ProcessTurn();
	for (const auto type : {Reaction::ReactToHandPickUp, Reaction::ReactToObjectCrushed})
	{
		_reactions.Create(Miracle(type));
		const auto villager = _villagers[0];
		// Heeded: the kind is remembered, so it isn't heeded again straight away
		const auto* memory = registry.TryGet<const VillagerReactionMemory>(villager);
		ASSERT_NE(memory, nullptr);
		EXPECT_NE(memory->memory.LastReacted(static_cast<uint32_t>(type)), 0u);
		// But never held: it goes on with what it does and its town believes nothing
		const auto* state = registry.TryGet<const LivingReaction>(villager);
		EXPECT_TRUE(state == nullptr || state->reaction == 0);
		EXPECT_EQ(StateOf(villager), VillagerStates::DecideWhatToDo);
		const auto* impression = registry.TryGet<const TownImpression>(_town);
		EXPECT_TRUE(impression == nullptr ||
		            impression->belief.pending.at(static_cast<size_t>(PlayerNames::PLAYER_ONE)) == 0.0f);
	}
}

TEST(ReactionRules, OnlyATownsOwnBuildingsAreReactedToWhenBuilt)
{
	using magic::villager_reaction::ReactedToWhenBuilt;
	for (const auto type : {AbodeType::Totem, AbodeType::StoragePit, AbodeType::Creche, AbodeType::Workshop, AbodeType::Wonder,
	                        AbodeType::Graveyard, AbodeType::TownCentre, AbodeType::SpellDispenser})
	{
		EXPECT_TRUE(ReactedToWhenBuilt(type));
	}
	for (const auto type :
	     {AbodeType::Citadel, AbodeType::FootballPitch, AbodeType::LivingQuarters, AbodeType::Windmill, AbodeType::Field})
	{
		EXPECT_FALSE(ReactedToWhenBuilt(type));
	}
}

TEST_F(Reactions, VillagersDanceRoundANewBuildingUnimpressedAndGoBackWhenItsReactionEnds)
{
	auto& registry = Locator::entitiesRegistry::value();
	Locator::danceSystem::emplace<OneGroupDances>();
	auto& dances = static_cast<OneGroupDances&>(Locator::danceSystem::value());
	auto& table = _info->villagerStateTable.at(static_cast<size_t>(VillagerStates::DanceWhileReacting));
	table.availableForReaction = 1;
	table.isReactionState = 1;
	Locator::infoConstants::emplace(*_info);
	const auto building = registry.Create();
	registry.Assign<Transform>(building, k_Here, glm::mat3(1.0f), glm::vec3(1.0f));
	auto source = Miracle(Reaction::ReactToNewBuilding);
	source.initiator = building;
	Locator::reactionSystem::emplace<ecs::systems::ReactionSystem>();
	auto& reactions = Locator::reactionSystem::value();
	const auto id = reactions.Create(source);
	// One dance is made for the reaction, the small building's, and those in reach join it
	ASSERT_EQ(dances.made.size(), 1u);
	EXPECT_EQ(dances.made.front(), DanceInfo::BuildingSmall);
	const auto dance = reactions.Find(id)->dance;
	ASSERT_TRUE(dance != entt::null);
	for (const auto villager : {_villagers[0], _villagers[1]})
	{
		EXPECT_EQ(StateOf(villager), VillagerStates::DanceWhileReacting);
		EXPECT_EQ(ecs::dances::DanceOf(registry, villager), dance);
	}
	EXPECT_EQ(StateOf(_villagers[2]), VillagerStates::DecideWhatToDo);
	// Dancing, they aren't impressed
	const auto* impression = registry.TryGet<const TownImpression>(_town);
	EXPECT_TRUE(impression == nullptr || impression->belief.pending.at(static_cast<size_t>(PlayerNames::PLAYER_ONE)) == 0.0f);
	// The reaction over, they leave the dance and go back to what they did, and the dance goes
	reactions.Remove(id);
	for (const auto villager : {_villagers[0], _villagers[1]})
	{
		EXPECT_EQ(StateOf(villager), VillagerStates::DecideWhatToDo);
		EXPECT_FALSE(registry.AllOf<Dancer>(villager));
		EXPECT_EQ(registry.Get<const LivingReaction>(villager).reaction, 0u);
	}
	ASSERT_EQ(dances.ended.size(), 1u);
	EXPECT_EQ(dances.ended.front(), dance);
	Locator::reactionSystem::reset();
	Locator::danceSystem::reset();
}

TEST(ReactionRules, OnlyAnOrdinaryVillagerHeedsItsOwnTownCelebratingWithoutACentre)
{
	using magic::villager_reaction::CelebrationWatch;
	using magic::villager_reaction::TownCelebrationPriority;
	const CelebrationWatch heeds {.hasTown = true, .madeByOwnTown = true};
	EXPECT_EQ(TownCelebrationPriority(heeds, 90), 90u);
	// A town celebrating from its centre draws none of its people
	EXPECT_EQ(TownCelebrationPriority({.hasTown = true, .madeByOwnTown = false}, 90), 0u);
	EXPECT_EQ(TownCelebrationPriority({.disciple = true, .hasTown = true, .madeByOwnTown = true}, 90), 0u);
	EXPECT_EQ(TownCelebrationPriority({.dancing = true, .hasTown = true, .madeByOwnTown = true}, 90), 0u);
	EXPECT_EQ(TownCelebrationPriority({}, 90), 0u);
}

// A burning thing in the hand alarms a villager near it, or further off when it moves fast; one near it, or alarmed by a
// thing moving faster still, heeds it again at once
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(ReactionRules, ABurningThingInTheHandAlarmsThoseNearOrWhenItMovesFast)
{
	using magic::villager_reaction::BurningInHandAgainTurns;
	using magic::villager_reaction::BurningInHandPriority;
	EXPECT_EQ(BurningInHandPriority(10.0f, 10.0f, 0.0f, 120), 120u);
	EXPECT_EQ(BurningInHandPriority(10.5f, 10.0f, 2.9f, 120), 0u);
	EXPECT_EQ(BurningInHandPriority(40.0f, 10.0f, 3.0f, 120), 120u);
	EXPECT_EQ(BurningInHandAgainTurns(9.0f, 10.0f, 0.0f, 50), 0u);
	EXPECT_EQ(BurningInHandAgainTurns(10.0f, 10.0f, 3.0f, 50), 50u);
	EXPECT_EQ(BurningInHandAgainTurns(20.0f, 10.0f, 3.5f, 50), 0u);
}
