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

TEST(ComputerHandMoves, KindsOfThisPartThatAreNotMadeEndTheActionBadly)
{
	FakeWorld world;
	for (size_t kind = 0; kind < static_cast<size_t>(StepKind::_Count); ++kind)
	{
		const auto step = static_cast<StepKind>(kind);
		if (steps::OwnerOf(step) != steps::StepOwner::Moves || steps::Table().at(kind).made)
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

TEST(ComputerHand, HandMovesFarStepsThenItsSpeed)
{
	FakeWorld world;
	world.places[k_Icon] = {1000.0f, 0.0f};
	HandAction action;
	Add(action, StepKind::MoveHandToObject, {Argument {.object = k_Icon}});
	Process(action, world.view); // the start finds the target
	Process(action, world.view);
	EXPECT_FLOAT_EQ(action.hand.x, 100.0f);
	EXPECT_FLOAT_EQ(action.hand.y, 30.0f);
	// Between 100 and 300 metres away the hand moves 10 metres a turn
	action.hand.x = 750.0f;
	Process(action, world.view);
	EXPECT_FLOAT_EQ(action.hand.x, 760.0f);
	// Near the target it moves at its speed, 90 metres a second
	action.hand.x = 950.0f;
	Process(action, world.view);
	EXPECT_FLOAT_EQ(action.hand.x, 959.0f);
	// It arrives when it is no more than a metre away, and the step ends
	action.hand.x = 999.5f;
	Process(action, world.view);
	Process(action, world.view);
	EXPECT_FALSE(Busy(action));
}
