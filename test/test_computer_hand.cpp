/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ComputerHandFake.h"
#include "ECS/ComputerHand.h"
#include "ECS/ComputerHandSteps.h"

using namespace openblack;
using namespace openblack::computer_hand;

TEST(ComputerHand, EachKindOfStepHasOneOwnerPart)
{
	// Each owner puts only its own kinds in the table
	const std::array adders {steps::AddCoreSteps, steps::AddMoveSteps,     steps::AddObjectSteps,
	                         steps::AddCastSteps, steps::AddCreatureSteps, steps::AddTotemSteps};
	const std::array owners {steps::StepOwner::Core,  steps::StepOwner::Moves,    steps::StepOwner::Objects,
	                         steps::StepOwner::Casts, steps::StepOwner::Creature, steps::StepOwner::Totem};
	for (size_t i = 0; i < adders.size(); ++i)
	{
		steps::StepTable table {};
		adders.at(i)(table);
		for (size_t kind = 0; kind < table.size(); ++kind)
		{
			if (table.at(kind).made)
			{
				EXPECT_EQ(steps::OwnerOf(static_cast<StepKind>(kind)), owners.at(i)) << kind;
			}
		}
	}
	// The steps that the actions of the god use today are in the table
	for (const auto kind :
	     {StepKind::PickUpObject, StepKind::MoveHandToPos, StepKind::MoveHandToObject, StepKind::DropObjectInHandAtPos,
	      StepKind::CastSpellAtObject, StepKind::MakeGesture, StepKind::Wait, StepKind::DropObjectInHandOnObject,
	      StepKind::MakeWorshippersChantForSpell, StepKind::MakeWorshippersChantForSpellNoWait})
	{
		EXPECT_TRUE(steps::Table().at(static_cast<size_t>(kind)).made) << static_cast<int>(kind);
	}
}

TEST(ComputerHand, EachKindOfStepThatTheGameAddsIsMade)
{
	// The game never adds a step to say a text or to lower a totem, so these two kinds are not made
	for (size_t kind = 0; kind < static_cast<size_t>(StepKind::_Count); ++kind)
	{
		const auto step = static_cast<StepKind>(kind);
		const bool neverAdded = step == StepKind::SayText || step == StepKind::LowerTotem;
		EXPECT_EQ(steps::Table().at(kind).made, !neverAdded) << kind;
	}
}

TEST(ComputerHand, KindsOfThisPartThatAreNotMadeEndTheActionBadly)
{
	FakeWorld world;
	for (size_t kind = 0; kind < static_cast<size_t>(StepKind::_Count); ++kind)
	{
		const auto step = static_cast<StepKind>(kind);
		if (steps::OwnerOf(step) != steps::StepOwner::Core || steps::Table().at(kind).made)
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

TEST(ComputerHand, StepGetsTheMoveThatBringsTheHandToIt)
{
	FakeWorld world;
	world.places[k_Store] = {50.0f, 0.0f};
	HandAction action;
	Add(action, StepKind::CastSpellAtObject,
	    {Argument {.object = k_Store}, Argument {.number = static_cast<int32_t>(MagicType::Food)},
	     Argument {.object = k_Icon}});
	EXPECT_TRUE(Process(action, world.view));
	ASSERT_EQ(action.steps.size(), 2u);
	EXPECT_EQ(action.steps[0].kind, StepKind::MoveHandToObject);
	EXPECT_EQ(action.steps[0].arguments[0].object, k_Store);
	// An angry miracle is cast from near the thing
	HandAction angry;
	Add(angry, StepKind::CastSpellAtObject,
	    {Argument {.object = k_Store}, Argument {.number = static_cast<int32_t>(MagicType::Fireball)}});
	Process(angry, world.view);
	EXPECT_EQ(angry.steps[0].kind, StepKind::MoveHandNearObject);
}

TEST(ComputerHand, MapPositionFollowsTheHandEachTurn)
{
	FakeWorld world;
	HandAction action;
	EXPECT_EQ(action.coords.x, 0x00808000);
	action.hand = {100.0f, 0.0f, 200.0f};
	Process(action, world.view);
	EXPECT_EQ(action.coords.x, map_coords::ToFixed(100.0f));
	EXPECT_EQ(action.coords.z, map_coords::ToFixed(200.0f));
}

TEST(ComputerHand, CameraFollowsTheHandButNotOnTheTurnOfTheInsertedMove)
{
	FakeWorld world;
	world.places[k_Icon] = {1000.0f, 1000.0f};
	HandAction action;
	action.hand = {900.0f, 0.0f, 1000.0f};
	Add(action, StepKind::CastSpellAtObject,
	    {Argument {.object = k_Icon}, Argument {.number = static_cast<int32_t>(MagicType::Food)}, Argument {.object = k_Icon}});
	// The turn that puts the move in front leaves the camera where it was
	Process(action, world.view);
	EXPECT_EQ(action.camera, glm::vec3(0.0f));
	Process(action, world.view); // the start of the move
	Process(action, world.view); // the first step of the move
	EXPECT_EQ(action.camera, action.hand);
	EXPECT_GT(action.camera.x, 900.0f);
}
