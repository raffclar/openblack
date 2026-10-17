/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <MindFile.h>
#include <gtest/gtest.h>

#include "Camera/Camera.h"
#include "Common/GameRandom.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureArena.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/GuidanceSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "Locator.h"

#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/CreatureFightSystem.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
namespace fight = openblack::creature_fight;

namespace
{
/// Keeps the reactions made, and does nothing else
class RecordedReactions final: public ecs::systems::ReactionSystemInterface
{
public:
	uint32_t Create(const Source& source) override
	{
		made.push_back(source);
		return static_cast<uint32_t>(made.size());
	}
	void Move(uint32_t, const glm::vec3&, const glm::vec3&, float) override {}
	[[nodiscard]] std::optional<Active> Find(uint32_t) const override { return std::nullopt; }
	void RemoveFrom(entt::entity) override {}
	void RemoveFrom(entt::entity, Reaction) override {}
	void Remove(uint32_t) override {}
	void SetInitiator(uint32_t, entt::entity) override {}
	[[nodiscard]] bool IsActive(uint32_t) const override { return false; }
	void StopReacting(entt::entity) override {}
	[[nodiscard]] bool HasReaction(entt::entity) const override { return false; }
	void ProcessTurn() override {}
	void Reset() override {}
	void SetLandBalance(Balance /*which*/, float /*balance*/) override {}
	[[nodiscard]] float GetLandBalance(Balance /*which*/) const override { return 1.0f; }
	[[nodiscard]] std::span<const Active> GetReactions() const override { return {}; }
	[[nodiscard]] std::vector<Active> ReactionsAt(const glm::vec3&) const override { return {}; }

	std::vector<Source> made;
};

/// A mind that keeps the plans that it is made to carry out, and does nothing else
class ForcedMinds final: public ecs::systems::CreatureMindSystemInterface
{
public:
	void ProcessTurn() override {}
	void PlanTurn() override {}
	void LearnTurn() override {}
	void LoadMind(entt::entity, std::shared_ptr<const creaturemind::MindFileData>) override {}
	[[nodiscard]] std::optional<creaturemind::MindFileData> SaveMind(entt::entity) const override { return std::nullopt; }
	void ClearLearning(entt::entity) override {}
	void SeeSkill(const glm::vec3&, size_t) override {}
	void SeeMiracle(const glm::vec3&, size_t) override {}
	void PlayerDid(size_t, const glm::vec3&, std::optional<entt::entity>, std::optional<PlayerNames>) override {}
	[[nodiscard]] const creature_mind_tables::Tables* GetTables() override { return nullptr; }
	bool PlayAction(entt::entity, size_t) override { return false; }
	bool PlayGesture(entt::entity, size_t) override { return false; }
	void PullFace(entt::entity, size_t) override {}
	std::optional<creature_face::Request> ShowFeeling(entt::entity, creature_face::Cue) override { return std::nullopt; }
	bool SitDown(entt::entity) override { return false; }
	void StandUp(entt::entity) override {}
	void ReceiveFeedback(entt::entity, float) override {}
	bool ForceAction(entt::entity, size_t, bool, std::optional<creature_face::Request>, float) override { return false; }
	bool Sleep(entt::entity) override { return false; }
	bool Eat(entt::entity, std::optional<entt::entity>) override { return false; }
	bool Drink(entt::entity) override { return false; }
	bool Poo(entt::entity creature) override
	{
		pooedDirectly = creature;
		return true;
	}
	bool Puke(entt::entity) override { return false; }
	bool Faint(entt::entity) override { return false; }
	void Wake(entt::entity) override {}
	void FoughtFight(entt::entity, bool) override {}
	void ChangeDesires(entt::entity, const std::function<void(creature_desires::Desires&)>&) override {}
	void SetAgendaPriority(entt::entity, float) override {}
	bool ForceActivity(entt::entity creature, const ForcedActivity& plan) override
	{
		forced.emplace_back(creature, plan.desire, std::string(plan.action), plan.actionObject, plan.activityObject);
		return true;
	}

	struct Forced
	{
		Forced(entt::entity who, creature_desires::Desire wanted, std::string done, entt::entity on,
		       std::optional<entt::entity> about)
		    : creature(who)
		    , desire(wanted)
		    , action(std::move(done))
		    , object(on)
		    , activityObject(about)
		{
		}
		entt::entity creature;
		creature_desires::Desire desire;
		std::string action;
		entt::entity object;
		std::optional<entt::entity> activityObject;
	};
	creature_mind::HandsCheck CheckHandsBeforeFaint(entt::entity) override
	{
		++handChecks;
		if (handChecks <= hands.size())
		{
			return hands.at(handChecks - 1);
		}
		return creature_mind::HandsCheck::Free;
	}
	std::vector<Forced> forced;
	std::optional<entt::entity> pooedDirectly;
	/// What the checks of the hands before a faint do in turn, then Free
	std::vector<creature_mind::HandsCheck> hands;
	size_t handChecks {0};
};

/// The game's random numbers: they give the numbers in the list in turn, then 0, and count the draws of each range
class ListedGameRandom final: public GameRandomInterface
{
public:
	uint32_t GameRand(uint32_t range) override
	{
		ranges.push_back(range);
		if (next < numbers.size())
		{
			return numbers.at(next++);
		}
		return 0;
	}
	float GameFloatRand(float) override { return 0.0f; }
	uint32_t LocalRand(int32_t) override { return 0; }
	float LocalFloatRand(float) override { return 0.0f; }
	int32_t CrtRand() override { return 0; }
	void CrtSrand(uint32_t) override {}
	[[nodiscard]] GameRandomSeeds GetSeeds() const override { return {}; }
	void SetSeeds(GameRandomSeeds) override {}
	[[nodiscard]] ParticleRandomStream GetParticleStream() const override { return ParticleRandomStream::None; }
	void SetParticleStream(ParticleRandomStream) override {}

