/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The creature fight system, stepped by the turn, with fakes of the animations, skins, minds, hands and locomotion: what
// starts a fight, a duel's turn on the synced stream only, the frame drawing between the turns, the hand's press kept on
// the player's fighter, the creature knocked out taken home at once, and the blow's clock at 100 ms steps against 30
// frames a second

#define LOCATOR_IMPLEMENTATIONS

#include <utility>

#include <gtest/gtest.h>

#include "Common/GameRandom.h"
#include "Common/GameRandomTesting.h"
#include "Creature/CreatureFight.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/Transform.h"
#include "ECS/Systems/Implementations/CreatureFightSystem.h"
#include "ECS/Systems/Implementations/MapCellsSystem.h"
#include "creature/CreatureSystemFakes.h"
#include "creature/CreatureSystemWorld.h"

using namespace openblack;
using namespace openblack::ecs::components;
using openblack::ecs::systems::CreatureFightSystem;
using StartResult = openblack::ecs::systems::CreatureFightSystemInterface::StartResult;
namespace fakes = openblack::test::creature_fakes;
namespace fight = openblack::creature_fight;

namespace
{
class CreatureFightSystemTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::mapCellsSystem::emplace<ecs::systems::MapCellsSystem>();
		_animation = &static_cast<fakes::FakeAnimation&>(Locator::creatureAnimationSystem::emplace<fakes::FakeAnimation>());
		_skin = &static_cast<fakes::FakeSkin&>(Locator::creatureSkinSystem::emplace<fakes::FakeSkin>());
		_mind = &static_cast<fakes::FakeMind&>(Locator::creatureMindSystem::emplace<fakes::FakeMind>());
		_hands =
		    &static_cast<fakes::FakeObjectAction&>(Locator::creatureObjectActionSystem::emplace<fakes::FakeObjectAction>());
		_locomotion = &static_cast<fakes::FakeLocomotion&>(Locator::creatureLocomotionSystem::emplace<fakes::FakeLocomotion>());
	}

	static CreatureFighting& FightingOf(entt::entity creature)
	{
		return test::creature_world::World::Registry().Get<CreatureFighting>(creature);
	}

	/// Two creatures duelling, facing each other 20 apart along z, both fought by the computer
	std::pair<entt::entity, entt::entity> Duel()
	{
		const auto a = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
		const auto b = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 80.0f), PlayerNames::PLAYER_THREE);
		EXPECT_EQ(_system.StartFight(a, b), StartResult::Started);
		auto& registry = test::creature_world::World::Registry();
		for (const auto& [self, heading, at] : {std::tuple(a, 0.0f, glm::vec3(100.0f, 0.0f, 100.0f)),
		                                        std::tuple(b, std::numbers::pi_v<float>, glm::vec3(100.0f, 0.0f, 80.0f))})
		{
			auto& locomotion = registry.Get<CreatureLocomotion>(self);
			locomotion.heading = heading;
			locomotion.fromPosition = at;
			locomotion.toPosition = at;
			auto& fighting = FightingOf(self);
			fighting.stage = CreatureFighting::Stage::Duel;
			fighting.measured = true;
			fight::Enter(fighting.fighter, fight::State::Stance);
			fighting.fighter.control = fight::Control::Computer;
		}
		return {a, b};
	}

	test::creature_world::World _world;
	const test::RestoreService<Locator::mapCellsSystem> _restoreCells;
	const test::RestoreService<Locator::creatureAnimationSystem> _restoreAnimation;
	const test::RestoreService<Locator::creatureSkinSystem> _restoreSkin;
	const test::RestoreService<Locator::creatureMindSystem> _restoreMind;
	const test::RestoreService<Locator::creatureObjectActionSystem> _restoreHands;
	const test::RestoreService<Locator::creatureLocomotionSystem> _restoreLocomotion;
	fakes::FakeAnimation* _animation {nullptr};
	fakes::FakeSkin* _skin {nullptr};
	fakes::FakeMind* _mind {nullptr};
	fakes::FakeObjectAction* _hands {nullptr};
	fakes::FakeLocomotion* _locomotion {nullptr};
	CreatureFightSystem _system;
};

