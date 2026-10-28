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
#include <chrono>
#include <memory>
#include <optional>
#include <vector>

#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/MapCoords.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureTeleportAgendas.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/Disciple.h"
#include "ECS/Components/Fire.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/LivingReaction.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/MiracleImpression.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/ReportedHand.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/TeleportStone.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Dances.h"
#include "ECS/PhysicsEntry.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Reflection/ComponentReflection.h"
#include "ECS/Registry.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/Systems/DanceSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/WallHugRules.h"
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
#include "ECS/Systems/Implementations/VillagerDisciples.h"
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
		// A material that keeps part of its spin: a zero share is not a material of the game
		entry.body = std::make_unique<physics::Body>(
		    physics::BodySetup {.scale = 1.0f, .halfHeight = 0.5f, .mass = 1.0f, .material = {.spinKeptPerSecond = 0.5f}},
		    cube);
		entry.body->velocity = velocity;
	}
	[[nodiscard]] PhysicsEntry* Find(entt::entity object) override { return object == entry.entity ? &entry : nullptr; }

	PhysicsEntry entry;
};
/// Dances of one shared group that takes anyone, made without files; remembers the dances it was asked to end
class OneGroupDances final: public ecs::systems::DanceSystemInterface
{
public:
	entt::entity Create(DanceInfo type, const glm::vec3&, entt::entity owner, uint32_t, bool, entt::entity) override
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
	uint32_t SyncedRand(uint32_t, DrawSite) override { return 0; }
	float SyncedFloatRand(float, DrawSite) override { return 0.0f; }
	uint32_t LocalRand(int32_t) override { return 0; }
	float LocalFloatRand(float) override { return 0.0f; }
	int32_t CrtRand() override { return 0; }
	void CrtSrand(uint32_t) override {}
	[[nodiscard]] GameRandomSeeds GetSeeds() const override { return {}; }
	void SetSeeds(GameRandomSeeds) override {}
	[[nodiscard]] ParticleRandomStream GetParticleStream() const override { return ParticleRandomStream::None; }
	void SetParticleStream(ParticleRandomStream) override {}
};
/// Records how the reaction system ends the reaction of an animal. It does nothing else.
class FakeAnimals final: public ecs::systems::AnimalSystemInterface
{
public:
	void ProcessTurn() override {}
	bool BeginTurn() override { return true; }
	void AnimalTurn(entt::entity) override {}
	void EndTurn() override {}
	void Update(std::chrono::milliseconds, float) override {}
	void ShowTurnEnd() override {}
	void Reset() override {}
	entt::entity CreateFlock(glm::vec2, float, float) override { return entt::null; }
	entt::entity CreateSpellAnimal(AnimalInfo, glm::vec2, float, uint16_t, PlayerNames, entt::entity, entt::entity) override
	{
		return entt::null;
	}
	entt::entity CreateScriptFlock(int32_t, glm::vec2, glm::vec2, float, float) override { return entt::null; }
	[[nodiscard]] entt::entity FindScriptFlock(int32_t) const override { return entt::null; }
	entt::entity CreateBird(AnimalInfo, glm::vec2, uint32_t, entt::entity) override { return entt::null; }
	void SetFlockTown(entt::entity, entt::entity) override {}
	entt::entity CreateGrazer(AnimalInfo, glm::vec2, uint32_t, entt::entity, entt::entity) override { return entt::null; }
	entt::entity CreateScriptAnimal(AnimalInfo, glm::vec2) override { return entt::null; }
	entt::entity CreateStillAnimal(AnimalInfo, glm::vec3, float, float) override { return entt::null; }
	void JoinFlock(entt::entity, entt::entity) override {}
	bool SetScriptState(entt::entity, LivingStates) override { return false; }
	void ScriptMoveTo(entt::entity, glm::vec2) override {}
	[[nodiscard]] std::optional<uint8_t> FinalGameStateOf(entt::entity) const override { return std::nullopt; }
	void SetScale(entt::entity, float) override {}
	[[nodiscard]] float RadiusOf(entt::entity) const override { return 0.0f; }
	[[nodiscard]] glm::vec3 MovementOf(entt::entity) const override { return glm::vec3(0.0f); }
	void SetFlockCentre(entt::entity, glm::vec2) override {}
	void MoveFlockPlace(entt::entity, glm::ivec2) override {}
	entt::entity SeparateIntoOwnFlock(entt::entity) override { return entt::null; }
	void SendToSlaughter(entt::entity, glm::vec2) override {}
	[[nodiscard]] uint16_t SpeedOf(entt::entity) const override { return 0; }
	[[nodiscard]] entt::entity LeaderOf(entt::entity) const override { return entt::null; }
	[[nodiscard]] std::vector<entt::entity> MembersOf(entt::entity) const override { return {}; }
	[[nodiscard]] glm::vec2 GoalOf(entt::entity) const override { return glm::vec2(0.0f); }
	[[nodiscard]] float GoalHeightOf(entt::entity) const override { return 0.0f; }
	void SendBird(entt::entity, glm::vec2, float, bool) override {}
	void SendWolf(entt::entity, glm::vec2, glm::vec2, float) override {}
	void StartFading(entt::entity) override {}
	void KillByEffect(entt::entity, glm::vec3) override {}
	void SetDying(entt::entity) override {}
	[[nodiscard]] bool IsFrighteningToCreature(entt::entity) const override { return false; }
	[[nodiscard]] bool CanPlayerPickUp(entt::entity) const override { return false; }
	[[nodiscard]] std::optional<bool> BirdCanBePickedUpByCreature(entt::entity) const override { return std::nullopt; }
	[[nodiscard]] bool CanBeStompedOnByCreature(entt::entity, float) const override { return false; }
	void StopReaction(entt::entity, bool resetState) override { stops.push_back(resetState); }
	[[nodiscard]] bool IsAvailableForReaction(entt::entity) const override { return available; }
	bool SetupLookAtObject(entt::entity, entt::entity object) override
	{
		lookedAt.push_back(object);
		return true;
	}
	bool SetupGotoFood(entt::entity, entt::entity food) override
	{
		wentTo.push_back(food);
		return true;
	}
	[[nodiscard]] uint32_t ReactToFoodPriority(entt::entity, entt::entity, uint32_t tablePriority) const override
	{
		return foodInterests ? tablePriority : 0;
	}