	std::vector<uint32_t> numbers;
	size_t next {0};
	std::vector<uint32_t> ranges;
};

/// Two creatures in a duel in an arena on flat land, neither with a body to click on. The camera shows their fight.
class CreatureFightSystemTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<Registry>();
		auto& registry = Locator::entitiesRegistry::value();
		first = MakeCreature(registry, PlayerNames::PLAYER_ONE, {-15.0f, 0.0f, 0.0f});
		second = MakeCreature(registry, PlayerNames::PLAYER_TWO, {15.0f, 0.0f, 0.0f});
		// The arena has appeared
		const auto arena = registry.Create();
		for (const auto& [self, other] : {std::pair(first, second), std::pair(second, first)})
		{
			registry.Assign<CreatureFighting>(self, CreatureFighting {
			                                            .stage = CreatureFighting::Stage::Duel,
			                                            .opponent = other,
			                                            .arena = {.centre = {0.0f, 0.0f}, .radius = 40.0f},
			                                            .madeArena = self == first,
			                                            .arenaEntity = arena,
			                                        });
		}
		ShowTheFight({0.0f, 0.0f, 0.0f});
	}
	void TearDown() override
	{
		fights.Reset();
		Locator::camera::reset();
		Locator::entitiesRegistry::reset();
	}

	/// The world camera looks at the arena. A double click sends it to a place, and the update after it starts.
	void ShowTheFight(const glm::vec3& place)
	{
		if (!Locator::camera::has_value())
		{
			auto& camera = Locator::camera::emplace<Camera>();
			camera.SetOrigin({0.0f, 30.0f, 40.0f});
			camera.SetFocus({0.0f, 0.0f, 0.0f});
		}
		fights.FlewTo(place);
		fights.Update(std::chrono::duration<float, std::milli>(0.0f));
	}

	static entt::entity MakeCreature(Registry& registry, PlayerNames owner, glm::vec3 at)
	{
		const auto creature = registry.Create();
		registry.Assign<Transform>(creature, at, glm::mat3(1.0f), glm::vec3(1.0f));
		auto& body = registry.Assign<Creature>(creature);
		body.owner = owner;
		registry.Assign<CreatureAnimation>(creature);
		registry.Assign<CreatureLocomotion>(creature);
		return creature;
	}

	/// A press looking down at a point on the ground
	bool PressGround(glm::vec2 point, fight::Button button, uint32_t milliseconds = 0, uint32_t turn = 0)
	{
		return fights.Press({point.x, 100.0f, point.y}, {0.0f, -1.0f, 0.0f}, button, milliseconds, turn);
	}

	[[nodiscard]] const fight::MoveQueue& QueueOf(entt::entity creature) const
	{
		return Locator::entitiesRegistry::value().Get<const CreatureFighting>(creature).fighter.queue;
	}

	ecs::systems::CreatureFightSystem fights;
	entt::entity first {entt::null};
	entt::entity second {entt::null};
};
} // namespace

TEST_F(CreatureFightSystemTest, ThePlayersFighterIsTheirFirstCreature)
{
	EXPECT_EQ(fights.PlayersFighter(), first);
	// A creature the player got later doesn't take its place
	auto& registry = Locator::entitiesRegistry::value();
	MakeCreature(registry, PlayerNames::PLAYER_ONE, {100.0f, 0.0f, 0.0f});
	EXPECT_EQ(fights.PlayersFighter(), first);
	EXPECT_EQ(fights.HandTip(first), fight::Tip::Block);
	EXPECT_EQ(fights.HandTip(second), fight::Tip::Attack);
	EXPECT_EQ(fights.HandTip(std::nullopt), fight::Tip::Manoeuvre);
}

TEST_F(CreatureFightSystemTest, TheMoveButtonReplacesAndTheActionButtonQueues)
{
	EXPECT_TRUE(PressGround({-15.0f, 10.0f}, fight::Button::Action));
	fights.Release(0, 0);
	EXPECT_TRUE(PressGround({-15.0f, -10.0f}, fight::Button::Action));
	fights.Release(0, 0);
	EXPECT_EQ(QueueOf(first).Size(), 2u);
	EXPECT_TRUE(PressGround({-25.0f, 0.0f}, fight::Button::Move));
	fights.Release(0, 0);
	EXPECT_EQ(QueueOf(first).Size(), 1u);
	// The ground beyond the arena isn't the fight's
	EXPECT_FALSE(PressGround({45.0f, 0.0f}, fight::Button::Action));
	EXPECT_EQ(QueueOf(first).Size(), 1u);
}

