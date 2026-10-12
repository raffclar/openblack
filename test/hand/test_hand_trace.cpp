/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <vector>

#include <gtest/gtest.h>

#include "Hand/HandTrace.h"

using namespace openblack::hand_trace;

namespace
{
/// A frame with the hand showing at a screen point and the cursor at another
Frame At(uint32_t number, glm::vec2 screen, glm::vec2 cursor)
{
	Frame frame;
	frame.frame = number;
	frame.screen = screen;
	frame.cursor = cursor;
	return frame;
}
} // namespace

TEST(HandTrace, KeepsNothingUntilStarted)
{
	HandTrace trace;
	trace.BeginFrame(1, 0.016f);
	trace.Note("navigation", {1.0f, 2.0f, 3.0f});
	EXPECT_EQ(trace.Current(), nullptr);
	trace.EndFrame();
	EXPECT_TRUE(trace.Frames().empty());
}

TEST(HandTrace, KeepsTheLastFramesOldestFirstWithTheirWritersInOrder)
{
	HandTrace trace;
	trace.Enable(3);
	for (uint32_t number = 1; number <= 5; ++number)
	{
		trace.BeginFrame(number, 0.016f);
		trace.Note("navigation", {static_cast<float>(number), 0.0f, 0.0f});
		trace.Note("grab", {static_cast<float>(number), 1.0f, 0.0f});
		trace.EndFrame();
	}
	const auto frames = trace.Frames();
	ASSERT_EQ(frames.size(), 3u);
	EXPECT_EQ(frames[0].frame, 3u);
	EXPECT_EQ(frames[2].frame, 5u);
	ASSERT_EQ(frames[2].Writes().size(), 2u);
	EXPECT_EQ(frames[2].Writes()[0].writer, "navigation");
	EXPECT_EQ(frames[2].Writes()[1].writer, "grab");
	EXPECT_EQ(frames[2].Writes()[1].position, glm::vec3(5.0f, 1.0f, 0.0f));
}

TEST(HandTrace, CountsTheWritesItHasNoRoomFor)
{
	HandTrace trace;
	trace.Enable(1);
	trace.BeginFrame(1, 0.016f);
	for (size_t i = 0; i < k_MaxWrites + 2; ++i)
	{
		trace.Note("again", glm::vec3(0.0f));
	}
	trace.EndFrame();
	const auto frames = trace.Frames();
	ASSERT_EQ(frames.size(), 1u);
	EXPECT_EQ(frames[0].Writes().size(), k_MaxWrites);
	EXPECT_EQ(frames[0].writesLost, 2u);
}

TEST(HandTrace, StoppingForgetsTheFrames)
{
	HandTrace trace;
	trace.Enable(4);
	trace.BeginFrame(1, 0.016f);
	trace.EndFrame();
	trace.Disable();
	EXPECT_FALSE(trace.Enabled());
	EXPECT_TRUE(trace.Frames().empty());
}

TEST(HandTrace, AHandFollowingTheCursorIsNoJump)
{
	const std::vector<Frame> frames {At(1, {100.0f, 100.0f}, {100.0f, 100.0f}), At(2, {140.0f, 100.0f}, {140.0f, 100.0f}),
	                                 At(3, {180.0f, 100.0f}, {180.0f, 100.0f})};
	EXPECT_TRUE(FindJumps(frames, 12.0f, 4).empty());
}

TEST(HandTrace, AHandBlinkingAwayAndBackIsAJumpThatCameBack)
{
	std::vector<Frame> frames {At(1, {100.0f, 100.0f}, {100.0f, 100.0f}), At(2, {100.0f, 160.0f}, {100.0f, 100.0f}),
	                           At(3, {100.0f, 104.0f}, {100.0f, 100.0f})};
	frames[1].writes[0] = {.writer = "stage fade", .position = glm::vec3(0.0f)};
	frames[1].writeCount = 1;
	const auto jumps = FindJumps(frames, 12.0f, 4);
	// Going away and coming back are both jumps; the first came back
	ASSERT_EQ(jumps.size(), 2u);
	EXPECT_EQ(jumps[0].frame, 2u);
	EXPECT_NEAR(jumps[0].pixels, 60.0f, 1e-4f);
	EXPECT_TRUE(jumps[0].cameBack);
	EXPECT_EQ(jumps[0].writer, "stage fade");
	EXPECT_EQ(jumps[1].frame, 3u);
}

TEST(HandTrace, AHandMovingAwayForGoodIsAJumpThatDidNotComeBack)
{
	const std::vector<Frame> frames {At(1, {100.0f, 100.0f}, {100.0f, 100.0f}), At(2, {300.0f, 100.0f}, {100.0f, 100.0f}),
	                                 At(3, {300.0f, 100.0f}, {100.0f, 100.0f})};
	const auto jumps = FindJumps(frames, 12.0f, 4);
	ASSERT_EQ(jumps.size(), 1u);
	EXPECT_FALSE(jumps[0].cameBack);
}

TEST(HandTrace, FramesOffTheScreenAreSkipped)
{
	std::vector<Frame> frames {At(1, {100.0f, 100.0f}, {100.0f, 100.0f}), At(2, {900.0f, 100.0f}, {100.0f, 100.0f})};
	frames[1].screen.reset();
	EXPECT_TRUE(FindJumps(frames, 12.0f, 4).empty());
}
