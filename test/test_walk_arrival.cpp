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

#include <gtest/gtest.h>

#include "ECS/Components/WallHug.h"
#include "ECS/WalkArrival.h"
#include "ECS/WallHugRules.h"

using namespace openblack::ecs;
using openblack::ecs::components::MoveState;

namespace
{
/// A metre in whole map units, truncated
constexpr int32_t k_Metre = 6553;
} // namespace

TEST(WalkArrival, AStepIsATenthOfTheSpeedInMetresASecond)
{
	// 1 m/s is 655 whole units a turn (a tenth of a metre)
	const glm::ivec2 goal {100000, 100000};
	EXPECT_TRUE(walk_arrival::WithinAStep(goal + glm::ivec2 {654, 0}, goal, 1.0f));
	EXPECT_FALSE(walk_arrival::WithinAStep(goal + glm::ivec2 {655, 0}, goal, 1.0f));
	// Half a metre off is five steps away, not there
	EXPECT_FALSE(walk_arrival::WithinAStep(goal + glm::ivec2 {k_Metre / 2, 0}, goal, 1.0f));
	EXPECT_FALSE(walk_arrival::WithinAStep(goal + glm::ivec2 {0, k_Metre / 2}, goal, 1.0f));
	// Ten times faster, half a metre is within the step
	EXPECT_TRUE(walk_arrival::WithinAStep(goal + glm::ivec2 {k_Metre / 2, 0}, goal, 10.0f));
}

TEST(WalkArrival, AStandingWalkerIsOnlyThereOnThePointItself)
{
	const glm::ivec2 goal {100000, 100000};
	EXPECT_FALSE(walk_arrival::WithinAStep(goal, goal, 0.0f));
	EXPECT_FALSE(walk_arrival::WithinAStep(goal + glm::ivec2 {1, 0}, goal, 0.0f));
}

TEST(WalkArrival, AWalkStillUnderWayIsNotOver)
{
	const glm::ivec2 goal {100000, 100000};
	for (const auto state : {MoveState::Linear, MoveState::Orbit, MoveState::ExitCircle, MoveState::StepThrough})
	{
		EXPECT_FALSE(walk_arrival::WalkIsOver(state, goal + glm::ivec2 {300, 0}, goal));
		EXPECT_FALSE(walk_arrival::WalkIsOver(state, goal, goal));
	}
}

TEST(WalkArrival, WithinAStepTheWalkIsOverOnlyOnceOnTheGoal)
{
	const glm::ivec2 goal {100000, 100000};
	for (const auto state : {MoveState::FinalStep, MoveState::Arrived})
	{
		EXPECT_FALSE(walk_arrival::WalkIsOver(state, goal + glm::ivec2 {300, -200}, goal));
		EXPECT_FALSE(walk_arrival::WalkIsOver(state, goal + glm::ivec2 {0, 1}, goal));
		EXPECT_TRUE(walk_arrival::WalkIsOver(state, goal, goal));
	}
}

TEST(WalkArrival, NoWalkUnderWayIsOver)
{
	EXPECT_TRUE(walk_arrival::WalkIsOver(std::nullopt, {0, 0}, {5000, 5000}));
}

/// A fake straight walk, turn by turn as the walk takes it: a step towards the goal, and on coming within a step, the
/// last step onto it on the next turn. The state waiting on it looks after the walk each turn.
TEST(WalkArrival, TheWaitingStateEndsOnTheTurnTheWalkerLandsOnItsGoal)
{
	constexpr float k_Speed = 2.0f;
	const glm::ivec2 start {200000, 300000};
	const glm::ivec2 goal = start + glm::ivec2 {4 * k_Metre + 777, 0};
	const auto step = wall_hug::StepAlong(0, wall_hug::WholeSpeed(k_Speed));
	ASSERT_GT(step.x, 0);

	glm::ivec2 position = start;
	MoveState state = MoveState::Linear;
	int turnWithinStep = -1;
	int turnOver = -1;
	for (int turn = 1; turn < 100 && turnOver < 0; ++turn)
	{
		if (state == MoveState::FinalStep)
		{
			position = goal;
		}
		else
		{
			position += step;
			if (walk_arrival::WithinAStep(position, goal, k_Speed))
			{
				state = MoveState::FinalStep;
				turnWithinStep = turn;
			}
		}
		if (walk_arrival::WalkIsOver(state, position, goal))
		{
			turnOver = turn;
		}
	}
	ASSERT_GT(turnWithinStep, 0);
	EXPECT_EQ(turnOver, turnWithinStep + 1);
	EXPECT_EQ(position, goal);
}