TEST_F(CreatureFightSystemTest, ADoubleClickIntoTheArenaShowsTheFightAtOnce)
{
	EXPECT_TRUE(fights.IsCameraOnFight());
	EXPECT_TRUE(fights.DirectsFight());
	fights.Reset();
	EXPECT_FALSE(fights.IsCameraOnFight());
	// A place out of the arena does not show the fight
	ShowTheFight({45.0f, 0.0f, 0.0f});
	EXPECT_FALSE(fights.IsCameraOnFight());
	// A double click that a new land forgot does not show the fight
	fights.FlewTo({0.0f, 0.0f, 0.0f});
	fights.Reset();
	fights.Update(std::chrono::duration<float, std::milli>(0.0f));
	EXPECT_FALSE(fights.IsCameraOnFight());
}

TEST_F(CreatureFightSystemTest, ADoubleClickMeasuresFromWhereTheCameraWasBeforeItsFlight)
{
	fights.Reset();
	auto& camera = Locator::camera::value();
	// The camera is far from the arena when the player double clicks into it
	camera.SetOrigin({0.0f, 30.0f, 2000.0f});
	camera.SetFocus({0.0f, 0.0f, 1900.0f});
	fights.FlewTo({0.0f, 0.0f, 0.0f});
	// The flight brings the camera near before the next update, but the fight is not shown
	camera.SetOrigin({0.0f, 30.0f, 40.0f});
	camera.SetFocus({0.0f, 0.0f, 0.0f});
	fights.Update(std::chrono::duration<float, std::milli>(0.0f));
	EXPECT_FALSE(fights.IsCameraOnFight());
	// A fight of the player's creature under a script's control is shown from any distance
	Locator::entitiesRegistry::value().Assign<ScriptControlled>(second);
	camera.SetOrigin({0.0f, 30.0f, 2000.0f});
	camera.SetFocus({0.0f, 0.0f, 1900.0f});
	ShowTheFight({0.0f, 0.0f, 0.0f});
	EXPECT_TRUE(fights.IsCameraOnFight());
}

TEST_F(CreatureFightSystemTest, WithoutTheFightViewTheHandDoesNotDirectTheFight)
{
	fights.Reset();
	EXPECT_FALSE(fights.DirectsFight());
	EXPECT_FALSE(PressGround({-15.0f, 10.0f}, fight::Button::Action));
	EXPECT_TRUE(QueueOf(first).Empty());
	EXPECT_EQ(fights.HandTip(second), std::nullopt);
	// The duel still has the player's fighter, for the gestures
	EXPECT_EQ(fights.PlayersFighter(), first);
}

TEST_F(CreatureFightSystemTest, ANewLandForgetsThePressAndTheView)
{
	EXPECT_TRUE(PressGround({-15.0f, 10.0f}, fight::Button::Move, 100, 3));
	EXPECT_TRUE(fights.IsPressed());
	fights.SetFightExit(false);
	fights.Reset();
	EXPECT_FALSE(fights.IsPressed());
	EXPECT_FALSE(fights.IsCameraOnFight());
	EXPECT_TRUE(fights.GetFightExit());
	// The creatures of the old land are gone; letting go of the button touches nothing
	Locator::entitiesRegistry::value().Destroy(first);
	fights.Release(400, 5);
	EXPECT_FALSE(fights.IsPressed());
}

TEST_F(CreatureFightSystemTest, AScriptHandsAFighterToTheComputerOrToNobody)
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Get<CreatureFighting>(first).fighter.control = fight::Control::Player;
	EXPECT_TRUE(fights.IsAutoFighting(first));
	fights.SetAutoFighting(first, false);
	EXPECT_EQ(registry.Get<const CreatureFighting>(first).fighter.control, fight::Control::None);
	EXPECT_FALSE(fights.IsAutoFighting(first));
	// Moves queued for it don't hand it back to anyone
	EXPECT_TRUE(fights.QueueMove(first, fight::AttackMove(fight::Band::Low), false));
	EXPECT_FALSE(fights.IsAutoFighting(first));
	EXPECT_EQ(fights.QueuedBlows(first), 1u);
	fights.SetAutoFighting(first, true);
	EXPECT_EQ(registry.Get<const CreatureFighting>(first).fighter.control, fight::Control::Computer);
	EXPECT_TRUE(fights.IsAutoFighting(first));
	// A creature that never fought has nobody choosing its moves yet
	const auto newcomer = MakeCreature(registry, PlayerNames::PLAYER_ONE, {100.0f, 0.0f, 0.0f});
	EXPECT_FALSE(fights.IsAutoFighting(newcomer));
	fights.SetAutoFighting(newcomer, true);
	EXPECT_TRUE(fights.IsAutoFighting(newcomer));
}

