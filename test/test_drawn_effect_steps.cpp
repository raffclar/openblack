/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <chrono>

#include <gtest/gtest.h>

#include "Common/FramePace.h"
#include "Particles/DrawnEffectSteps.h"

using namespace openblack;
using std::chrono::milliseconds;

TEST(DrawnEffectSteps, FastFramesStepSixtyTimesASecondWithAllTheirGameTime)
{
	frame_pace::Pace pace;
	int steps = 0;
	float moved = 0.0f;
	// A second of 1 ms frames, as at a thousand frames a second
	for (int frame = 0; frame < 1000; ++frame)
	{
		if (const auto seconds = particles::drawn_steps::StepSeconds(pace, milliseconds(1), milliseconds(1), true))
		{
			++steps;
			moved += *seconds;
			EXPECT_TRUE(*seconds == 0.016f || *seconds == 0.017f);
		}
	}
	EXPECT_EQ(steps, 60);
	EXPECT_NEAR(moved + static_cast<float>(pace.gameSinceStep.count()) * 0.001f, 1.0f, 1e-4f);
}

TEST(DrawnEffectSteps, SlowFramesStepOnceEachByTheirGameTime)
{
	frame_pace::Pace pace;
	for (int frame = 0; frame < 10; ++frame)
	{
		const auto seconds = particles::drawn_steps::StepSeconds(pace, milliseconds(40), milliseconds(40), true);
		ASSERT_TRUE(seconds.has_value());
		EXPECT_FLOAT_EQ(*seconds, 0.04f);
	}
}

TEST(DrawnEffectSteps, APausedGameStillStepsByNoTime)
{
	frame_pace::Pace pace;
	const auto seconds = particles::drawn_steps::StepSeconds(pace, milliseconds(20), milliseconds(0), true);
	ASSERT_TRUE(seconds.has_value());
	EXPECT_EQ(*seconds, 0.0f);
}

TEST(DrawnEffectSteps, AStepInAFrameThatDoesNotDrawTheLandLosesItsGameTime)
{
	frame_pace::Pace pace;
	EXPECT_FALSE(particles::drawn_steps::StepSeconds(pace, milliseconds(20), milliseconds(20), false).has_value());
	// The next drawn frame steps only by its own game time
	const auto seconds = particles::drawn_steps::StepSeconds(pace, milliseconds(20), milliseconds(20), true);
	ASSERT_TRUE(seconds.has_value());
	EXPECT_FLOAT_EQ(*seconds, 0.02f);
}

TEST(DrawnEffectSteps, AFrameBeforeTheStepIsDueDoesNotStep)
{
	frame_pace::Pace pace;
	EXPECT_FALSE(particles::drawn_steps::StepSeconds(pace, milliseconds(5), milliseconds(5), true).has_value());
	EXPECT_FALSE(particles::drawn_steps::StepSeconds(pace, milliseconds(5), milliseconds(5), true).has_value());
	EXPECT_FALSE(particles::drawn_steps::StepSeconds(pace, milliseconds(5), milliseconds(5), true).has_value());
	const auto seconds = particles::drawn_steps::StepSeconds(pace, milliseconds(5), milliseconds(5), true);
	ASSERT_TRUE(seconds.has_value());
	EXPECT_FLOAT_EQ(*seconds, 0.02f);
}