/// A blow of 1000 ms landing at 400 ms, played at the speed of a blow, stepped some milliseconds at a time for a while:
/// how many times it landed, and how long each blow took
struct Blows
{
	int landed {0};
	int finished {0};
	float lastBlowMs {0.0f};
};
Blows PlayBlows(float stepMs, float totalMs)
{
	constexpr float k_Duration = 1000.0f;
	constexpr float k_HitMs = 400.0f;
	Blows blows;
	float timeMs = 0.0f;
	float sinceStart = 0.0f;
	bool landed = false;
	for (float elapsed = 0.0f; elapsed + (stepMs * 0.5f) < totalMs; elapsed += stepMs)
	{
		const auto before = timeMs;
		timeMs = fight::AdvanceClock(timeMs, stepMs, 1.0f, fight::animations::k_FirstAttack);
		sinceStart += stepMs;
		if (!landed && fight::CrossesHit(before, timeMs, k_HitMs))
		{
			landed = true;
			++blows.landed;
		}
		// the next blow starts from its beginning: what went past the end is lost, as Enter does
		if (timeMs >= k_Duration)
		{
			++blows.finished;
			blows.lastBlowMs = sinceStart;
			sinceStart = 0.0f;
			timeMs = 0.0f;
			landed = false;
		}
	}
	return blows;
}
} // namespace

TEST_F(CreatureFightSystemTest, WhatStartsAFight)
{
	const auto a = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	const auto b = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 60.0f), PlayerNames::PLAYER_THREE);
	EXPECT_EQ(_system.StartFight(a, a), StartResult::NoOpponent);
	EXPECT_EQ(_system.StartFight(a, test::creature_world::World::Registry().Create()), StartResult::NoOpponent);
	ASSERT_EQ(_system.StartFight(a, b), StartResult::Started);
	EXPECT_TRUE(_system.IsFighting(a));
	EXPECT_EQ(_system.OpponentOf(b), std::optional(a));
	// both stopped what they were doing
	EXPECT_EQ(_locomotion->stopped.size(), 2u);
	EXPECT_EQ(_hands->cancelled.size(), 2u);
	EXPECT_EQ(_system.StartFight(b, a), StartResult::Busy);
	// a creature too hurt to fight
	const auto c = test::creature_world::World::MakeCreature(glm::vec3(0.0f), PlayerNames::PLAYER_TWO);
	const auto d = test::creature_world::World::MakeCreature(glm::vec3(0.0f), PlayerNames::PLAYER_THREE);
	auto& needs = test::creature_world::World::Registry().Get<CreatureNeeds>(c);
	needs.needs.life = 0.0f;
	EXPECT_EQ(_system.StartFight(c, d), StartResult::TooWeak);
	_system.AbortFight(a);
	EXPECT_FALSE(_system.IsFighting(a));
	EXPECT_FALSE(_system.IsFighting(b));
}

TEST_F(CreatureFightSystemTest, ADuelsTurnDrawsOnTheSyncedStreamOnly)
{
	const auto [a, b] = Duel();
	const game_random::testing::ScopedState state;
	std::vector<uint32_t> draws;
	game_random::testing::SetGameRand(
	    [&draws](uint32_t n) {
		    draws.push_back(n);
		    return 0u;
	    },
	    [](float) { return 0.0f; });
	const auto local = game_random::Current().local;
	const auto crt = game_random::crt::Seed();
	for (int turn = 0; turn < 10; ++turn)
	{
		_system.ProcessTurn();
	}
	// the computer chose its moves on the synced stream
	EXPECT_FALSE(draws.empty());
	EXPECT_EQ(game_random::Current().local, local);
	EXPECT_EQ(game_random::crt::Seed(), crt);
	EXPECT_TRUE(_system.IsFighting(a));
	EXPECT_TRUE(_system.IsFighting(b));
}

TEST_F(CreatureFightSystemTest, TheFrameOnlyDrawsBetweenTheTurns)
{
	const auto creature = test::creature_world::World::MakeCreature();
	auto& registry = test::creature_world::World::Registry();
	// a blow playing out after a fight that ended: nothing but its animation moves it on
	auto& fighting = registry.AssignState<CreatureFighting>(creature);
	fighting.stage = CreatureFighting::Stage::Respond;
	fight::Enter(fighting.fighter, fight::State::Action, fight::animations::k_FirstAttack);
	fighting.fighter.timeMs = 200.0f;
	_animation->durations[fight::animations::k_FirstAttack] = 1000.0f;

	_system.ProcessTurn();
	const auto after = FightingOf(creature).fighter.timeMs;
	EXPECT_FLOAT_EQ(FightingOf(creature).drawFromMs, 200.0f);
	EXPECT_FLOAT_EQ(after, fight::AdvanceClock(200.0f, 100.0f, 1.0f, fight::animations::k_FirstAttack));
	const auto transform = registry.Get<Transform>(creature);

	_system.Update(0.5f, 16.0f);
	EXPECT_FLOAT_EQ(FightingOf(creature).fighter.timeMs, after);
	EXPECT_FLOAT_EQ(registry.Get<CreatureAnimation>(creature).body.timeMs, fight::DrawnTime(200.0f, after, 0.5f));
	EXPECT_EQ(registry.Get<Transform>(creature).position, transform.position);
	EXPECT_FALSE(_system.IsPressed());
}