TEST_F(CreatureFightSystemTest, ScriptsAskWhatAFighterIsDoing)
{
	auto& registry = Locator::entitiesRegistry::value();
	fight::Enter(registry.Get<CreatureFighting>(first).fighter, fight::State::Stance);
	EXPECT_EQ(fights.CurrentFightAction(first), fight::FightAction::Stance);
	const auto newcomer = MakeCreature(registry, PlayerNames::PLAYER_ONE, {100.0f, 0.0f, 0.0f});
	EXPECT_EQ(fights.CurrentFightAction(newcomer), fight::FightAction::Other);
	registry.Assign<CreatureKnockedOut>(newcomer);
	EXPECT_EQ(fights.CurrentFightAction(newcomer), fight::FightAction::Fainted);
}

TEST_F(CreatureFightSystemTest, AKnockOutThatNoBlowMadeGivesNoSightOfAFightWon)
{
	Locator::reactionSystem::emplace<RecordedReactions>();
	auto& reactions = static_cast<RecordedReactions&>(Locator::reactionSystem::value());
	// A miracle or a loss of life knocks the loser out, not a blow. The faint alone makes a reaction, as to a death, on
	// the turn that it starts on the body.
	fights.KnockOut(second);
	EXPECT_TRUE(reactions.made.empty());
	fights.ProcessTurn();
	ASSERT_EQ(reactions.made.size(), 1u);
	EXPECT_EQ(reactions.made.front().type, Reaction::ReactToDeath);
	EXPECT_EQ(reactions.made.front().initiator, second);
	EXPECT_TRUE(reactions.made.front().playerless);
	Locator::reactionSystem::reset();
}

TEST_F(CreatureFightSystemTest, TheWinnerIsMadeToShowOffAtTheLoser)
{
	auto& minds = static_cast<ForcedMinds&>(Locator::creatureMindSystem::emplace<ForcedMinds>());
	Locator::entitiesRegistry::value().Get<Creature>(first).alignment = -0.5f;
	fights.KnockOut(second);
	ASSERT_EQ(minds.forced.size(), 1u);
	EXPECT_EQ(minds.forced[0].creature, first);
	EXPECT_EQ(minds.forced[0].desire, creature_desires::Desire::ObeyPlayer);
	EXPECT_EQ(minds.forced[0].action, "ShowImpressiveAnimation");
	EXPECT_EQ(minds.forced[0].object, second);
	EXPECT_EQ(minds.forced[0].activityObject, second);
	EXPECT_FALSE(minds.pooedDirectly.has_value());
	Locator::creatureMindSystem::reset();
}

TEST_F(CreatureFightSystemTest, AnEvilWinnerIsMadeToPooOnTheLoser)
{
	auto& minds = static_cast<ForcedMinds&>(Locator::creatureMindSystem::emplace<ForcedMinds>());
	Locator::entitiesRegistry::value().Get<Creature>(first).alignment = -0.51f;
	fights.KnockOut(second);
	ASSERT_EQ(minds.forced.size(), 1u);
	EXPECT_EQ(minds.forced[0].action, "Poo");
	EXPECT_EQ(minds.forced[0].object, second);
	// The mind carries out the plan's own agenda: the fight never makes the creature poo by itself
	auto& registry = Locator::entitiesRegistry::value();
	for (int turn = 0; turn < 600 && registry.AllOf<CreatureFighting>(first); ++turn)
	{
		fights.Update(std::chrono::duration<float, std::milli>(100.0f));
		fights.ProcessTurn();
	}
	EXPECT_FALSE(registry.AllOf<CreatureFighting>(first));
	EXPECT_FALSE(minds.pooedDirectly.has_value());
	Locator::creatureMindSystem::reset();
}

TEST_F(CreatureFightSystemTest, ScriptsSeeAFightOnlyWhileItsDuelLasts)
{
	auto& registry = Locator::entitiesRegistry::value();
	EXPECT_TRUE(fights.IsDuelling(first));
	registry.Get<CreatureFighting>(first).stage = CreatureFighting::Stage::Taunt;
	EXPECT_FALSE(fights.IsDuelling(first));
	EXPECT_TRUE(fights.IsFighting(first));
	registry.Get<CreatureFighting>(first).stage = CreatureFighting::Stage::Duel;
	fights.KnockOut(second);
	// The winner finishes its move in the fight, but its duel is over
	EXPECT_FALSE(fights.IsDuelling(first));
	EXPECT_FALSE(fights.IsDuelling(second));
}

TEST_F(CreatureFightSystemTest, AScriptTakesACreatureFromItsDuelForBoth)
{
	fights.TakeForScript(first);
	EXPECT_FALSE(fights.IsDuelling(first));
	EXPECT_FALSE(fights.IsDuelling(second));
	// Neither won: neither is knocked out
	EXPECT_FALSE(fights.IsKnockedOut(first));
	EXPECT_FALSE(fights.IsKnockedOut(second));
}

