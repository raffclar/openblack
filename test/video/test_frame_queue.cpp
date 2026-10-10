/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <memory>
#include <set>
#include <vector>

#include <BinkDecoder.h>
#include <BinkFile.h>
#include <gtest/gtest.h>

#include "BinkTestFiles.h"
#include "Video/FrameQueue.h"

using namespace openblack;
using namespace openblack::video;

namespace
{
/// A 16x16 video whose frame i is a flat grey of 16 + i
bink::FrameReader MakeReader(uint32_t frames)
{
	std::vector<std::vector<uint8_t>> packets;
	for (uint32_t i = 0; i < frames; ++i)
	{
		packets.push_back(bink::test::FillFrame(static_cast<uint8_t>(16 + i), 128, 128));
	}
	auto file = bink::BinkFile::Parse(bink::test::MakeBik(packets));
	EXPECT_TRUE(file.has_value());
	auto reader = bink::FrameReader::Create(std::make_shared<const bink::BinkFile>(std::move(*file)));
	EXPECT_TRUE(reader.has_value());
	return std::move(*reader);
}
} // namespace

TEST(FrameQueue, InlineDecodesUpToTheFrameAskedFor)
{
	FrameQueue queue(MakeReader(10), DecodeMode::Inline);
	const auto* first = queue.Latest(0);
	ASSERT_NE(first, nullptr);
	EXPECT_EQ(first->frame, 0u);
	EXPECT_EQ(first->goodFrames, 1u);
	EXPECT_EQ(first->planes[0][0], 16);
	EXPECT_EQ(first->strides[0], 16u);
	// Frames skipped over by a stall are still decoded, in order: only the last is shown
	const auto* later = queue.Latest(6);
	ASSERT_NE(later, nullptr);
	EXPECT_EQ(later->frame, 6u);
	EXPECT_EQ(later->goodFrames, 7u);
	EXPECT_EQ(later->planes[0][0], 16 + 6);
	// Past the end the last frame stays
	const auto* last = queue.Latest(40);
	ASSERT_NE(last, nullptr);
	EXPECT_EQ(last->frame, 9u);
	EXPECT_EQ(last->planes[0][0], 16 + 9);
}

TEST(FrameQueue, TheWorkerDecodesAheadInOrder)
{
	FrameQueue queue(MakeReader(30), DecodeMode::Worker);
	std::set<const DecodedFrame*> buffers;
	for (uint32_t frame = 0; frame < 30; ++frame)
	{
		queue.WaitFor(frame);
		const auto* shown = queue.Latest(frame);
		ASSERT_NE(shown, nullptr);
		EXPECT_EQ(shown->frame, frame);
		EXPECT_EQ(shown->goodFrames, frame + 1);
		EXPECT_EQ(shown->planes[0][0], 16 + frame);
		buffers.insert(shown);
	}
	// The same few buffers over and over
	EXPECT_LE(buffers.size(), FrameQueue::k_Slots);
}

TEST(FrameQueue, NeverWaitsForAFrameNotYetDecoded)
{
	FrameQueue queue(MakeReader(30), DecodeMode::Worker);
	queue.WaitFor(0);
	// Asked for a frame far ahead, it gives the newest decoded at once, never one past the frame asked for
	const auto* shown = queue.Latest(25);
	ASSERT_NE(shown, nullptr);
	EXPECT_LE(shown->frame, 25u);
	// Each call lets the frames before the one shown go, and the worker catches up a few buffers at a time
	auto previous = shown->frame;
	for (int i = 0; i < 30 && shown->frame < 25; ++i)
	{
		queue.WaitFor(25);
		shown = queue.Latest(25);
		ASSERT_NE(shown, nullptr);
		EXPECT_GE(shown->frame, previous);
		EXPECT_LE(shown->frame, 25u);
		previous = shown->frame;
	}
	EXPECT_EQ(shown->frame, 25u);
	EXPECT_EQ(shown->planes[0][0], 16 + 25);
}

TEST(FrameQueue, StopsWhileItsBuffersAreFull)
{
	// Never asked for a frame: the worker fills its buffers and waits, and the queue still goes at once
	const auto queue = std::make_unique<FrameQueue>(MakeReader(30), DecodeMode::Worker);
	queue->WaitFor(29);
}
