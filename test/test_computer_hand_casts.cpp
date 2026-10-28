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

TEST(ComputerHandCasts, KindsOfThisPartThatAreNotMadeEndTheActionBadly)
{
	FakeWorld world;
	for (size_t kind = 0; kind < static_cast<size_t>(StepKind::_Count); ++kind)
	{
		const auto step = static_cast<StepKind>(kind);
		if (steps::OwnerOf(step) != steps::StepOwner::Casts || steps::Table().at(kind).made)
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

TEST(ComputerHand, ChantStopsWhenTheWorshippersCannotStart)
{
	FakeWorld world;
	HandAction action;
	Add(action, StepKind::MakeWorshippersChantForSpellNoWait,
	    {Argument {.number = static_cast<int32_t>(MagicType::Food)}, Argument {.object = k_Icon}});
	EXPECT_TRUE(Process(action, world.view)); // the start
	// It tries for 6 whole seconds, then the action ends badly
	for (int turn = 0; turn < 59; ++turn)
	{
		ASSERT_TRUE(Process(action, world.view)) << turn;
	}
	EXPECT_FALSE(Process(action, world.view));
	EXPECT_FALSE(Busy(action));
}

TEST(ComputerHand, ChantWaitsForTheFullCharge)
{
	FakeWorld world;
	world.startWorks = true;
	world.needed = 10.0f;
	HandAction action;
	Add(action, StepKind::MakeWorshippersChantForSpell,
	    {Argument {.number = static_cast<int32_t>(MagicType::Food)}, Argument {.object = k_Icon}});
	Process(action, world.view);
	Process(action, world.view);
	EXPECT_EQ(action.part, 2);
	Process(action, world.view);
	EXPECT_TRUE(Busy(action));
	world.needed = 0.0f;
	Process(action, world.view);
	EXPECT_FALSE(Busy(action));
}

TEST(ComputerHand, CastKeepsTheHandOverTheThing)
{
	FakeWorld world;
	world.places[k_Store] = {0.0f, 0.0f};
	HandAction action;
	action.moveFirst = false;
	Add(action, StepKind::CastSpellAtObject,
	    {Argument {.object = k_Store}, Argument {.number = static_cast<int32_t>(MagicType::Food)},
	     Argument {.object = k_Icon}});
	Process(action, world.view);
	Process(action, world.view);
	ASSERT_EQ(world.cast.size(), 1u);
	// The hand floats over the thing at two and a half times its height
	EXPECT_FLOAT_EQ(action.hand.y, 30.0f);
	EXPECT_EQ(action.counter, 5u);
	for (int turn = 0; turn < 4; ++turn)
	{
		Process(action, world.view);
	}
	EXPECT_TRUE(Busy(action));
	Process(action, world.view);
	EXPECT_FALSE(Busy(action));
	// A miracle that is not made ends the action badly
	world.castWorks = false;
	Add(action, StepKind::CastSpellAtObject, {Argument {.object = k_Store}, Argument {.number = 14}});
	action.moveFirst = false;
	Process(action, world.view);
	EXPECT_FALSE(Process(action, world.view));
}

TEST(ComputerHand, GestureDrawsTheShapeInTheAirThenShowsItOnTheLand)
{
	FakeWorld world;
	HandAction action;
	action.hand = {100.0f, 30.0f, 200.0f};
	Add(action, StepKind::MakeGesture, {Argument {.number = static_cast<int32_t>(MagicType::Food)}, Argument {.number = 0}});
	// The start: the hand goes to the start of the shape, which is the corner here
	EXPECT_TRUE(Process(action, world.view));
	EXPECT_EQ(action.hand, glm::vec3(100.0f, 30.0f, 200.0f));
	// Half as long as the circle: 15 turns. The shape's x goes east and its z goes up.
	const float share = GestureSharePerTurn(1.0f, 2.0f);
	EXPECT_FLOAT_EQ(share, 1.0f / 15.0f);
	EXPECT_TRUE(Process(action, world.view));
	EXPECT_FLOAT_EQ(action.progress, share);
	EXPECT_FLOAT_EQ(action.hand.x, share * k_GestureSize + 100.0f);
	EXPECT_FLOAT_EQ(action.hand.y, share * k_GestureSize + 30.0f);
	EXPECT_FLOAT_EQ(action.hand.z, 200.0f);
	int turns = 1;
	while (action.part == 1)
	{
		ASSERT_TRUE(Process(action, world.view));
		++turns;
		ASSERT_LT(turns, 40);
	}
	EXPECT_EQ(turns, 15);
	EXPECT_FLOAT_EQ(action.progress, 1.0f);
	EXPECT_TRUE(world.shown.empty());
	// The end: the hand goes back, and the shape of the gesture that opens the choice shows on the land there
	EXPECT_TRUE(Process(action, world.view));
	EXPECT_FALSE(Busy(action));
	EXPECT_EQ(action.hand, glm::vec3(100.0f, 30.0f, 200.0f));
	ASSERT_EQ(world.shown.size(), 1u);
	EXPECT_EQ(world.shown[0], 1u);
	EXPECT_EQ(world.shownAt, action.hand);
	EXPECT_FLOAT_EQ(world.shownSize, k_GestureSize);
}

TEST(ComputerHand, GestureWithNoShapeTakesOneTurn)
{
	EXPECT_FLOAT_EQ(GestureSharePerTurn(0.0f, 2.0f), 1.0f);
	EXPECT_FLOAT_EQ(GestureSharePerTurn(3.0f, 0.0f), 1.0f);
	EXPECT_FLOAT_EQ(GestureSharePerTurn(4.0f, 2.0f), 1.0f / 60.0f);
}