TEST_F(CreatureFightSystemTest, AScriptTakesAKnockedOutCreatureBeforeItIsTakenHome)
{
	auto& registry = Locator::entitiesRegistry::value();
	fights.KnockOut(second);
	ASSERT_TRUE(fights.IsKnockedOut(second));
	const auto where = registry.Get<const Transform>(second).position;
	fights.TakeForScript(second);
	EXPECT_FALSE(fights.IsKnockedOut(second));
	// It is not taken home later
	for (int turn = 0; turn < 600; ++turn)
	{
		fights.ProcessTurn();
	}
	EXPECT_EQ(registry.Get<const Transform>(second).position, where);
}

TEST_F(CreatureFightSystemTest, AScriptDoesNotRaiseACreatureKilledForGood)
{
	fights.Withdraw(second);
	fights.KillPermanently(second);
	ASSERT_TRUE(fights.IsKnockedOut(second));
	fights.TakeForScript(second);
	EXPECT_TRUE(fights.IsKnockedOut(second));
}

TEST_F(CreatureFightSystemTest, AScriptReadsAndSetsTheFightHealthInAndOutOfAFight)
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Get<CreatureFighting>(second).fighter.health = 0.6f;
	EXPECT_FLOAT_EQ(fights.GetFightHealth(second), 0.6f);
	// Set in a fight, the fighter takes it at once
	fights.SetFightHealth(second, 1.0f);
	EXPECT_FLOAT_EQ(registry.Get<const CreatureFighting>(second).fighter.health, 1.0f);
	registry.Get<CreatureFighting>(second).fighter.health = 0.25f;
	// Out of the fight it stays as the fight left it
	fights.Withdraw(second);
	ASSERT_FALSE(registry.AllOf<CreatureFighting>(second));
	EXPECT_FLOAT_EQ(fights.GetFightHealth(second), 0.25f);
	// A creature that never fought has full fight health, and keeps whatever a script gives it
	const auto other = MakeCreature(registry, PlayerNames::PLAYER_THREE, {200.0f, 0.0f, 0.0f});
	EXPECT_FLOAT_EQ(fights.GetFightHealth(other), 1.0f);
	fights.SetFightHealth(other, 1.5f);
	EXPECT_FLOAT_EQ(fights.GetFightHealth(other), 1.5f);
}

TEST_F(CreatureFightSystemTest, AFightCalledOffEndsWithNoWinnerAndNoResponse)
{
	Locator::reactionSystem::emplace<RecordedReactions>();
	auto& reactions = static_cast<RecordedReactions&>(Locator::reactionSystem::value());
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto self : {first, second})
	{
		fight::Enter(registry.Get<CreatureFighting>(self).fighter, fight::State::Stance);
	}
	EXPECT_TRUE(fights.QueueMove(first, fight::AttackMove(fight::Band::High), false));
	fights.CallOff(first);
	for (const auto self : {first, second})
	{
		const auto& side = registry.Get<const CreatureFighting>(self);
		// Both wind down to stand, with nothing left to do and nothing to celebrate or sulk about
		EXPECT_TRUE(side.ended);
		EXPECT_TRUE(side.fighter.stopping);
		EXPECT_TRUE(side.played);
		EXPECT_EQ(side.stage, CreatureFighting::Stage::Respond);
		EXPECT_TRUE(side.fighter.queue.Empty());
	}
	// Nobody won it
	EXPECT_TRUE(reactions.made.empty());
	EXPECT_EQ(registry.Get<const CreatureFightRecord>(first).wins, 0u);
	EXPECT_EQ(registry.Get<const CreatureFightRecord>(second).wins, 0u);
	Locator::reactionSystem::reset();
}

TEST_F(CreatureFightSystemTest, OnTheWayToAFightACreatureIsSettingItUp)
{
	auto& registry = Locator::entitiesRegistry::value();
	EXPECT_FALSE(fights.IsSettingUpFight(first));
	registry.Get<CreatureFighting>(first).stage = CreatureFighting::Stage::Approach;
	EXPECT_TRUE(fights.IsSettingUpFight(first));
	// Called off then, it simply leaves the fight
	fights.CallOff(first);
	EXPECT_FALSE(registry.AllOf<CreatureFighting>(first));
}

TEST_F(CreatureFightSystemTest, ACreatureRefusesAFightWithOneTooBigOrTooSmall)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto big = MakeCreature(registry, PlayerNames::PLAYER_ONE, {100.0f, 0.0f, 0.0f});
	const auto small = MakeCreature(registry, PlayerNames::PLAYER_TWO, {130.0f, 0.0f, 0.0f});
	registry.Get<Creature>(big).size = 1.5f;
	registry.Get<Creature>(small).size = 0.6f;
	using Start = ecs::systems::CreatureFightSystemInterface::StartResult;
	// Less than half the asker's height, it says no, either way round
	EXPECT_EQ(fights.StartFight(big, small), Start::Refused);
	EXPECT_EQ(fights.StartFight(small, big), Start::Refused);
	EXPECT_FALSE(registry.AnyOf<CreatureFighting>(big));
	EXPECT_FALSE(registry.AnyOf<CreatureFighting>(small));
	// Only one of the two under a script's control: refused too
	registry.Get<Creature>(small).size = 1.0f;
	registry.Assign<ScriptControlled>(small);
	EXPECT_EQ(fights.StartFight(big, small), Start::Refused);
}

