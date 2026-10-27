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

TEST(ComputerHandObjects, KindsOfThisPartThatAreNotMadeEndTheActionBadly)
{
	FakeWorld world;
	for (size_t kind = 0; kind < static_cast<size_t>(StepKind::_Count); ++kind)
	{
		const auto step = static_cast<StepKind>(kind);
		if (steps::OwnerOf(step) != steps::StepOwner::Objects || steps::Table().at(kind).made)
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

TEST(ComputerHand, HandTakesAThingAndDropsItOnTheStore)
{
	constexpr auto k_Field = static_cast<entt::entity>(7);
	FakeWorld world;
	world.places[k_Field] = {100.0f, 100.0f};
	world.places[k_Store] = {100.0f, 102.0f};
	HandAction action;
	action.hand = {100.0f, 0.0f, 100.0f};
	Add(action, StepKind::PickUpObject, {Argument {.object = k_Field}, Argument {.value = 90.0f}});
	Add(action, StepKind::DropObjectInHandOnObject, {Argument {.object = k_Store}});
	// Each step gets a move to its thing first: three parts of the move, then three of the step
	for (int turn = 0; turn < 20 && Busy(action); ++turn)
	{
		ASSERT_TRUE(Process(action, world.view));
	}
	EXPECT_FALSE(Busy(action));
	ASSERT_EQ(world.usedOn.size(), 1u);
	EXPECT_EQ(world.usedOn[0], k_Store);
	EXPECT_TRUE(world.held == entt::null);
}

TEST(ComputerHand, FullHandPutsDownFirstAndAMissingThingEndsBadly)
{
	constexpr auto k_Field = static_cast<entt::entity>(7);
	constexpr auto k_Other = static_cast<entt::entity>(8);
	FakeWorld world;
	world.places[k_Field] = {100.0f, 100.0f};
	world.held = k_Other;
	HandAction action;
	action.hand = {100.0f, 0.0f, 100.0f};
	action.moveFirst = false;
	Add(action, StepKind::PickUpObject, {Argument {.object = k_Field}});
	Process(action, world.view); // the start
	Process(action, world.view); // the hand is full: it puts down what it holds
	EXPECT_EQ(world.putDown, 1);
	EXPECT_EQ(action.part, 1);
	Process(action, world.view);
	EXPECT_EQ(world.held, k_Field);
	// A thing that is gone ends the action badly
	Clear(action);
	world.held = entt::null;
	action.moveFirst = false;
	Add(action, StepKind::PickUpObject, {Argument {.object = static_cast<entt::entity>(99)}});
	Process(action, world.view);
	EXPECT_FALSE(Process(action, world.view));
}

TEST(ComputerHand, VillagerGoesToThePlaceWaitsAndIsDropped)
{
	constexpr auto k_Villager = static_cast<entt::entity>(9);
	FakeWorld world;
	world.places[k_Villager] = {100.0f, 100.0f};
	HandAction action;
	action.hand = {100.0f, 0.0f, 100.0f};
	action.moveFirst = false;
	const glm::vec3 gate {100.0f, 20.0f, 130.0f};
	Add(action, StepKind::PickUpObject, {Argument {.object = k_Villager}});
	Add(action, StepKind::MoveHandToPos, {Argument {.place = gate}, Argument {.value = 900.0f}});
	Add(action, StepKind::Wait, {Argument {.value = 1.0f}});
	Add(action, StepKind::DropObjectInHandAtPos, {Argument {.place = gate}});
	// The pick up has no move in front of it here; its three parts take three turns
	for (int turn = 0; turn < 3; ++turn)
	{
		Process(action, world.view);
	}
	EXPECT_EQ(world.held, k_Villager);
	ASSERT_EQ(action.steps.front().kind, StepKind::MoveHandToPos);
	// The move gets no move of its own. At 90 metres a second the hand covers 30 metres in four steps of 9 and the rest.
	Process(action, world.view); // the start
	for (int turn = 0; turn < 3; ++turn)
	{
		Process(action, world.view);
		EXPECT_EQ(action.part, 1);
	}
	EXPECT_FLOAT_EQ(action.hand.z, 127.0f);
	Process(action, world.view);
	EXPECT_EQ(action.part, 2);
	EXPECT_FLOAT_EQ(action.hand.z, 130.0f);
	// The hand floats over the land at the place
	EXPECT_FLOAT_EQ(action.hand.y, 30.0f);
	Process(action, world.view); // the end of the move
	ASSERT_EQ(action.steps.front().kind, StepKind::Wait);
	// The wait takes ten turns of the second, after its start
	Process(action, world.view);
	EXPECT_EQ(action.counter, 10u);
	for (int turn = 0; turn < 9; ++turn)
	{
		Process(action, world.view);
		EXPECT_EQ(action.part, 1);
	}
	Process(action, world.view);
	EXPECT_EQ(action.part, 2);
	Process(action, world.view);
	// The drop gets the move to the place first, which ends at once
	ASSERT_EQ(action.steps.front().kind, StepKind::DropObjectInHandAtPos);
	for (int turn = 0; turn < 8 && Busy(action); ++turn)
	{
		Process(action, world.view);
	}
	EXPECT_FALSE(Busy(action));
	ASSERT_EQ(world.droppedAt.size(), 1u);
	EXPECT_TRUE(world.held == entt::null);
}
