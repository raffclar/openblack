/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "ECS/Components/HandStillness.h"
#include "Hand/HandStillness.h"

using openblack::ecs::components::HandStillness;
using openblack::hand_stillness::Step;

// Each turn the hand stays put counts one more
TEST(HandStillness, CountsTheTurnsTheHandStaysPut)
{
	HandStillness stillness {.stillTurns = 0, .drift = glm::vec3(0.0f), .last = glm::vec3(10.0f, 2.0f, 5.0f)};
	for (int turn = 0; turn < 3; ++turn)
	{
		Step(stillness, glm::vec3(10.0f, 2.0f, 5.0f));
	}
	EXPECT_EQ(stillness.stillTurns, 3u);
}

// Small moves add up: once they come to a length of one the count starts again
TEST(HandStillness, SmallMovesAddUpToAMove)
{
	HandStillness stillness {};
	Step(stillness, glm::vec3(0.4f, 0.0f, 0.0f));
	Step(stillness, glm::vec3(0.8f, 0.0f, 0.0f));
	EXPECT_EQ(stillness.stillTurns, 2u);
	Step(stillness, glm::vec3(1.0f, 0.0f, 0.0f));
	EXPECT_EQ(stillness.stillTurns, 0u);
	EXPECT_EQ(stillness.drift, glm::vec3(0.0f));
	Step(stillness, glm::vec3(1.0f, 0.0f, 0.0f));
	EXPECT_EQ(stillness.stillTurns, 1u);
}

// Moves that go back cancel each other: the hand that shakes in place stays still
TEST(HandStillness, MovesThatGoBackCancel)
{
	HandStillness stillness {};
	for (int turn = 0; turn < 10; ++turn)
	{
		Step(stillness, glm::vec3(turn % 2 == 0 ? 0.9f : 0.0f, 0.0f, 0.0f));
	}
	EXPECT_EQ(stillness.stillTurns, 10u);
}

// The velocity goes six tenths of the way to the movement of each turn, made a velocity of ten turns a second
TEST(HandStillness, TheVelocityFollowsTheMovementOfEachTurn)
{
	HandStillness stillness {};
	Step(stillness, glm::vec3(1.0f, 0.0f, 0.0f));
	EXPECT_FLOAT_EQ(stillness.velocity.x, 6.0f);
	Step(stillness, glm::vec3(1.0f, 0.0f, 0.0f));
	EXPECT_FLOAT_EQ(stillness.velocity.x, 2.4f);
	EXPECT_FLOAT_EQ(stillness.velocity.y, 0.0f);
}
