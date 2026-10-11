/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <vector>

#include <gtest/gtest.h>

#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
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
	[[nodiscard]] bool HasReaction(entt::entity) const override { return false; }
	void ProcessTurn() override {}
	void Reset() override {}
	void SetLandBalance(float) override {}
	[[nodiscard]] float GetLandBalance() const override { return 1.0f; }
	[[nodiscard]] std::span<const Active> GetReactions() const override { return {}; }
	[[nodiscard]] std::vector<Active> ReactionsAt(const glm::vec3&) const override { return {}; }

	std::vector<Source> made;
};

/// Two creatures duelling in an arena on flat land, neither with a body to click on
class CreatureFightSystemTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<Registry>();
		auto& registry = Locator::entitiesRegistry::value();
		first = MakeCreature(registry, PlayerNames::PLAYER_ONE, {-15.0f, 0.0f, 0.0f});
		second = MakeCreature(registry, PlayerNames::PLAYER_TWO, {15.0f, 0.0f, 0.0f});
		for (const auto& [self, other] : {std::pair(first, second), std::pair(second, first)})
		{
			registry.Assign<CreatureFighting>(self, CreatureFighting {
			                                            .stage = CreatureFighting::Stage::Duel,
			                                            .opponent = other,
			                                            .arena = {.centre = {0.0f, 0.0f}, .radius = 40.0f},
			                                            .madeArena = self == first,
			                                        });
		}
	}
	void TearDown() override { Locator::entitiesRegistry::reset(); }

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

TEST_F(CreatureFightSystemTest, AFightWonIsSeenByThoseAroundUnlessAScriptControlsTheWinner)
{
	Locator::reactionSystem::emplace<RecordedReactions>();
	auto& reactions = static_cast<RecordedReactions&>(Locator::reactionSystem::value());
	fights.KnockOut(second);
	ASSERT_EQ(reactions.made.size(), 1u);
	EXPECT_EQ(reactions.made[0].type, Reaction::ReactToFightWon);
	EXPECT_EQ(reactions.made[0].initiator, first);
	EXPECT_EQ(reactions.made[0].player, PlayerNames::PLAYER_ONE);
	EXPECT_TRUE(reactions.made[0].onCast);
	Locator::reactionSystem::reset();
}

TEST_F(CreatureFightSystemTest, AScriptControlledWinnerMakesNoReaction)
{
	Locator::reactionSystem::emplace<RecordedReactions>();
	auto& reactions = static_cast<RecordedReactions&>(Locator::reactionSystem::value());
	Locator::entitiesRegistry::value().Assign<ScriptControlled>(first);
	fights.KnockOut(second);
	EXPECT_TRUE(reactions.made.empty());
	Locator::reactionSystem::reset();
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