	/// For each end of a reaction, whether the animal goes into the state after the reaction
	std::vector<bool> stops;
	/// Whether the animal may take up a reaction, and whether food interests it
	bool available {false};
	bool foodInterests {false};
	/// The things that the animal looked at and the food that it went to
	std::vector<entt::entity> lookedAt;
	std::vector<entt::entity> wentTo;
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
		// The fire reaction has a reach, as every reaction of the game's data: a zero reach divides by zero
		auto& fireRow = _info->reaction.at(static_cast<size_t>(Reaction::ReactToFire));
		fireRow.maxReactionDistance = 60.0f;
		fireRow.howImportantIsDistance = 0.25f;
		for (const auto type : {Reaction::LookAtNiceSpell, Reaction::FleeFromSpell, Reaction::ReactToMagicShield,
		                        Reaction::FleeFromObject, Reaction::ReactToHandPickUp, Reaction::ReactToObjectCrushed,
		                        Reaction::ReactToNewBuilding, Reaction::LookAtObject, Reaction::ReactToFood})
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
			// The town needs share divides by this: zero is not a value of the game's data
			villager.divisorForTownNeedsSpeedMod = 1.0f;
			// What a villager carries is a share of its most: a zero most divides by zero
			villager.maxFoodCarried = villager.maxTraderFoodCarried = 10;
			villager.maxWoodCarried = villager.maxTraderWoodCarried = 10;
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
		// The town's statistics count its three people: two adults and a child
		registry.Assign<TownStats>(_town, TownStats {.adults = 2, .children = 1});
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
		Locator::reactionSystem::reset();
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