TEST_F(CreatureFightSystemTest, AKnockOutKeepsTheWinnerAndTheLoserFromReactions)
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Assign<CreatureMindState>(first).idle.serial = 4;
	registry.Assign<CreatureMindState>(second).idle.serial = 9;
	Locator::creatureMindSystem::emplace<ForcedMinds>();
	fights.KnockOut(second);
	// Each keeps the number of the agenda that it had at the knock-out
	ASSERT_TRUE(registry.AllOf<CreatureAfterFight>(first));
	ASSERT_TRUE(registry.AllOf<CreatureAfterFight>(second));
	EXPECT_EQ(registry.Get<const CreatureAfterFight>(first).agenda, 4u);
	EXPECT_EQ(registry.Get<const CreatureAfterFight>(second).agenda, 9u);
	Locator::creatureMindSystem::reset();
}

TEST_F(CreatureFightSystemTest, AFightFacesOffAndTauntsBeforeItLooksForAnArena)
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Remove<CreatureFighting>(first);
	registry.Remove<CreatureFighting>(second);
	ASSERT_EQ(fights.StartFight(first, second), ecs::systems::CreatureFightSystemInterface::StartResult::Started);
	EXPECT_EQ(registry.Get<CreatureFighting>(first).stage, CreatureFighting::Stage::FaceOff);
	EXPECT_EQ(registry.Get<CreatureFighting>(second).stage, CreatureFighting::Stage::FaceOff);
	EXPECT_TRUE(registry.Get<CreatureFighting>(first).arenaEntity == entt::null);
	EXPECT_TRUE(fights.IsSettingUpFight(second));

	// The first creature turns, then tells the other creature to taunt with it
	fights.ProcessTurn();
	fights.ProcessTurn();
	EXPECT_EQ(registry.Get<CreatureFighting>(first).stage, CreatureFighting::Stage::Taunt);
	EXPECT_EQ(registry.Get<CreatureFighting>(second).stage, CreatureFighting::Stage::Taunt);

	// This land has no room for an arena, so after the taunts the fight stops for both
	for (int turn = 0; turn < 4 && registry.AnyOf<CreatureFighting>(first); ++turn)
	{
		fights.ProcessTurn();
	}
	EXPECT_FALSE(registry.AnyOf<CreatureFighting>(first));
	EXPECT_FALSE(registry.AnyOf<CreatureFighting>(second));
}

namespace
{
/// Keeps the kinds of the remarks asked for, and does nothing else
class RecordedGuidance final: public ecs::systems::GuidanceSystemInterface
{
public:
	void Reset() override {}
	void ProcessTurn() override {}
	bool PlayNow(guidance::Kind) override { return false; }
	bool RemarkNow(guidance::Kind) override { return false; }
	void RemarkIfNow(guidance::Kind kind) override { asked.push_back(kind); }
	void TownAttacked(entt::entity, PlayerNames, bool) override {}
	void TownAttackCry(entt::entity, const guidance::TownAttackVoice&) override {}
	void BuildingBroken(entt::entity, std::optional<PlayerNames>, float) override {}
	void AlignmentTurn(float) override {}
	void SayOneOff(guidance::OneOff) override {}
	void EndWaits() override {}
	[[nodiscard]] const std::vector<Remark>& GetRemarks() const override { return remarks; }
	[[nodiscard]] int32_t GetMoonWait() const override { return 0; }

	std::vector<guidance::Kind> asked;
	std::vector<Remark> remarks;
};
} // namespace

TEST_F(CreatureFightSystemTest, TheAdvisorsSpeakOfAFightOfThePlayersCreatureTooFarToShow)
{
	auto& guidance = static_cast<RecordedGuidance&>(Locator::guidanceSystem::emplace<RecordedGuidance>());
	fights.Reset();
	auto& camera = Locator::camera::value();
	camera.SetOrigin({0.0f, 30.0f, 2000.0f});
	camera.SetFocus({0.0f, 0.0f, 1900.0f});
	ShowTheFight({0.0f, 0.0f, 0.0f});
	EXPECT_FALSE(fights.IsCameraOnFight());
	EXPECT_EQ(guidance.asked, std::vector {guidance::Kind::CreatureFight});
	// A fight near enough is shown, and the advisors say nothing
	guidance.asked.clear();
	camera.SetOrigin({0.0f, 30.0f, 40.0f});
	camera.SetFocus({0.0f, 0.0f, 0.0f});
	ShowTheFight({0.0f, 0.0f, 0.0f});
	EXPECT_TRUE(fights.IsCameraOnFight());
	EXPECT_TRUE(guidance.asked.empty());
	// A fight far away between creatures of other players is not spoken of
	fights.Reset();
	Locator::entitiesRegistry::value().Get<Creature>(first).owner = PlayerNames::PLAYER_THREE;
	camera.SetOrigin({0.0f, 30.0f, 2000.0f});
	camera.SetFocus({0.0f, 0.0f, 1900.0f});
	ShowTheFight({0.0f, 0.0f, 0.0f});
	EXPECT_TRUE(guidance.asked.empty());
	Locator::guidanceSystem::reset();
}

