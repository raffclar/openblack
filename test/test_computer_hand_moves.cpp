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

namespace
{

/// Does turns of the action until it is done, at most a number of turns. Gives the turns.
int Finish(HandAction& action, const World& world, int most = 1000)
{
	int turns = 0;
	while (Busy(action) && turns < most)
	{
		Process(action, world);
		++turns;
	}
	return turns;
}

} // namespace

TEST(ComputerHandMoves, SetHandToPlaceFloatsOverTheLandOrKeepsTheHeight)
{
	FakeWorld world;
	HandAction action;
	action.moveFirst = false;
	Add(action, StepKind::SetHandToPos, {Argument {.place = {300.0f, 2.0f, 400.0f}}});
	// Each step has three parts, one each turn
	EXPECT_EQ(Finish(action, world.view), 3);
	EXPECT_FLOAT_EQ(action.hand.x, 300.0f);
	EXPECT_FLOAT_EQ(action.hand.y, 30.0f);
	EXPECT_FLOAT_EQ(action.hand.z, 400.0f);
	action.moveFirst = false;
	Add(action, StepKind::SetHandToPosDirectY, {Argument {.place = {100.0f, 2.0f, 50.0f}}});
	EXPECT_EQ(Finish(action, world.view), 3);
	EXPECT_FLOAT_EQ(action.hand.y, 2.0f);
}

TEST(ComputerHandMoves, DirectMoveGoesStraightInThreeDirections)
{
	FakeWorld world;
	HandAction action;
	action.moveFirst = false;
	action.hand = {0.0f, 0.0f, 0.0f};
	Add(action, StepKind::MoveHandToPosDirectYInterpolation, {Argument {.place = {40.0f, 30.0f, 0.0f}}});
	Process(action, world.view); // the start finds the target
	Process(action, world.view);
	// The hand moves 9 metres a turn along the line to the target, and the land does not change its height
	EXPECT_NEAR(action.hand.x, 7.2f, 1.0e-5f);
	EXPECT_NEAR(action.hand.y, 5.4f, 1.0e-5f);
	// The hand arrives when the distance across the land is no more than a step: four more moves, the turn that
	// arrives and the end
	EXPECT_EQ(Finish(action, world.view), 6);
	EXPECT_FLOAT_EQ(action.hand.y, 30.0f);
	// A target straight above the hand is no distance across the land, so the hand goes there at once
	action.moveFirst = false;
	Add(action, StepKind::MoveHandToPosDirectYInterpolation, {Argument {.place = {40.0f, 90.0f, 0.0f}}});
	Process(action, world.view);
	Process(action, world.view);
	EXPECT_FLOAT_EQ(action.hand.y, 90.0f);
}

TEST(ComputerHandMoves, HandStopsWhereTheGodHasNoInfluence)
{
	FakeWorld world;
	world.influenceBorder = 25.0f;
	HandAction action;
	action.moveFirst = false;
	Add(action, StepKind::MoveHandToPosStopWhenOutOfInfluence, {Argument {.place = {100.0f, 0.0f, 0.0f}}});
	Process(action, world.view);
	// The hand moves 9 metres a turn: after three turns it is at 27 metres, past the border, and it stops there
	EXPECT_EQ(Finish(action, world.view), 4);
	EXPECT_FLOAT_EQ(action.hand.x, 27.0f);
}

TEST(ComputerHandMoves, NearMoveStopsNearThePlaceAndMovesAwayWhenTooNear)
{
	FakeWorld world;
	HandAction action;
	action.moveFirst = false;
	action.hand = {0.0f, 0.0f, 0.0f};
	Add(action, StepKind::MoveHandNearPos, {Argument {.place = {200.0f, 0.0f, 0.0f}}});
	Process(action, world.view);
	// The hand stops when it is no more than 80 metres away: after 14 turns of 9 metres, and the end
	EXPECT_EQ(Finish(action, world.view), 15);
	EXPECT_FLOAT_EQ(action.hand.x, 126.0f);
	// A hand too near the place moves to the side, by at most 1.4142 metres a turn
	action.hand = {195.0f, 0.0f, 0.0f};
	action.moveFirst = false;
	Add(action, StepKind::MoveHandNearPos, {Argument {.place = {200.0f, 0.0f, 0.0f}}});
	Process(action, world.view);
	Process(action, world.view);
	EXPECT_FLOAT_EQ(action.hand.x, 195.0f + k_NearPlaceSideStep);
	EXPECT_TRUE(Busy(action));
}

TEST(ComputerHandMoves, WaitEndsWhenTheThingIsNearOrAfterAMinute)
{
	constexpr auto k_Thing = static_cast<entt::entity>(9);
	FakeWorld world;
	world.places[k_Thing] = {500.0f, 0.0f};
	HandAction action;
	action.moveFirst = false;
	const std::vector<Argument> arguments {Argument {.object = k_Thing},
	                                       Argument {.value = 5.5f, .place = {500.0f, 24.0f, 3.0f}}};
	Add(action, StepKind::WaitForObjectToBeNearPos, arguments);
	EXPECT_EQ(Finish(action, world.view), 3);
	// A thing that does not come near ends the action badly after 600 turns of the wait
	world.places[k_Thing] = {900.0f, 0.0f};
	action.moveFirst = false;
	Add(action, StepKind::WaitForObjectToBeNearPos, arguments);
	int turns = 0;
	bool good = true;
	while (Busy(action) && turns < 1000)
	{
		good = Process(action, world.view);
		++turns;
	}
	EXPECT_EQ(turns, 602);
	EXPECT_FALSE(good);
}