	/// One game turn as the game plays it: the turn count moves on, each living thing lets go of a reaction that is over
	/// in its own turn, then the reactions take their turn
	void PlayTurn()
	{
		_reactions.StartTurn();
		std::vector<entt::entity> living;
		Locator::entitiesRegistry::value().Each<const LivingReaction>(
		    [&living](entt::entity entity, const LivingReaction&) { living.push_back(entity); });
		for (const auto entity : living)
		{
			_reactions.EndReactionIfOver(entity);
		}
		_reactions.ProcessTurn();
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

TEST(ReactionRules, AFightWatcherGoesBackAtOnceWhenTheFightReactionEnds)
{
	// The end of the fight's reaction sends each watcher back to what it did before, as with the other kinds
	EXPECT_TRUE(ecs::systems::villager_reactions::ActsOut(Reaction::ReactToFight));
	// A reaction that a villager only holds is let go
	EXPECT_FALSE(ecs::systems::villager_reactions::ActsOut(Reaction::ReactToHandPickUp));
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
	PlayTurn();
	// The town's turn of belief comes in the turn of its player
	_reactions.BelieveInTown(_town);
	const auto& town = registry.Get<const TownImpression>(_town).belief;
	// Believed at the town's turn, no more than its cap
	EXPECT_FLOAT_EQ(town.belief.at(static_cast<size_t>(PlayerNames::PLAYER_ONE)),
	                std::min(belief, openblack::magic::town_belief::k_DefaultCap));
	EXPECT_FLOAT_EQ(town.pending.at(static_cast<size_t>(PlayerNames::PLAYER_ONE)), 0.0f);
}

TEST_F(Reactions, TheReactionsGoingOnComeBackFromASave)
{
	namespace reflection = openblack::ecs::reflection;
	entt::meta_ctx context;
	reflection::RegisterComponents(context);
	const auto id = _reactions.Create(Miracle());
	ASSERT_TRUE(_reactions.Find(id).has_value());
	reflection::BinaryWriter out(context);
	_reactions.SaveState(out);
	// Every value of the reactions goes in, with the things that react to each
	EXPECT_TRUE(out.Unsaved().empty());
	ecs::systems::ReactionSystem loaded;
	const reflection::EntityRemap identity;
	reflection::BinaryReader in(context, out.Bytes(), identity);
	ASSERT_TRUE(loaded.LoadState(in));
	EXPECT_EQ(in.Remaining(), 0u);
	reflection::BinaryWriter again(context);
	loaded.SaveState(again);
	EXPECT_EQ(again.Bytes(), out.Bytes());
	ASSERT_TRUE(loaded.Find(id).has_value());
	EXPECT_EQ(loaded.Find(id)->age, _reactions.Find(id)->age);
	EXPECT_EQ(loaded.Find(id)->source.type, _reactions.Find(id)->source.type);
}

TEST_F(Reactions, AVillagerTooWeakToWalkTakesUpNoMiracle)
{
	for (auto& villager : _info->villager)
	{
		villager.lifeWhenCrawlsWounded = 0.15f;
	}
	Locator::infoConstants::emplace(*_info);
	auto& registry = Locator::entitiesRegistry::value();
	// The one right by the miracle crawls, the next only limps
	registry.Get<Villager>(_villagers[0]).life = 0.15f;
	registry.Get<Villager>(_villagers[1]).life = 0.2f;
	_reactions.Create(Miracle());
	EXPECT_EQ(StateOf(_villagers[0]), VillagerStates::DecideWhatToDo);
	EXPECT_EQ(StateOf(_villagers[1]), VillagerStates::LookingAtObjectReaction);
}

TEST_F(Reactions, AVillagerCountedAtWorshipTakesUpNoMiracle)
{
	auto& registry = Locator::entitiesRegistry::value();
	// The one right by the miracle is counted at worship; the next is only on its way to the site
	registry.Assign<Worshipper>(_villagers[0], Worshipper {.atWorship = true});
	registry.Assign<Worshipper>(_villagers[1], Worshipper {.onTheWay = true});
	_reactions.Create(Miracle());
	EXPECT_EQ(StateOf(_villagers[0]), VillagerStates::DecideWhatToDo);
	const auto* held = registry.TryGet<const LivingReaction>(_villagers[0]);
	EXPECT_TRUE(held == nullptr || held->reaction == 0u);
	EXPECT_EQ(StateOf(_villagers[1]), VillagerStates::LookingAtObjectReaction);
}

TEST_F(Reactions, VillagersStopAfterTheirTimeAndGoBack)
{
	_reactions.Create(Miracle());
	for (int turn = 0; turn < 100; ++turn)
	{
		PlayTurn();
	}
	EXPECT_EQ(StateOf(_villagers[0]), VillagerStates::DecideWhatToDo);
	EXPECT_EQ(Locator::entitiesRegistry::value().Get<const LivingReaction>(_villagers[0]).reaction, 0u);
}

TEST_F(Reactions, AVillagerLettingGoOfAReactionKeepsWhatItDoesAndWaitsItsTimeFromThen)
{
	for (int turn = 0; turn < 5; ++turn)
	{
		PlayTurn();
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

TEST_F(Reactions, ACreatureWaitsItsTimeFromTheEndOfItsReaction)
{
	for (int turn = 0; turn < 5; ++turn)
	{
		PlayTurn();
	}
	_reactions.Create(Miracle());
	auto& registry = Locator::entitiesRegistry::value();
	ASSERT_NE(registry.Get<const LivingReaction>(_creature).reaction, 0u);
	const auto takenUp = _reactions.CurrentTurn();
	for (int turn = 0; turn < 100 && registry.Get<const LivingReaction>(_creature).reaction != 0; ++turn)
	{
		PlayTurn();
	}
	ASSERT_EQ(registry.Get<const LivingReaction>(_creature).reaction, 0u);
	const auto& memory = registry.Get<const LivingReaction>(_creature).creatureMemory;
	const auto kind = static_cast<uint32_t>(Reaction::LookAtNiceSpell);
	// The memory of the kind comes up to the turn when the reaction ended, after the turn when the creature took it up
	EXPECT_GT(memory.LastReacted(kind), takenUp);
	EXPECT_EQ(memory.LastReacted(kind), _reactions.CurrentTurn());
}

TEST_F(Reactions, AVillagerWaitsItsTimeFromTheEndOfItsReaction)
{
	_reactions.Create(Miracle());
	auto& registry = Locator::entitiesRegistry::value();
	ASSERT_NE(registry.Get<const LivingReaction>(_villagers[0]).reaction, 0u);
	uint32_t turns = 0;
	while (registry.Get<const LivingReaction>(_villagers[0]).reaction != 0 && turns < 200)
	{
		PlayTurn();
		++turns;
	}
	ASSERT_EQ(registry.Get<const LivingReaction>(_villagers[0]).reaction, 0u);
	// The villager remembers the kind from the turn that the reaction stopped, not from the turn that it started
	const auto& memory = registry.Get<const VillagerReactionMemory>(_villagers[0]).memory;
	EXPECT_EQ(memory.LastReacted(static_cast<uint32_t>(Reaction::LookAtNiceSpell)), turns);
}

TEST_F(Reactions, AnAnimalWaitsItsTimeFromTheEndOfAReactionThatIsGone)
{
	auto& registry = Locator::entitiesRegistry::value();
	for (int turn = 0; turn < 4; ++turn)
	{
		PlayTurn();
	}
	const auto animal = registry.Create();
	registry.Assign<Transform>(animal, k_Here, glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Animal>(animal);
	auto& state = registry.Assign<LivingReaction>(animal, LivingReaction {.reaction = 999, .type = Reaction::LookAtNiceSpell});
	state.creatureMemory.Record(static_cast<uint32_t>(Reaction::LookAtNiceSpell), 1);
	// The reaction does not exist, so the animal stops it on the next turn
	PlayTurn();
	const auto& after = registry.Get<const LivingReaction>(animal);
	EXPECT_EQ(after.reaction, 0u);
	EXPECT_EQ(after.creatureMemory.LastReacted(static_cast<uint32_t>(Reaction::LookAtNiceSpell)), 5u);
}

TEST_F(Reactions, AnAnimalWhoseReactionIsGoneStaysInItsState)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& animals = static_cast<FakeAnimals&>(Locator::animalSystem::emplace<FakeAnimals>());
	const auto animal = registry.Create();
	registry.Assign<Transform>(animal, k_Here, glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Animal>(animal);
	registry.Assign<LivingReaction>(animal, LivingReaction {.reaction = 999, .type = Reaction::FleeFromObject});
	// The reaction does not exist, so the animal lets go of it and keeps its state
	PlayTurn();
	EXPECT_EQ(animals.stops, std::vector<bool> {false});
	Locator::animalSystem::reset();
}

TEST_F(Reactions, AnAnimalWhoseReactionShutsDownGoesIntoTheStateAfterTheReaction)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& animals = static_cast<FakeAnimals&>(Locator::animalSystem::emplace<FakeAnimals>());
	const auto id = _reactions.Create(Miracle());
	const auto animal = registry.Create();
	registry.Assign<Transform>(animal, k_Here, glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Animal>(animal);
	registry.Assign<LivingReaction>(animal, LivingReaction {.reaction = id, .type = Reaction::LookAtNiceSpell});
	_reactions.Remove(id);
	EXPECT_EQ(animals.stops, std::vector<bool> {true});
	EXPECT_EQ(registry.Get<const LivingReaction>(animal).reaction, 0u);
	Locator::animalSystem::reset();
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
	PlayTurn();
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

// A creature a script has told to take no notice, one a script controls, and one with a forced plan that the leash does
// not lead take up no reaction, so aren't impressed by one
TEST_F(Reactions, ACreatureToldToTakeNoNoticeOrBusyWithAForcedPlanIsNotImpressed)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto impressed = [&]() {
		const auto* impression = registry.TryGet<const CreatureImpression>(_creature);
		return impression != nullptr && impression->byOwnPlayer > 0.0f;
	};
	registry.Assign<CreatureReactionsOff>(_creature);
	_reactions.Create(Miracle());
	EXPECT_FALSE(impressed());
	registry.Remove<CreatureReactionsOff>(_creature);
	registry.Assign<CreatureMindState>(_creature).planForced = true;
	_reactions.Create(Miracle());
	EXPECT_FALSE(impressed());
	// An order on the leash is forced, but the leash leads it
	registry.Get<CreatureMindState>(_creature).ledByLeash = true;
	_reactions.Create(Miracle());
	EXPECT_TRUE(impressed());
	registry.Remove<CreatureMindState>(_creature);
}

TEST_F(Reactions, ACreatureWithNoLifeOrCarriedByATeleportIsNotImpressed)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto impressed = [&]() {
		const auto* impression = registry.TryGet<const CreatureImpression>(_creature);
		return impression != nullptr && impression->byOwnPlayer > 0.0f;
	};
	registry.Assign<CreatureNeeds>(_creature).needs.life = 0.0f;
	_reactions.Create(Miracle());
	EXPECT_FALSE(impressed());
	registry.Remove<CreatureNeeds>(_creature);
	// A stone controls the creature while its plan has a step of the teleport left
	registry.Assign<TeleportTraveller>(_creature);
	auto& mind = registry.Assign<CreatureMindState>(_creature);
	mind.idle.agenda = openblack::creature_mind::AfterTeleport(std::nullopt);
	_reactions.Create(Miracle());
	EXPECT_FALSE(impressed());
	// Once its plan has no step of the teleport left, the stone no longer controls it, at once
	registry.Get<CreatureMindState>(_creature).idle.step = 2;
	_reactions.Create(Miracle());
	EXPECT_TRUE(impressed());
	registry.Remove<TeleportTraveller>(_creature);
	registry.Remove<CreatureMindState>(_creature);
	_reactions.Create(Miracle());
	EXPECT_TRUE(impressed());
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
	PlayTurn();
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
	PlayTurn();
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
	PlayTurn();
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
	PlayTurn();
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
	locomotion.heading = GameHeading(1.0f);
	const auto id = _reactions.Create({.initiator = _creature,
	                                   .type = Reaction::FleeFromObject,
	                                   .player = PlayerNames::NEUTRAL,
	                                   .position = k_Here,
	                                   .playerless = true});
	const glm::vec3 moved = k_Here + glm::vec3(20.0f, 0.0f, -10.0f);
	registry.Get<Transform>(_creature).position = moved;
	PlayTurn();
	const auto reaction = _reactions.Find(id);
	ASSERT_TRUE(reaction.has_value());
	EXPECT_EQ(reaction->source.position, moved);
	const auto facing = creature_locomotion::DirectionOf(GameHeading(1.0f)) * 10.0f;
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
		reactions.StartTurn();
		reactions.EndReactionIfOver(villager);
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

// A villager lets go of its reaction at the start of its own turn, as soon as the thing that it reacts to is gone. The
// reactions' own turn is not necessary for this. A villager that holds no reaction stays as it is.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(Reactions, AVillagerLetsGoOfItsReactionInItsOwnTurnOnceWhatItReactsToIsGone)
{
	MakeJob(*_info, k_JobAgain);
	Locator::reactionSystem::emplace<ecs::systems::ReactionSystem>();
	auto& reactions = Locator::reactionSystem::value();
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = _villagers[0];
	const auto far = _villagers[2];
	WalkToJob(villager);
	auto miracle = Miracle();
	miracle.initiator = registry.Create();
	reactions.Create(miracle);
	ASSERT_EQ(StateOf(villager), VillagerStates::LookingAtObjectReaction);
	const auto farState = StateOf(far);

	reactions.StartTurn();
	reactions.EndReactionIfOver(villager);
	EXPECT_NE(registry.Get<const LivingReaction>(villager).reaction, 0u);

	registry.Destroy(miracle.initiator);
	reactions.EndReactionIfOver(villager);
	reactions.EndReactionIfOver(far);
	EXPECT_EQ(registry.Get<const LivingReaction>(villager).reaction, 0u);
	EXPECT_EQ(StateOf(villager), k_JobAgain);
	EXPECT_EQ(StateOf(far), farState);
	Locator::reactionSystem::reset();
}

// A villager reacting to a thing goes back to what it was doing once the hand holds the thing, when its kind of
// reaction ends then
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(Reactions, AVillagerStopsReactingToAThingTheHandHoldsWhenItsKindEndsThen)
{
	_info->reaction.at(static_cast<size_t>(Reaction::LookAtNiceSpell)).whetherReactionFinishesIfInitiatorInHand = 1;
	MakeJob(*_info, k_JobAgain);
	Locator::reactionSystem::emplace<ecs::systems::ReactionSystem>();
	auto& reactions = Locator::reactionSystem::value();
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = _villagers[0];
	WalkToJob(villager);
	auto miracle = Miracle();
	miracle.initiator = registry.Create();
	reactions.Create(miracle);
	ASSERT_EQ(StateOf(villager), VillagerStates::LookingAtObjectReaction);
	auto& action = registry.Get<LivingAction>(villager);
	// Not yet in the hand: nothing changes
	ecs::systems::villager_reactions::ReactionValidate(action);
	EXPECT_EQ(StateOf(villager), VillagerStates::LookingAtObjectReaction);
	registry.Assign<InHand>(miracle.initiator);
	ecs::systems::villager_reactions::ReactionValidate(action);
	EXPECT_EQ(StateOf(villager), k_JobAgain);
	EXPECT_EQ(registry.Get<const LivingReaction>(villager).reaction, 0u);
	Locator::reactionSystem::reset();
}

// When its kind of reaction does not end then, the villager keeps reacting to the thing in the hand
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(Reactions, AVillagerKeepsReactingToAThingTheHandHoldsWhenItsKindDoesNotEndThen)
{
	_info->reaction.at(static_cast<size_t>(Reaction::LookAtNiceSpell)).whetherReactionFinishesIfInitiatorInHand = 0;
	MakeJob(*_info, k_JobAgain);
	Locator::reactionSystem::emplace<ecs::systems::ReactionSystem>();
	auto& reactions = Locator::reactionSystem::value();
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = _villagers[0];
	WalkToJob(villager);
	auto miracle = Miracle();
	miracle.initiator = registry.Create();
	reactions.Create(miracle);
	ASSERT_EQ(StateOf(villager), VillagerStates::LookingAtObjectReaction);
	auto& action = registry.Get<LivingAction>(villager);
	// Not yet in the hand: nothing changes
	ecs::systems::villager_reactions::ReactionValidate(action);
	EXPECT_EQ(StateOf(villager), VillagerStates::LookingAtObjectReaction);
	registry.Assign<InHand>(miracle.initiator);
	ecs::systems::villager_reactions::ReactionValidate(action);
	EXPECT_EQ(StateOf(villager), VillagerStates::LookingAtObjectReaction);
	EXPECT_NE(registry.Get<const LivingReaction>(villager).reaction, 0u);
	Locator::reactionSystem::reset();
}

// A villager that reacts to a thing goes back to what it did before once the thing is gone
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(Reactions, AVillagerStopsReactingWhenTheThingItReactsToIsGone)
{
	MakeJob(*_info, k_JobAgain);
	Locator::reactionSystem::emplace<ecs::systems::ReactionSystem>();
	auto& reactions = Locator::reactionSystem::value();
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = _villagers[0];
	WalkToJob(villager);
	auto miracle = Miracle();
	miracle.initiator = registry.Create();
	reactions.Create(miracle);
	ASSERT_EQ(StateOf(villager), VillagerStates::LookingAtObjectReaction);
	auto& action = registry.Get<LivingAction>(villager);
	// The thing is there: nothing changes
	ecs::systems::villager_reactions::ReactionValidate(action);
	EXPECT_EQ(StateOf(villager), VillagerStates::LookingAtObjectReaction);
	registry.Destroy(miracle.initiator);
	ecs::systems::villager_reactions::ReactionValidate(action);
	EXPECT_EQ(StateOf(villager), k_JobAgain);
	EXPECT_EQ(registry.Get<const LivingReaction>(villager).reaction, 0u);
	Locator::reactionSystem::reset();
}

// A villager is free for a new task only when no script and no hand holds it, and its state lets it go
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(Reactions, AVillagerIsFreeForANewTaskOnlyWhenNothingHoldsItAndItsStateLetsItGo)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = _villagers[0];
	const auto state = Locator::livingActionSystem::value().VillagerGetFinalState(registry.Get<const LivingAction>(villager));
	// The locator holds its own copy of the tables: give it the changed tables each time
	const auto stateLetsGo = [this, state](int lets) {
		_info->villagerStateTable.at(static_cast<size_t>(state)).field0xa8 = lets;
		Locator::infoConstants::emplace(*_info);
	};
	stateLetsGo(1);
	EXPECT_TRUE(ecs::systems::villager_reactions::FreeForNewTask(villager));
	stateLetsGo(0);
	EXPECT_FALSE(ecs::systems::villager_reactions::FreeForNewTask(villager));
	stateLetsGo(1);
	registry.Assign<InHand>(villager);
	EXPECT_FALSE(ecs::systems::villager_reactions::FreeForNewTask(villager));
	registry.Remove<InHand>(villager);
	registry.Assign<ScriptControlled>(villager);
	EXPECT_FALSE(ecs::systems::villager_reactions::FreeForNewTask(villager));
	registry.Remove<ScriptControlled>(villager);
	EXPECT_TRUE(ecs::systems::villager_reactions::FreeForNewTask(villager));
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
	auto& unacted = _info->reaction.at(static_cast<size_t>(Reaction::Confused));
	unacted = _info->reaction.at(static_cast<size_t>(Reaction::LookAtNiceSpell));
	Locator::infoConstants::emplace(*_info);
	Locator::reactionSystem::emplace<ecs::systems::ReactionSystem>();
	const auto villager = _villagers[0];
	WalkToJob(villager);
	const auto id = Locator::reactionSystem::value().Create(Miracle(Reaction::Confused));
	ASSERT_NE(Locator::entitiesRegistry::value().Get<const LivingReaction>(villager).reaction, 0u);
	Locator::reactionSystem::value().Remove(id);
	EXPECT_EQ(StateOf(villager), VillagerStates::MoveToPos);
	EXPECT_EQ(SlotOf(villager, LivingAction::Index::Final), k_Job);
	EXPECT_EQ(Locator::entitiesRegistry::value().Get<const LivingReaction>(villager).reaction, 0u);
	Locator::reactionSystem::reset();
}

namespace
{
/// Gives every kind of villager the same made-up speeds: a walk, a run three times as fast, and loads that slow it
void GiveSpeeds(InfoConstants& info)
{
	for (auto& villager : info.villager)
	{
		villager.speedGroup = {.speedDefault = SpeedState {1000},
		                       .speedFleeing = SpeedState {3000},
		                       .speed2 = SpeedState {1000},
		                       .speed3 = SpeedState {1000},
		                       .speed4 = SpeedState {500},
		                       .speed5 = SpeedState {2500}};
		villager.maxWoodCarried = 100;
		villager.maxTraderWoodCarried = 200;
		villager.maxFoodCarried = 100;
		villager.maxTraderFoodCarried = 200;
		villager.speedModWhenFullLoadOfWood = 0.75f;
		villager.speedModWhenFullLoadOfFood = 0.85f;
		villager.foodPowerupIncrease = 1.0f;
	}
	info.villagerStateTable.at(static_cast<size_t>(VillagerStates::FleeingAndLookingAtObjectReaction)).speedIndex = 1;
}

[[nodiscard]] int32_t WholeSpeedOf(entt::entity villager)
{
	return ecs::wall_hug::WholeSpeed(Locator::entitiesRegistry::value().Get<const WallHug>(villager).speed);
}
} // namespace

TEST_F(Reactions, AVillagerWalksAtTheSpeedOfTheStateItIsWalkingTo)
{
	GiveSpeeds(*_info);
	Locator::infoConstants::emplace(*_info);
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = _villagers[0];
	auto& action = registry.Get<LivingAction>(villager);
	// Walking to decide what to do it walks; walking to watch something it runs, three times as fast
	ASSERT_TRUE(villager_home::SetupMoveTo(action, glm::vec2(600.0f, 600.0f), VillagerStates::DecideWhatToDo));
	const auto walk = WholeSpeedOf(villager);
	ASSERT_GT(walk, 0);
	ASSERT_TRUE(
	    villager_home::SetupMoveTo(action, glm::vec2(600.0f, 600.0f), VillagerStates::FleeingAndLookingAtObjectReaction));
	EXPECT_EQ(StateOf(villager), VillagerStates::MoveToPos);
	EXPECT_NEAR(static_cast<float>(WholeSpeedOf(villager)), static_cast<float>(walk) * 3.0f, 3.0f);
}

TEST_F(Reactions, ALoadSlowsAVillagerByHowFullItIsForItsKindOfWork)
{
	GiveSpeeds(*_info);
	Locator::infoConstants::emplace(*_info);
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = _villagers[0];
	auto& action = registry.Get<LivingAction>(villager);
	const auto walk = [&] {
		villager_home::SetupMoveTo(action, glm::vec2(600.0f, 600.0f), VillagerStates::DecideWhatToDo);
		return static_cast<float>(WholeSpeedOf(villager));
	};
	const float unladen = walk();
	// A full load of wood slows it to three quarters, a full load of food to 85%
	registry.Get<Villager>(villager).woodHeld = 100;
	EXPECT_NEAR(walk(), unladen * 0.75f, 2.0f);
	registry.Get<Villager>(villager).woodHeld = 0;
	registry.Get<Villager>(villager).foodHeld = 100;
	EXPECT_NEAR(walk(), unladen * 0.85f, 2.0f);
	// A trader carries twice as much, so the same food is only half a load and doesn't slow it
	registry.Assign<Disciple>(villager).type = VillagerDisciple::Trader;
	EXPECT_NEAR(walk(), unladen, 1.0f);
}

TEST_F(Reactions, AVillagerLettingGoOfAReactionKeepsItsSpeed)
{
	GiveSpeeds(*_info);
	Locator::infoConstants::emplace(*_info);
	_reactions.Create(Miracle(Reaction::FleeFromSpell));
	auto& registry = Locator::entitiesRegistry::value();
	ASSERT_NE(registry.Get<const LivingReaction>(_villagers[0]).reaction, 0u);
	auto& wallHug = registry.Get<WallHug>(_villagers[0]);
	wallHug.speed = GetSpeedStateSpeed(SpeedState {2000});
	const auto running = WholeSpeedOf(_villagers[0]);
	_reactions.StopReacting(_villagers[0]);
	// It runs on at the speed it had until its state next changes
	EXPECT_EQ(WholeSpeedOf(_villagers[0]), running);
}

TEST_F(Reactions, VillagersHeedTheHandPickingUpOrSomethingCrushedWithoutTakingItUpOrBelieving)
{
	auto& registry = Locator::entitiesRegistry::value();
	// A few turns in, so that what is remembered is later than nothing
	PlayTurn();
	PlayTurn();
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

// The turn finds the hand where the last report of its player put it. The drawn hand follows the pointer of one
// computer only, so the turn does not use it.
TEST_F(Reactions, TheTurnFindsTheHandAtItsReportedPlaceAndNotAtItsDrawnPlace)
{
	auto& registry = Locator::entitiesRegistry::value();
	PlayTurn();
	PlayTurn();
	const auto hand = registry.Create();
	registry.Assign<Transform>(hand, k_Here + glm::vec3(1000.0f, 0.0f, 0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<ReportedHand>(hand).handPoint = k_Here;
	auto source = Miracle(Reaction::ReactToHandPickUp);
	source.initiator = hand;
	_reactions.Create(source);
	const auto* memory = registry.TryGet<const VillagerReactionMemory>(_villagers[0]);
	ASSERT_NE(memory, nullptr);
	EXPECT_NE(memory->memory.LastReacted(static_cast<uint32_t>(Reaction::ReactToHandPickUp)), 0u);
	const auto* far = registry.TryGet<const VillagerReactionMemory>(_villagers[2]);
	EXPECT_TRUE(far == nullptr || far->memory.LastReacted(static_cast<uint32_t>(Reaction::ReactToHandPickUp)) == 0u);
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

TEST_F(Reactions, ATownCelebratingWithoutACentreMakesOneEmptyDanceAndItsPeopleLook)
{
	auto& registry = Locator::entitiesRegistry::value();
	Locator::danceSystem::emplace<OneGroupDances>();
	auto& dances = static_cast<OneGroupDances&>(Locator::danceSystem::value());
	_info->reaction.at(static_cast<size_t>(Reaction::ReactToTownCelebration)) =
	    _info->reaction.at(static_cast<size_t>(Reaction::LookAtNiceSpell));
	Locator::infoConstants::emplace(*_info);
	registry.Assign<Transform>(_town, k_Here, glm::mat3(1.0f), glm::vec3(1.0f));
	auto source = Miracle(Reaction::ReactToTownCelebration);
	source.initiator = _town;
	Locator::reactionSystem::emplace<ecs::systems::ReactionSystem>();
	auto& reactions = Locator::reactionSystem::value();
	const auto id = reactions.Create(source);
	// One dance about the town is made for the reaction, the dance about a person, with no owner
	ASSERT_EQ(dances.made.size(), 1u);
	EXPECT_EQ(dances.made.front(), DanceInfo::DanceAroundPerson);
	const auto dance = reactions.Find(id)->dance;
	ASSERT_TRUE(dance != entt::null);
	EXPECT_TRUE(registry.Get<const Dance>(dance).owner == entt::null);
	// The town's people in reach look at the town and do not join the dance
	for (const auto villager : {_villagers[0], _villagers[1]})
	{
		EXPECT_EQ(StateOf(villager), VillagerStates::LookingAtObjectReaction);
		EXPECT_FALSE(registry.AllOf<Dancer>(villager));
	}
	// The dance goes with the reaction
	reactions.Remove(id);
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

// A villager looks at a thing that came to rest, a miracle to look at or a scaffold put up for a building, as at a nice
// miracle
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(Reactions, VillagersLookAtAThingThatCameToRestOrAScaffold)
{
	for (const auto type : {Reaction::LookAtObject, Reaction::LookAtSpell, Reaction::ReactToScaffold})
	{
		_info->reaction.at(static_cast<size_t>(type)) = _info->reaction.at(static_cast<size_t>(Reaction::LookAtNiceSpell));
	}
	Locator::infoConstants::emplace(*_info);
	for (const auto type : {Reaction::LookAtObject, Reaction::LookAtSpell, Reaction::ReactToScaffold})
	{
		const auto id = _reactions.Create(Miracle(type));
		EXPECT_EQ(StateOf(_villagers[0]), VillagerStates::LookingAtObjectReaction);
		EXPECT_EQ(Locator::entitiesRegistry::value().Get<const LivingReaction>(_villagers[0]).type, type);
		_reactions.Remove(id);
	}
}

TEST_F(Reactions, VillagersFaintWhereTheyStandUntilTheStinkIsOver)
{
	_info->reaction.at(static_cast<size_t>(Reaction::Fainting)) =
	    _info->reaction.at(static_cast<size_t>(Reaction::LookAtNiceSpell));
	auto& fainting = _info->villagerStateTable.at(static_cast<size_t>(VillagerStates::FaintingReaction));
	fainting.availableForReaction = 1;
	fainting.isReactionState = 1;
	Locator::infoConstants::emplace(*_info);
	// The creature that did the fart
	auto& registry = Locator::entitiesRegistry::value();
	const auto creature = registry.Create();
	registry.Assign<Transform>(creature, k_Here, glm::mat3(1.0f), glm::vec3(1.0f));
	auto source = Miracle(Reaction::Fainting);
	source.initiator = creature;
	const auto id = _reactions.Create(source);
	EXPECT_EQ(StateOf(_villagers[0]), VillagerStates::FaintingReaction);
	EXPECT_EQ(registry.Get<const LivingReaction>(_villagers[0]).type, Reaction::Fainting);
	// The villager stays in the faint state while the reaction continues
	PlayTurn();
	EXPECT_EQ(StateOf(_villagers[0]), VillagerStates::FaintingReaction);
	// At the end of the reaction, the villager does its last task again
	_reactions.Remove(id);
	EXPECT_EQ(StateOf(_villagers[0]), VillagerStates::DecideWhatToDo);
	EXPECT_EQ(registry.Get<const LivingReaction>(_villagers[0]).reaction, 0u);
	registry.Destroy(creature);
}

// Only an adult villager of the same town near a disciple, free to change what it does and not a disciple, follows it
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(ReactionRules, OnlyAFreeAdultOfTheTownNearADiscipleFollowsIt)
{
	using magic::villager_reaction::DroppedByHandPriority;
	using magic::villager_reaction::DroppedByHandWatch;
	const DroppedByHandWatch follows {
	    .leaderAvailable = true, .sameTown = true, .distance = 9.9f, .watcherFree = true, .watcherAvailable = true};
	EXPECT_EQ(DroppedByHandPriority(follows, 70), 70u);
	auto far = follows;
	far.distance = 10.0f;
	EXPECT_EQ(DroppedByHandPriority(far, 70), 0u);
	auto otherTown = follows;
	otherTown.sameTown = false;
	EXPECT_EQ(DroppedByHandPriority(otherTown, 70), 0u);
	auto disciple = follows;
	disciple.watcherDisciple = true;
	EXPECT_EQ(DroppedByHandPriority(disciple, 70), 0u);
	auto child = follows;
	child.watcherChild = true;
	EXPECT_EQ(DroppedByHandPriority(child, 70), 0u);
	auto held = follows;
	held.watcherFree = false;
	EXPECT_EQ(DroppedByHandPriority(held, 70), 0u);
	auto skeleton = follows;
	skeleton.watcherSkeleton = true;
	EXPECT_EQ(DroppedByHandPriority(skeleton, 70), 0u);
	auto leaderGone = follows;
	leaderGone.leaderAvailable = false;
	EXPECT_EQ(DroppedByHandPriority(leaderGone, 70), 0u);
}

// A disciple that starts its work makes a reaction. A villager of its town near it becomes a follower of the same
// work and decides again. It does not hold the reaction and the disciple does not impress it.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(Reactions, AVillagerNearADiscipleFollowsItToItsWorkOneTime)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& row = _info->reaction.at(static_cast<size_t>(Reaction::ReactToDroppedByHand));
	row.priority = 70;
	row.maxReactionDistance = 60.0f;
	row.numGameTurnsForNormalThingsToReact = 80;
	row.numGameTurnsForNormalThingsBeforeReactingAgain = 15;
	_info->villagerStateTable.at(static_cast<size_t>(VillagerStates::DecideWhatToDo)).field0xa8 = 1;
	Locator::infoConstants::reset();
	Locator::infoConstants::emplace(*_info);
	for (const auto villager : _villagers)
	{
		registry.Get<Villager>(villager).lifeStage = Villager::LifeStage::Adult;
	}
	const auto disciple = _villagers[0];
	registry.Assign<Disciple>(disciple, Disciple {.type = VillagerDisciple::Farmer});
	// The villagers find the reaction that they take up through the locator
	auto& reactions = Locator::reactionSystem::emplace<ecs::systems::ReactionSystem>();
	reactions.Create({.initiator = disciple,
	                  .type = Reaction::ReactToDroppedByHand,
	                  .player = PlayerNames::PLAYER_ONE,
	                  .position = k_Here,
	                  .onCast = true});
	// The villager 5 metres away follows the farmer; the one 200 metres away does not
	const auto* follower = registry.TryGet<const DiscipleFollower>(_villagers[1]);
	ASSERT_NE(follower, nullptr);
	EXPECT_EQ(follower->type, VillagerDisciple::Farmer);
	EXPECT_EQ(StateOf(_villagers[1]), VillagerStates::DecideWhatToDo);
	EXPECT_FALSE(registry.AllOf<DiscipleFollower>(_villagers[2]));
	EXPECT_FALSE(registry.AllOf<DiscipleFollower>(disciple));
	// It holds no reaction and gives its town no belief
	const auto* state = registry.TryGet<const LivingReaction>(_villagers[1]);
	EXPECT_TRUE(state == nullptr || state->reaction == 0);
	EXPECT_EQ(registry.TryGet<const TownImpression>(_town), nullptr);
	// A follower already does not take a new work
	registry.Get<Disciple>(disciple).type = VillagerDisciple::Forester;
	ecs::villager_disciple::Follow(registry.Get<LivingAction>(_villagers[1]), disciple);
	EXPECT_EQ(registry.Get<const DiscipleFollower>(_villagers[1]).type, VillagerDisciple::Farmer);
	// A new work from the hand ends the follower
	ecs::villager_disciple::SetType(_villagers[1], VillagerDisciple::Builder);
	EXPECT_FALSE(registry.AllOf<DiscipleFollower>(_villagers[1]));
	Locator::reactionSystem::reset();
}

// A creature a script has told to take no notice may take up no kind of reaction, until a script tells it to again
TEST_F(Reactions, ACreatureToldToTakeNoNoticeIsAvailableForNoReaction)
{
	auto& registry = Locator::entitiesRegistry::value();
	EXPECT_TRUE(_reactions.IsAvailable(_creature, Reaction::FleeFromObject));
	registry.Assign<CreatureReactionsOff>(_creature);
	EXPECT_FALSE(_reactions.IsAvailable(_creature, Reaction::FleeFromObject));
	EXPECT_FALSE(_reactions.IsAvailable(_creature, Reaction::ReactToHandPickUp));
	registry.Remove<CreatureReactionsOff>(_creature);
	EXPECT_TRUE(_reactions.IsAvailable(_creature, Reaction::ReactToHandPickUp));
}

// A creature with a forced plan takes up no kind of reaction, unless the leash leads the plan. An order on the leash
// alone does not keep reactions off.
TEST_F(Reactions, ACreatureWithAForcedPlanIsAvailableOnlyWhenTheLeashLeadsIt)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& mind = registry.Assign<CreatureMindState>(_creature);
	EXPECT_TRUE(_reactions.IsAvailable(_creature, Reaction::FleeFromObject));
	mind.planForced = true;
	EXPECT_FALSE(_reactions.IsAvailable(_creature, Reaction::FleeFromObject));
	EXPECT_FALSE(_reactions.IsAvailable(_creature, Reaction::ReactToHandPickUp));
	mind.ledByLeash = true;
	EXPECT_TRUE(_reactions.IsAvailable(_creature, Reaction::FleeFromObject));
	mind.planForced = false;
	mind.ledByLeash = false;
	registry.Assign<CreatureLeash>(_creature).order = CreatureLeash::Order {};
	EXPECT_TRUE(_reactions.IsAvailable(_creature, Reaction::FleeFromObject));
	registry.Remove<CreatureLeash>(_creature);
	registry.Remove<CreatureMindState>(_creature);
}

TEST(DiscipleGlow, TheFrameComesFromTheMillisecondsIntoTheHalfSecond)
{
	EXPECT_EQ(ecs::villager_disciple::GlowFrame(0), 0u);
	EXPECT_EQ(ecs::villager_disciple::GlowFrame(499), 14u);
	// A count of a whole half second shows the sixteenth puff, as the game's count does
	EXPECT_EQ(ecs::villager_disciple::GlowFrame(500), 15u);
	EXPECT_EQ(ecs::villager_disciple::GlowFrame(1000), 15u);
	EXPECT_EQ(ecs::villager_disciple::GlowFrame(501), 0u);
	// The share a millisecond is a float a little less than 0.03, but each product rounds to a float, so 100 ms is 3
	EXPECT_EQ(ecs::villager_disciple::GlowFrame(100), 3u);
	EXPECT_EQ(ecs::villager_disciple::GlowFrame(1400), 12u);
	EXPECT_EQ(ecs::villager_disciple::GlowFrame(99), 2u);
}

TEST(VillagerAvailability, AVillagerThatAScriptControlsReactsAgainWhenTheScriptReleasesIt)
{
	magic::villager_reaction::Availability facts {.stateLetsItReact = true, .life = 0.8f, .crawlLife = 0.1f};
	ASSERT_TRUE(magic::villager_reaction::Available(facts, Reaction::FleeFromObject));
	// The script takes control: the villager takes up no reaction, also not food
	facts.scriptControlled = true;
	EXPECT_FALSE(magic::villager_reaction::Available(facts, Reaction::FleeFromObject));
	EXPECT_FALSE(magic::villager_reaction::Available(facts, Reaction::ReactToFood));
	// The script releases it: it reacts again
	facts.scriptControlled = false;
	EXPECT_TRUE(magic::villager_reaction::Available(facts, Reaction::FleeFromObject));
}

TEST(VillagerAvailability, AWeakVillagerReactsOnlyToFoodAndADyingOneToNothing)
{
	magic::villager_reaction::Availability facts {.stateLetsItReact = true, .life = 0.1f, .crawlLife = 0.1f};
	EXPECT_FALSE(magic::villager_reaction::Available(facts, Reaction::FleeFromObject));
	EXPECT_TRUE(magic::villager_reaction::Available(facts, Reaction::ReactToFood));
	facts.life = 0.5f;
	facts.dyingNow = true;
	EXPECT_FALSE(magic::villager_reaction::Available(facts, Reaction::ReactToFood));
	facts.dyingNow = false;
	facts.stateLetsItReact = false;
	EXPECT_FALSE(magic::villager_reaction::Available(facts, Reaction::ReactToFood));
}

TEST_F(Reactions, AnAnimalLooksAtAThingToLookAt)
{
	auto& registry = Locator::entitiesRegistry::value();
	ReactToEverything(_info->animal.at(static_cast<size_t>(AnimalInfo::Cow)));
	Locator::infoConstants::emplace(*_info);
	auto& animals = static_cast<FakeAnimals&>(Locator::animalSystem::emplace<FakeAnimals>());
	animals.available = true;
	const auto cow = registry.Create();
	registry.Assign<Transform>(cow, k_Here + glm::vec3(10.0f, 0.0f, 0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	auto& cowAnimal = registry.Assign<Animal>(cow);
	cowAnimal.type = AnimalInfo::Cow;
	// The map puts an animal in the cell of its map position, which is where it stands
	cowAnimal.move.position = {map_coords::ToFixed(k_Here.x + 10.0f), map_coords::ToFixed(k_Here.z)};
	Locator::entitiesMap::value().Sync();
	_reactions.Create(Miracle(Reaction::LookAtObject));
	EXPECT_EQ(animals.lookedAt, std::vector<entt::entity> {static_cast<entt::entity>(1000)});
	EXPECT_EQ(registry.Get<const LivingReaction>(cow).type, Reaction::LookAtObject);
	Locator::animalSystem::reset();
}

TEST_F(Reactions, AnAnimalGoesOnlyToFoodThatInterestsIt)
{
	auto& registry = Locator::entitiesRegistry::value();
	ReactToEverything(_info->animal.at(static_cast<size_t>(AnimalInfo::Cow)));
	Locator::infoConstants::emplace(*_info);
	auto& animals = static_cast<FakeAnimals&>(Locator::animalSystem::emplace<FakeAnimals>());
	animals.available = true;
	const auto cow = registry.Create();
	registry.Assign<Transform>(cow, k_Here + glm::vec3(10.0f, 0.0f, 0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	auto& cowAnimal = registry.Assign<Animal>(cow);
	cowAnimal.type = AnimalInfo::Cow;
	// The map puts an animal in the cell of its map position, which is where it stands
	cowAnimal.move.position = {map_coords::ToFixed(k_Here.x + 10.0f), map_coords::ToFixed(k_Here.z)};
	Locator::entitiesMap::value().Sync();
	_reactions.Create(Miracle(Reaction::ReactToFood));
	EXPECT_TRUE(animals.wentTo.empty());
	animals.foodInterests = true;
	_reactions.Create(Miracle(Reaction::ReactToFood));
	EXPECT_EQ(animals.wentTo, std::vector<entt::entity> {static_cast<entt::entity>(1000)});
	Locator::animalSystem::reset();
}

TEST_F(Reactions, ANetworkLandStartsTheIdsOfTheReactionsAgain)
{
	const auto first = _reactions.Create(Miracle());
	const auto second = _reactions.Create(Miracle());
	EXPECT_NE(first, second);
	// A plain new land keeps the count of the ids
	_reactions.Reset();
	EXPECT_NE(_reactions.Create(Miracle()), first);
	// A network land starts them again, so that each computer gives the same ids
	_reactions.SetFreshIdsAtNextReset();
	_reactions.Reset();
	EXPECT_EQ(_reactions.Create(Miracle()), first);
	// Only the next reset does it
	_reactions.Reset();
	EXPECT_NE(_reactions.Create(Miracle()), first);
}