TEST_F(CreatureFightSystemTest, TheFightMusicPlaysWhileTheHandDirectsTheFightOrNearItsArena)
{
	auto& registry = Locator::entitiesRegistry::value();
	EXPECT_TRUE(fights.DirectsFight());
	EXPECT_TRUE(fights.FightMusicHeard({0.0f, 30.0f, 2000.0f}));
	fights.Reset();
	// The arena of the fight, seen from near and from far
	const auto arena = registry.Get<const CreatureFighting>(first).arenaEntity;
	registry.Assign<Transform>(arena, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<CreatureArena>(arena, CreatureArena {.place = {0, 0},
	                                                     .radius = 40.0f,
	                                                     .temporary = true,
	                                                     .fightOn = true,
	                                                     .first = first,
	                                                     .second = second,
	                                                     .ring = nullptr});
	EXPECT_TRUE(fights.FightMusicHeard({0.0f, 30.0f, 90.0f}));
	EXPECT_FALSE(fights.FightMusicHeard({0.0f, 30.0f, 2000.0f}));
	// No fight is on in the arena, or the player's creature is not in it
	registry.Get<CreatureArena>(arena).fightOn = false;
	EXPECT_FALSE(fights.FightMusicHeard({0.0f, 30.0f, 90.0f}));
	registry.Get<CreatureArena>(arena).fightOn = true;
	registry.Get<Creature>(first).owner = PlayerNames::PLAYER_THREE;
	EXPECT_FALSE(fights.FightMusicHeard({0.0f, 30.0f, 90.0f}));
}

TEST_F(CreatureFightSystemTest, AFightGestureIsTakenOnlyWhenTheLineToTheOpponentMeetsItsBody)
{
	// Neither creature has a body, so the line from the player's creature meets nothing
	EXPECT_FALSE(fights.GestureSpecialMove());
	EXPECT_FALSE(fights.GestureSpell(MagicType::Fireball));
	EXPECT_TRUE(QueueOf(first).Empty());
	// Without a duel of the player's creature, no gesture is taken
	Locator::entitiesRegistry::value().Get<Creature>(first).owner = PlayerNames::PLAYER_THREE;
	EXPECT_EQ(fights.PlayersFighter(), std::nullopt);
	EXPECT_FALSE(fights.GestureSpecialMove());
}

TEST_F(CreatureFightSystemTest, ThePlayerDirectsTheirOwnCreatureAndTheComputerAnyOther)
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Remove<CreatureFighting>(first);
	registry.Remove<CreatureFighting>(second);
	// The other creature starts the fight: who starts it does not matter
	ASSERT_EQ(fights.StartFight(second, first), ecs::systems::CreatureFightSystemInterface::StartResult::Started);
	EXPECT_EQ(registry.Get<const CreatureFighting>(first).fighter.control, fight::Control::Player);
	EXPECT_EQ(registry.Get<const CreatureFighting>(second).fighter.control, fight::Control::Computer);
	EXPECT_TRUE(fights.IsAutoFighting(first));
	EXPECT_TRUE(fights.IsAutoFighting(second));
}

TEST_F(CreatureFightSystemTest, LifeLostInADuelTakesTheFightHealthAndCanKnockTheCreatureOut)
{
	auto& registry = Locator::entitiesRegistry::value();
	Locator::creatureMindSystem::emplace<ForcedMinds>();
	registry.Get<CreatureFighting>(first).fighter.health = 0.5f;
	fights.LoseFightHealth(first, 0.2f);
	EXPECT_FLOAT_EQ(registry.Get<const CreatureFighting>(first).fighter.health, 0.3f);
	EXPECT_FALSE(fights.IsKnockedOut(first));
	// With none left it is knocked out, and its opponent wins
	fights.LoseFightHealth(first, 0.5f);
	EXPECT_TRUE(fights.IsKnockedOut(first));
	EXPECT_EQ(registry.Get<const CreatureFightRecord>(second).wins, 1u);
	Locator::creatureMindSystem::reset();
}

TEST_F(CreatureFightSystemTest, LifeLostOutOfADuelLeavesTheFightHealth)
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Get<CreatureFighting>(first).stage = CreatureFighting::Stage::Taunt;
	const auto before = registry.Get<const CreatureFighting>(first).fighter.health;
	fights.LoseFightHealth(first, 0.2f);
	EXPECT_FLOAT_EQ(registry.Get<const CreatureFighting>(first).fighter.health, before);
}

