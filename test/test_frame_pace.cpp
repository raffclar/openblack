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

using namespace openblack;
using std::chrono::milliseconds;

TEST(FramePace, FastFramesStepSixtyTimesASecondWithAllTheirGameTime)
{
	frame_pace::Pace pace;
	int steps = 0;
	milliseconds moved {0};
	// A second of 1 ms frames
	for (int frame = 0; frame < 1000; ++frame)
	{
		const auto step = frame_pace::Step(pace, milliseconds(1), milliseconds(1));
		if (step.count() != 0)
		{
			++steps;
			moved += step;
			EXPECT_TRUE(step == milliseconds(16) || step == milliseconds(17));
		}
	}
	EXPECT_EQ(steps, 60);
	EXPECT_EQ(moved + pace.gameSinceStep, milliseconds(1000));
}

TEST(FramePace, SlowFramesStepOnceEachAndALongFrameLeavesNoStepsOwed)
{
	frame_pace::Pace pace;
	EXPECT_EQ(frame_pace::Step(pace, milliseconds(33), milliseconds(33)), milliseconds(33));
	EXPECT_EQ(frame_pace::Step(pace, milliseconds(40), milliseconds(80)), milliseconds(80));
	EXPECT_EQ(frame_pace::Step(pace, milliseconds(500), milliseconds(500)), milliseconds(500));
	EXPECT_LT(pace.sinceStep, frame_pace::Pace::k_StepInterval);
	EXPECT_EQ(frame_pace::Step(pace, milliseconds(1), milliseconds(1)), milliseconds(0));
	// The next step takes the game time owed; while the game is paused the frames bring none
	EXPECT_EQ(frame_pace::Step(pace, milliseconds(50), milliseconds(0)), milliseconds(1));
	EXPECT_EQ(frame_pace::Step(pace, milliseconds(50), milliseconds(0)), milliseconds(0));
}
