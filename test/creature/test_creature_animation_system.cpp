/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The creature animation system on a registry of its own: the fatness and breathing ease once a turn, the eyes blink by
// the turn on the C runtime's numbers, and nothing is posed without a mesh or a rig

#define LOCATOR_IMPLEMENTATIONS

#include <chrono>

#include <gtest/gtest.h>

#include "Common/GameRandom.h"
#include "Common/GameRandomTesting.h"
#include "Creature/CreatureAnimation.h"
#include "Creature/CreatureEyes.h"
#include "Creature/CreatureMorph.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Systems/Implementations/CreatureAnimationSystem.h"
#include "creature/CreatureSystemWorld.h"

using namespace openblack;
using namespace openblack::ecs::components;
using openblack::ecs::systems::CreatureAnimationSystem;

namespace
{
class CreatureAnimationSystemTest: public ::testing::Test
{
protected:
	test::creature_world::World _world;
	CreatureAnimationSystem _system;
};
} // namespace

TEST_F(CreatureAnimationSystemTest, ProcessTurnEasesTheFatnessAndTheBreathing)
{
	const auto creature = test::creature_world::World::MakeCreature();
	auto& registry = test::creature_world::World::Registry();
	registry.Get<Creature>(creature).fatness = 1.0f;
	auto& animation = registry.Get<CreatureAnimation>(creature);
	animation.breathPeriod = 1.0f;
	const auto shown = registry.Get<CreatureMorph>(creature).shownFatness;
	const auto size = registry.Get<Creature>(creature).size;

	_system.ProcessTurn();

	EXPECT_FLOAT_EQ(registry.Get<CreatureMorph>(creature).shownFatness, creature_morph::EaseFatness(shown, 1.0f));
	const auto resting = creature_animation::BreathPeriod(size);
	EXPECT_FLOAT_EQ(registry.Get<CreatureAnimation>(creature).breathPeriod,
	                creature_animation::EaseBreathPeriod(1.0f, resting, resting, 0.1f));
}

TEST_F(CreatureAnimationSystemTest, TheEyesBlinkByTheTurnOnTheRuntimesNumbers)
{
	const game_random::testing::ScopedState state;
	const auto creature = test::creature_world::World::MakeCreature();
	auto& registry = test::creature_world::World::Registry();
	const auto seeds = game_random::Current();
	const auto crt = game_random::crt::Seed();

	// the first blink comes after a second: ten turns of 100 ms
	for (int turn = 0; turn < 10; ++turn)
	{
		_system.ProcessTurn();
	}
	EXPECT_EQ(registry.Get<CreatureEyes>(creature).blink.state, creature_eyes::Blink::State::Closing);
	EXPECT_EQ(game_random::crt::Seed(), crt);
	// closed for two turns, opening for two, then open again for a time drawn from the C runtime's numbers
	for (int turn = 0; turn < 4; ++turn)
	{
		_system.ProcessTurn();
	}
	const auto& blink = registry.Get<CreatureEyes>(creature).blink;
	EXPECT_EQ(blink.state, creature_eyes::Blink::State::Open);
	EXPECT_GE(blink.timerMs, blink.intervalMs / 2);
	EXPECT_LT(blink.timerMs, blink.intervalMs + (blink.intervalMs / 2));
	EXPECT_NE(game_random::crt::Seed(), crt);
	// the game's synced and local streams are not drawn from
	EXPECT_EQ(game_random::Current().synced, seeds.synced);
	EXPECT_EQ(game_random::Current().local, seeds.local);
}

TEST_F(CreatureAnimationSystemTest, UpdateWithoutAMeshPosesNothing)
{
	const auto creature = test::creature_world::World::MakeCreature();
	auto& registry = test::creature_world::World::Registry();
	const auto before = registry.Get<CreatureMorph>(creature).revision;

	_system.Update(std::chrono::duration<float, std::milli>(33.0f));

	const auto& animation = registry.Get<CreatureAnimation>(creature);
	EXPECT_TRUE(animation.skeleton.Empty());
	EXPECT_TRUE(animation.boneMatrices.empty());
	EXPECT_EQ(registry.Get<CreatureMorph>(creature).revision, before);
}

TEST_F(CreatureAnimationSystemTest, NoRigMeansNoBoneOrDuration)
{
	const auto creature = test::creature_world::World::MakeCreature();
	EXPECT_FALSE(_system.AnimationDuration(creature, 0).has_value());
	EXPECT_FALSE(_system.BoneInAnimation(creature, 0, 0.0f, 0, false).has_value());
	// nor for something that is no creature
	const auto thing = test::creature_world::World::Registry().Create();
	EXPECT_FALSE(_system.AnimationDuration(thing, 0).has_value());
}
