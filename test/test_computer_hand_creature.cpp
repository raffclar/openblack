/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ComputerHandFake.h"
#include "ECS/ComputerHand.h"
#include "ECS/ComputerHandSteps.h"

using namespace openblack;
using namespace openblack::computer_hand;

TEST(ComputerHandCreature, KindsOfThisPartThatAreNotMadeEndTheActionBadly)
{
	FakeWorld world;
	for (size_t kind = 0; kind < static_cast<size_t>(StepKind::_Count); ++kind)
	{
		const auto step = static_cast<StepKind>(kind);
		if (steps::OwnerOf(step) != steps::StepOwner::Creature || steps::Table().at(kind).made)
		{
			continue;
		}
		HandAction action;
		action.moveFirst = false;
		Add(action, step, {});
		EXPECT_FALSE(Process(action, world.view)) << kind;
		EXPECT_FALSE(Busy(action)) << kind;
	}
}

namespace
{

constexpr auto k_Creature = static_cast<entt::entity>(9);
constexpr auto k_Food = static_cast<entt::entity>(10);

} // namespace

TEST(ComputerHandCreature, HandGoesToTheHeadOfTheCreature)
{
	FakeWorld world;
	world.places[k_Creature] = {100.0f, 100.0f};
	HandAction action;
	action.moveFirst = false;
	action.hand = {100.0f, 0.0f, 80.0f};
	action.speed = 90.0f;
	Add(action, StepKind::MoveHandToCreatureHead, {Argument {.object = k_Creature}});
	ASSERT_TRUE(Process(action, world.view)); // the start aims at the head
	// The head is as high as the creature is tall, over its place
	EXPECT_FLOAT_EQ(action.target.x, 100.0f);
	EXPECT_FLOAT_EQ(action.target.y, 4.0f);
	EXPECT_FLOAT_EQ(action.target.z, 100.0f);
	// Each turn moves the hand 9 metres. It is at the head when it is 1 metre from it or nearer.
	ASSERT_TRUE(Process(action, world.view));
	EXPECT_EQ(action.part, 1);
	EXPECT_FLOAT_EQ(action.hand.z, 89.0f);
	// The hand floats as high over the land as the creature is tall
	EXPECT_FLOAT_EQ(action.hand.y, 24.0f);
	ASSERT_TRUE(Process(action, world.view));
	EXPECT_EQ(action.part, 1);
	ASSERT_TRUE(Process(action, world.view));
	EXPECT_FLOAT_EQ(action.hand.z, 100.0f);
	EXPECT_EQ(action.part, 2);
}

TEST(ComputerHandCreature, HandThrowsWhatItHoldsToTheCreature)
{
	FakeWorld world;
	world.places[k_Creature] = {120.0f, 100.0f};
	world.held = k_Food;
	HandAction action;
	action.moveFirst = false;
	action.hand = {100.0f, 30.0f, 100.0f};
	Add(action, StepKind::GiveObjectInHandToCreature, {Argument {.object = k_Creature}});
	for (int turn = 0; turn < 3; ++turn)
	{
		ASSERT_TRUE(Process(action, world.view));
	}
	EXPECT_FALSE(Busy(action));
	ASSERT_EQ(world.thrownAt.size(), 1u);
	const auto expected = ThrowVelocity({100.0f, 30.0f, 100.0f}, {120.0f, 20.0f, 100.0f});
	EXPECT_EQ(world.thrownAt[0], expected);
	EXPECT_TRUE(world.held == entt::null);
}

TEST(ComputerHandCreature, HandStaysOverTheCreatureUntilItsOrderIsDone)
{
	FakeWorld world;
	world.places[k_Creature] = {100.0f, 100.0f};
	world.speeds[k_Creature] = {.walk = 0.3f, .run = 0.6f};
	HandAction action;
	action.moveFirst = false;
	action.hand = {100.0f, 0.0f, 100.0f};
	Add(action, StepKind::ForceCreatureToPerformAction,
	    {Argument {.object = k_Creature}, Argument {.number = 162}, Argument {.object = k_Creature}});
	ASSERT_TRUE(Process(action, world.view));
	ASSERT_EQ(world.ordered.size(), 1u);
	EXPECT_EQ(world.ordered[0].action, 162u);
	EXPECT_EQ(world.ordered[0].object, k_Creature);
	for (int turn = 0; turn < 5; ++turn)
	{
		ASSERT_TRUE(Process(action, world.view));
		EXPECT_EQ(action.part, 1);
	}
	// Over the creature at two and a half times its height
	EXPECT_FLOAT_EQ(action.hand.y, 30.0f);
	world.forced.clear();
	ASSERT_TRUE(Process(action, world.view));
	EXPECT_EQ(action.part, 2);
}

TEST(ComputerHandCreature, AnOrderToAThingThatIsNotACreatureEnds)
{
	FakeWorld world;
	world.places[k_Food] = {100.0f, 100.0f};
	HandAction action;
	action.moveFirst = false;
	Add(action, StepKind::ForceCreatureToPerformAction,
	    {Argument {.object = k_Food}, Argument {.number = 17}, Argument {.object = k_Food}});
	for (int turn = 0; turn < 3; ++turn)
	{
		ASSERT_TRUE(Process(action, world.view));
	}
	EXPECT_FALSE(Busy(action));
	EXPECT_TRUE(world.ordered.empty());
}

TEST(ComputerHandCreature, GodMakesADesireDominant)
{
	FakeWorld world;
	HandAction action;
	action.moveFirst = false;
	action.counter = 7;
	Add(action, StepKind::ForceCreatureDominantDesire,
	    {Argument {.object = k_Creature}, Argument {.number = 4}, Argument {.value = 30.0f}});
	ASSERT_TRUE(Process(action, world.view));
	ASSERT_EQ(world.dominant.size(), 1u);
	EXPECT_EQ(world.dominant[0].creature, k_Creature);
	EXPECT_EQ(world.dominant[0].desire, 4);
	EXPECT_FLOAT_EQ(world.dominant[0].seconds, 30.0f);
	EXPECT_EQ(action.counter, 0u);
}

TEST(ComputerHandCreature, JourneySpeedRunsFarAndDrawsForNear)
{
	FakeWorld world;
	world.places[k_Creature] = {100.0f, 100.0f};
	world.speeds[k_Creature] = {.walk = 0.3f, .run = 0.6f};
	const auto journey = [&world](glm::vec3 place) {
		HandAction action;
		action.moveFirst = false;
		Add(action, StepKind::SetCreatureSpeedForJourney, {Argument {.object = k_Creature}, Argument {.place = place}});
		for (int turn = 0; turn < 3; ++turn)
		{
			EXPECT_TRUE(Process(action, world.view));
		}
		EXPECT_FALSE(Busy(action));
		return world.required.at(k_Creature);
	};
	// The creature stands at (100, 20, 100)
	EXPECT_FLOAT_EQ(journey({300.0f, 20.0f, 100.0f}), 0.6f * 1.5f);
	EXPECT_EQ(world.shortJourneyDraws, 0);
	EXPECT_FLOAT_EQ(journey({150.0f, 20.0f, 100.0f}), (0.6f + 0.3f) * 0.5f);
	EXPECT_EQ(world.shortJourneyDraws, 1);
}
