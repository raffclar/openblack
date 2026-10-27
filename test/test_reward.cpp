/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <numbers>

#include <gtest/gtest.h>

#include "Blast/CameraShake.h"
#include "ECS/RewardRules.h"

using namespace openblack;
using namespace openblack::ecs;

TEST(Reward, AChestFromTheSkyFallsAHundredAndFiftyMetresInThreeSecondsTurningHalfARoundASecond)
{
	EXPECT_FLOAT_EQ(reward::FallHeight(0.0f), 150.0f);
	EXPECT_FLOAT_EQ(reward::FallHeight(1.0f), 100.0f);
	EXPECT_FLOAT_EQ(reward::FallHeight(3.0f), 0.0f);
	EXPECT_FLOAT_EQ(reward::FallYaw(1.0f), std::numbers::pi_v<float>);
}

TEST(Reward, TheTurnsAloneMoveAChestThatFallsAndItLandsAtItsThirtySecondTurn)
{
	float seconds = 0.0f;
	int turns = 0;
	reward::FallTurn step;
	do
	{
		step = reward::StepFall(seconds);
		seconds = step.seconds;
		++turns;
	} while (!step.lands && turns < 100);
	// The clock adds a tenth of a second in each turn. In floats, thirty tenths give a little less than three seconds,
	// so the chest is still a little above the land at the thirty-first turn.
	EXPECT_EQ(turns, 32);
	EXPECT_LE(step.height, 0.0f);
	// The first turn puts it where its clock starts: at the top, not turned
	const auto first = reward::StepFall(0.0f);
	EXPECT_FLOAT_EQ(first.height, 150.0f);
	EXPECT_FLOAT_EQ(first.yaw, 0.0f);
	EXPECT_FLOAT_EQ(first.seconds, 0.1f);
	EXPECT_FALSE(first.lands);
}

TEST(Reward, TheFramesDrawAChestThatFallsBetweenThePlacesOfItsLastTwoTurns)
{
	// After the turn that put it at a clock of 1 second, its clock is 1.1 seconds
	const auto turn = reward::StepFall(1.0f);
	EXPECT_FLOAT_EQ(reward::DrawnFallClock(turn.seconds, 0.0f), 1.0f);
	EXPECT_FLOAT_EQ(reward::DrawnFallClock(turn.seconds, 0.5f), 1.05f);
	EXPECT_FLOAT_EQ(reward::DrawnFallClock(turn.seconds, 1.0f), 1.1f);
	// Before its first turn the frames draw it at the top
	EXPECT_FLOAT_EQ(reward::DrawnFallClock(0.0f, 0.7f), 0.0f);
	// Just before the turn that lands it, the frames draw it on the land, never under it
	EXPECT_FLOAT_EQ(reward::DrawnFallHeight(3.05f), 0.0f);
	EXPECT_FLOAT_EQ(reward::DrawnFallHeight(2.95f), 2.5f);
}

TEST(Reward, ItsDustSpreadsAboutItInSandColoursAndGrowsAsItFades)
{
	const auto dust = reward::MakeDust([](float a, float b) { return b + 0.0f * a; });
	EXPECT_EQ(dust.size(), 27u);
	EXPECT_FLOAT_EQ(dust[0].offset.x, 2.0f);
	EXPECT_FLOAT_EQ(dust[0].offset.y, 2.0f);
	EXPECT_EQ(dust[0].frame, 15u);
	EXPECT_EQ(dust[0].rgb, (std::array<uint8_t, 3> {212, 180, 140}));
	const auto fresh = reward::LookOfDust(2500.0f);
	EXPECT_EQ(fresh.alpha, 255);
	EXPECT_FLOAT_EQ(fresh.size, 0.7f);
	const auto gone = reward::LookOfDust(0.0f);
	EXPECT_EQ(gone.alpha, 0);
	EXPECT_FLOAT_EQ(gone.size, 2.5f);
}

TEST(Reward, ItsDustIsPlacedAcrossThenUpThenAlong)
{
	// Each draw is told apart by its order
	float next = 0.0f;
	const auto dust = reward::MakeDust([&next](float /*from*/, float /*to*/) { return next += 1.0f; });
	EXPECT_FLOAT_EQ(dust[0].offset.z, 1.0f);
	EXPECT_FLOAT_EQ(dust[0].offset.y, 2.0f);
	EXPECT_FLOAT_EQ(dust[0].offset.x, 3.0f);
}

TEST(Reward, ItsThumpShakesTheCameraNearTheWorldsOrigin)
{
	std::vector<camera_shake::Shake> shakes {{.position = glm::vec3(0.0f),
	                                          .radius = reward::k_ShakeRadius,
	                                          .strength = reward::k_ShakeStrength,
	                                          .milliseconds = reward::k_ShakeMilliseconds,
	                                          .millisecondsLeft = reward::k_ShakeMilliseconds,
	                                          .verticalOnly = false}};
	EXPECT_FLOAT_EQ(camera_shake::Amplitude(shakes, {100.0f, 0.0f, 100.0f}), 1.0f);
	EXPECT_FLOAT_EQ(camera_shake::Amplitude(shakes, {2560.0f, 0.0f, 2560.0f}), 0.0f);
}