// As the fight starts, the game's random numbers choose the taunt of each creature, the other creature's first. Each try
// to start the taunt draws a side first, and a body that plays an action refuses the try until it is free.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureFightSystemTest, TheGameChoosesTheTauntsAndEachTryDrawsASide)
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Remove<CreatureFighting>(first);
	registry.Remove<CreatureFighting>(second);
	auto& game = static_cast<ListedGameRandom&>(Locator::gameRandom::emplace<ListedGameRandom>());
	game.numbers = {1, 0};
	ASSERT_EQ(fights.StartFight(first, second), ecs::systems::CreatureFightSystemInterface::StartResult::Started);
	EXPECT_EQ(game.ranges, (std::vector<uint32_t> {2, 2}));
	EXPECT_TRUE(registry.Get<const CreatureFighting>(second).rude);
	EXPECT_FALSE(registry.Get<const CreatureFighting>(first).rude);

	registry.Get<CreatureFighting>(first).stage = CreatureFighting::Stage::Taunt;
	registry.Get<CreatureFighting>(second).stage = CreatureFighting::Stage::Taunt;
	auto& body = registry.Get<CreatureAnimation>(first).body;
	body.kind = creature_layers::BodyAction::Kind::Once;
	for (int turn = 0; turn < 3; ++turn)
	{
		fights.ProcessTurn();
	}
	// The busy creature draws on each turn and does not taunt; the other draws once, taunts and goes on
	EXPECT_FALSE(registry.Get<const CreatureFighting>(first).played);
	EXPECT_EQ(registry.Get<const CreatureFighting>(first).stage, CreatureFighting::Stage::Taunt);
	EXPECT_NE(registry.Get<const CreatureFighting>(second).stage, CreatureFighting::Stage::Taunt);
	EXPECT_EQ(game.ranges.size(), 2u + 3u + 1u);
	Locator::gameRandom::reset();
}

// A faint starts on the body on the next turn of the knock-out. It cuts what the body plays and lies on the side that
// one draw of the game's random numbers chooses then.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureFightSystemTest, AFaintCutsTheBodyAndDrawsItsSide)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& game = static_cast<ListedGameRandom&>(Locator::gameRandom::emplace<ListedGameRandom>());
	game.numbers = {1};
	registry.Get<CreatureAnimation>(second).body.kind = creature_layers::BodyAction::Kind::Once;
	fights.ForceFaint(second);
	EXPECT_TRUE(game.ranges.empty());
	EXPECT_EQ(registry.Get<const CreatureAnimation>(second).body.kind, creature_layers::BodyAction::Kind::Once);
	fights.ProcessTurn();
	const auto& body = registry.Get<const CreatureAnimation>(second).body;
	EXPECT_EQ(body.kind, creature_layers::BodyAction::Kind::Sequence);
	EXPECT_TRUE(body.mirrored);
	EXPECT_EQ(game.ranges, (std::vector<uint32_t> {2}));
	Locator::gameRandom::reset();
}

// A faint that does not come from the mind first checks the hands. With a thing in the hand, the creature starts to let
// go, and the faint starts on the next turn with no new check. While the body or the hands are busy, the faint waits.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureFightSystemTest, AFaintWaitsForTheLetGoOfTheThingInTheHand)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& minds = static_cast<ForcedMinds&>(Locator::creatureMindSystem::emplace<ForcedMinds>());
	auto& game = static_cast<ListedGameRandom&>(Locator::gameRandom::emplace<ListedGameRandom>());
	using creature_mind::HandsCheck;
	minds.hands = {HandsCheck::Wait, HandsCheck::Wait, HandsCheck::LetGo};
	fights.ForceFaint(second);
	const auto faints = [&] {
		return registry.Get<const CreatureAnimation>(second).body.kind == creature_layers::BodyAction::Kind::Sequence;
	};
	// Two turns busy, then the let-go starts
	for (size_t turn = 1; turn <= 3; ++turn)
	{
		fights.ProcessTurn();
		EXPECT_FALSE(faints());
		EXPECT_EQ(minds.handChecks, turn);
		EXPECT_TRUE(game.ranges.empty());
	}
	EXPECT_TRUE(registry.Get<const CreatureKnockedOut>(second).handsChecked);
	// The faint starts with no new check and draws its side
	fights.ProcessTurn();
	EXPECT_TRUE(faints());
	EXPECT_EQ(minds.handChecks, 3u);
	EXPECT_EQ(game.ranges, (std::vector<uint32_t> {2}));
	EXPECT_FALSE(registry.Get<const CreatureKnockedOut>(second).fainting);
	Locator::gameRandom::reset();
	Locator::creatureMindSystem::reset();
}

// The knocked-out creature counts the time that it lies only from when it lies still: not while the faint waits for the
// hands, and not while the faint plays before it lies still.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureFightSystemTest, AKnockedOutCreatureCountsItsTimeFromWhenItLiesStill)
{
	auto& registry = Locator::entitiesRegistry::value();
	fights.ForceFaint(second);
	fights.ProcessTurn();
	fights.ProcessTurn();
	const auto& knockedOut = registry.Get<const CreatureKnockedOut>(second);
	EXPECT_LT(knockedOut.seconds, 0.0f);
	// It lies still: from now its time counts on each turn
	registry.Get<CreatureAnimation>(second).body.phase = creature_layers::BodyAction::Phase::Loop;
	fights.ProcessTurn();
	const auto first = knockedOut.seconds;
	fights.ProcessTurn();
	EXPECT_GT(knockedOut.seconds, first);
	EXPECT_EQ(knockedOut.stage, CreatureKnockedOut::Stage::Lying);
}
