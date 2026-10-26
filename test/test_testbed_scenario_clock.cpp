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
#include <chrono>

#include <gtest/gtest.h>

#include "Debug/TestbedScenarioClock.h"

using namespace openblack::testbed_scenarios;
using namespace std::chrono_literals;

namespace
{

/// The steps that a run of frames of one length gives in a time, with the menu open over the paused game
uint32_t StepsIn(std::chrono::microseconds frame, std::chrono::microseconds total)
{
	ScenarioClock clock;
	uint32_t steps = 0;
	for (auto time = 0us; time + frame <= total; time += frame)
	{
		steps += clock.StepsOfFrame(frame, true);
	}
	return steps;
}

} // namespace

TEST(TestbedScenarioClock, AStepIsTheLengthOfATurn)
{
	EXPECT_EQ(ScenarioClock::k_Step, 100ms);
	EXPECT_FLOAT_EQ(ScenarioClock::k_StepSeconds, 0.1f);
}

TEST(TestbedScenarioClock, TheMenuGivesTheSameStepsAtEachFrameRate)
{
	// The frame times of the frame time check: in each case three seconds of frames give 30 steps, less the part of a
	// step that the last frame does not complete
	for (const auto frame : std::array {7000us, 16700us, 33000us, 50000us})
	{
		const auto total = frame * (3s / frame);
		EXPECT_EQ(StepsIn(frame, total), static_cast<uint32_t>(total / ScenarioClock::k_Step)) << frame.count();
	}
}

TEST(TestbedScenarioClock, AFrameKeepsWhatIsNotAFullStep)
{
	ScenarioClock clock;
	EXPECT_EQ(clock.StepsOfFrame(60ms, true), 0u);
	EXPECT_EQ(clock.StepsOfFrame(60ms, true), 1u);
	EXPECT_EQ(clock.StepsOfFrame(80ms, true), 1u);
	// A long frame gives each step that it holds
	EXPECT_EQ(clock.StepsOfFrame(250ms, true), 2u);
	EXPECT_EQ(clock.StepsOfFrame(50ms, true), 1u);
}

TEST(TestbedScenarioClock, NoStepWhileTheMenuDoesNotHoldThePausedGame)
{
	ScenarioClock clock;
	EXPECT_EQ(clock.StepsOfFrame(90ms, true), 0u);
	// The turns move the scenario on now, and the time of the frames is not kept for later
	EXPECT_EQ(clock.StepsOfFrame(500ms, false), 0u);
	EXPECT_EQ(clock.StepsOfFrame(90ms, true), 0u);
	EXPECT_EQ(clock.StepsOfFrame(10ms, true), 1u);
}

TEST(TestbedScenarioClock, ATurnOfTheTempleMovesOnOnlyWhileTheGameIsNotPaused)
{
	EXPECT_TRUE(ScenarioClock::TempleTurnMoves(false));
	EXPECT_FALSE(ScenarioClock::TempleTurnMoves(true));
}