TEST_F(CreatureFightSystemTest, APressChargesOnThePlayersFighter)
{
	const auto [a, b] = Duel();
	auto& registry = test::creature_world::World::Registry();
	const auto& lookup = std::as_const(registry);
	registry.Get<Creature>(a).owner = PlayerNames::PLAYER_ONE;
	// straight down on the middle of the arena: a step there
	const glm::vec3 from(100.0f, 50.0f, 90.0f);
	const glm::vec3 down(0.0f, -1.0f, 0.0f);
	EXPECT_FALSE(_system.IsPressed());
	ASSERT_TRUE(_system.Press(from, down));
	EXPECT_TRUE(_system.IsPressed());
	ASSERT_NE(lookup.TryGet<const CreatureFightPress>(a), nullptr);
	EXPECT_EQ(lookup.TryGet<const CreatureFightPress>(b), nullptr);
	// the frames charge it
	_system.Update(0.5f, 16.0f);
	_system.Update(0.5f, 20.0f);
	EXPECT_FLOAT_EQ(lookup.Get<const CreatureFightPress>(a).heldMs, 36.0f);
	// pressed again, the charge starts again
	ASSERT_TRUE(_system.Press(from, down));
	EXPECT_FLOAT_EQ(lookup.Get<const CreatureFightPress>(a).heldMs, 0.0f);
	// let go, it is gone
	_system.Update(0.5f, 16.0f);
	_system.Release();
	EXPECT_FALSE(_system.IsPressed());
	EXPECT_EQ(lookup.TryGet<const CreatureFightPress>(a), nullptr);
	// the fight ending drops a press still held
	ASSERT_TRUE(_system.Press(from, down));
	_system.AbortFight(a);
	EXPECT_FALSE(_system.IsPressed());
	EXPECT_EQ(lookup.TryGet<const CreatureFightPress>(a), nullptr);
}

TEST_F(CreatureFightSystemTest, KnockedOutItIsTakenHomeAtOnce)
{
	const auto creature = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	auto& registry = test::creature_world::World::Registry();
	registry.AssignState<CreatureKnockedOut>(creature, CreatureKnockedOut {.stage = CreatureKnockedOut::Stage::FadingOut,
	                                                                       .seconds = fight::k_FizzSeconds,
	                                                                       .home = glm::vec3(300.0f, 0.0f, 300.0f)});
	_world.teleported.clear();
	_system.ProcessTurn();
	ASSERT_EQ(_world.teleported.size(), 1u);
	EXPECT_EQ(_world.teleported.front(), creature);
	EXPECT_EQ(registry.Get<Transform>(creature).position, glm::vec3(300.0f, 0.0f, 300.0f));
	const auto& locomotion = registry.Get<CreatureLocomotion>(creature);
	EXPECT_EQ(locomotion.fromPosition, locomotion.toPosition);
	EXPECT_EQ(registry.Get<CreatureKnockedOut>(creature).stage, CreatureKnockedOut::Stage::FadingIn);
	EXPECT_TRUE(_system.IsKnockedOut(creature));
}

TEST(CreatureFightClock, ABlowLandsOnceAtTurnStepsAndAtFrameSteps)
{
	// 16 seconds of blows, at the turn's 100 ms and at three frames a turn
	const auto turns = PlayBlows(100.0f, 16000.0f);
	const auto frames = PlayBlows(100.0f / 3.0f, 16000.0f);
	// every blow lands once, at either rate
	EXPECT_EQ(turns.landed, turns.finished + (turns.landed > turns.finished ? 1 : 0));
	EXPECT_EQ(frames.landed, frames.finished + (frames.landed > frames.finished ? 1 : 0));
	// what goes past a blow's end is lost as the next one starts, so a blow takes the whole steps it needs: 16 turns
	// (1.6 s), or 46 frames (about 1.53 s). The turn-stepped duel is that much slower than one stepped by the frame.
	EXPECT_EQ(turns.finished, 10);
	EXPECT_NEAR(turns.lastBlowMs, 1600.0f, 1e-2f);
	EXPECT_NEAR(frames.lastBlowMs, 46.0f * 100.0f / 3.0f, 1e-1f);
	EXPECT_EQ(frames.finished, 10);
	EXPECT_EQ(frames.landed, 11);
	EXPECT_EQ(turns.landed, 10);
}
