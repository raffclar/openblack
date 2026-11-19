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

#include "Audio/SampleFeed.h"

using openblack::audio::FramesOf;
using openblack::audio::LoopRegion;
using openblack::audio::LoopRegionOf;
using openblack::audio::ReleasedFrameOf;
using openblack::audio::SampleFeed;
using openblack::audio::SampleFeedOf;
using openblack::audio::SamplePart;

namespace
{
/// The frames that a voice plays, in sequence, from the parts in its queue
std::vector<LoopRegion> PlayedFrames(const SampleFeed& feed, LoopRegion region, int32_t frames)
{
	std::vector<LoopRegion> played;
	for (const auto& queued : feed.queue)
	{
		for (int32_t i = 0; i < queued.copies; ++i)
		{
			played.push_back(FramesOf(queued.part, region, frames));
		}
	}
	return played;
}
} // namespace

// A voice that loops until it stops holds the sample as one buffer, so that the mixer loops over the loop region.
// The food scoop of the hand loops this way: its sample loops over its last part and not from its start.
TEST(SampleFeed, AVoiceThatLoopsUntilItStopsHoldsTheSampleAsOneBuffer)
{
	const auto region = LoopRegion {.first = 100, .end = 600};
	EXPECT_EQ(SampleFeedOf(-1, region, 1000), (SampleFeed {.attached = true, .queue = {}}));
	EXPECT_EQ(SampleFeedOf(-5, std::nullopt, 1000), (SampleFeed {.attached = true, .queue = {}}));
}

// The sound library plays a sample with no loop region its loop count plus one times
TEST(SampleFeed, AVoiceQueuesASampleWithNoRegionOnceForEachPlay)
{
	EXPECT_EQ(SampleFeedOf(0, std::nullopt, 1000),
	          (SampleFeed {.attached = false, .queue = {{.part = SamplePart::Whole, .copies = 1}}}));
	EXPECT_EQ(SampleFeedOf(5, std::nullopt, 1000),
	          (SampleFeed {.attached = false, .queue = {{.part = SamplePart::Whole, .copies = 6}}}));
}

// With a loop count of 0, the sound library plays the sample one time from its start to its end
TEST(SampleFeed, ALoopCountOfZeroPlaysAllOfTheSampleOneTime)
{
	EXPECT_EQ(SampleFeedOf(0, LoopRegion {.first = 100, .end = 600}, 1000),
	          (SampleFeed {.attached = false, .queue = {{.part = SamplePart::Whole, .copies = 1}}}));
}

// The sound library plays the frames before the region one time, the region loop count plus one times, and the frames
// after the region one time
TEST(SampleFeed, AFiniteLoopPlaysTheRegionLoopCountPlusOneTimes)
{
	const auto region = LoopRegion {.first = 100, .end = 600};
	const auto feed = SampleFeedOf(3, region, 1000);
	EXPECT_EQ(feed, (SampleFeed {.attached = false,
	                             .queue = {
	                                 {.part = SamplePart::BeforeRegion, .copies = 1},
	                                 {.part = SamplePart::Region, .copies = 4},
	                                 {.part = SamplePart::AfterRegion, .copies = 1},
	                             }}));
	EXPECT_EQ(PlayedFrames(feed, region, 1000), (std::vector<LoopRegion> {
	                                                {.first = 0, .end = 100},
	                                                {.first = 100, .end = 600},
	                                                {.first = 100, .end = 600},
	                                                {.first = 100, .end = 600},
	                                                {.first = 100, .end = 600},
	                                                {.first = 600, .end = 1000},
	                                            }));
}

// A region at the start of the sample has no frames before it. The village bell loops this way.
TEST(SampleFeed, ARegionAtTheStartHasNoPartBeforeIt)
{
	const auto region = LoopRegion {.first = 0, .end = 27400};
	const auto feed = SampleFeedOf(5, region, 30000);
	EXPECT_EQ(feed, (SampleFeed {.attached = false,
	                             .queue = {
	                                 {.part = SamplePart::Region, .copies = 6},
	                                 {.part = SamplePart::AfterRegion, .copies = 1},
	                             }}));
	EXPECT_EQ(PlayedFrames(feed, region, 30000).back(), (LoopRegion {.first = 27400, .end = 30000}));
}

// A region to the end of the sample has no frames after it
TEST(SampleFeed, ARegionToTheEndHasNoPartAfterIt)
{
	EXPECT_EQ(SampleFeedOf(2, LoopRegion {.first = 400, .end = 1000}, 1000),
	          (SampleFeed {.attached = false,
	                       .queue = {
	                           {.part = SamplePart::BeforeRegion, .copies = 1},
	                           {.part = SamplePart::Region, .copies = 3},
	                       }}));
}

// The parts of a sample divide its frames with no gap and no overlap
TEST(SampleFeed, ThePartsOfASampleDivideItsFrames)
{
	const auto region = LoopRegion {.first = 250, .end = 700};
	EXPECT_EQ(FramesOf(SamplePart::Whole, region, 1000), (LoopRegion {.first = 0, .end = 1000}));
	EXPECT_EQ(FramesOf(SamplePart::BeforeRegion, region, 1000), (LoopRegion {.first = 0, .end = 250}));
	EXPECT_EQ(FramesOf(SamplePart::Region, region, 1000), region);
	EXPECT_EQ(FramesOf(SamplePart::AfterRegion, region, 1000), (LoopRegion {.first = 700, .end = 1000}));
}

// The bank header gives the first frame of the region and its end frame. The mixer does not play the end frame in the
// region.
TEST(SampleFeed, TheLoopRegionStopsBeforeTheEndFrameOfTheHeader)
{
	// The food scoop sample: 110079 frames, with the loop region from frame 47743 up to frame 110078
	EXPECT_EQ(LoopRegionOf(47743, 110078, 110079), (LoopRegion {.first = 47743, .end = 110078}));
	EXPECT_EQ(LoopRegionOf(0, 5406, 10000), (LoopRegion {.first = 0, .end = 5406}));
}

// The region stops at the end of the sample
TEST(SampleFeed, TheLoopRegionStopsAtTheEndOfTheSample)
{
	EXPECT_EQ(LoopRegionOf(100, 5000, 1000), (LoopRegion {.first = 100, .end = 1000}));
}

// The header marks no region with -1, or with a first frame that is not before the end frame
TEST(SampleFeed, SomeHeadersGiveNoLoopRegion)
{
	EXPECT_FALSE(LoopRegionOf(-1, -1, 1000).has_value());
	EXPECT_FALSE(LoopRegionOf(-1, 500, 1000).has_value());
	EXPECT_FALSE(LoopRegionOf(500, 500, 1000).has_value());
	EXPECT_FALSE(LoopRegionOf(600, 500, 1000).has_value());
	EXPECT_FALSE(LoopRegionOf(1000, 2000, 1000).has_value());
}

// A release in a pass of the region that is not the last lets the voice play from where it is to the end of the sample
TEST(SampleFeed, AReleaseInTheRegionPlaysToTheEndOfTheSample)
{
	const auto region = LoopRegion {.first = 250, .end = 700};
	const auto feed = SampleFeedOf(5, region, 1000);
	// 250 frames before the region, then 100 frames into the second pass of the region
	EXPECT_EQ(ReleasedFrameOf(feed, region, 1000, 250 + 450 + 100), 350);
	// The first frame of the first pass
	EXPECT_EQ(ReleasedFrameOf(feed, region, 1000, 250), 250);
}

// A release before the region lets the voice play the region one time and then the frames after it
TEST(SampleFeed, AReleaseBeforeTheRegionPlaysToTheEndOfTheSample)
{
	const auto region = LoopRegion {.first = 250, .end = 700};
	EXPECT_EQ(ReleasedFrameOf(SampleFeedOf(2, region, 1000), region, 1000, 100), 100);
	EXPECT_EQ(ReleasedFrameOf(SampleFeedOf(2, region, 1000), region, 1000, 0), 0);
}

// In the last pass of the region, and after the region, the queue already plays to the end of the sample
TEST(SampleFeed, AReleaseInTheLastPassChangesNothing)
{
	const auto region = LoopRegion {.first = 250, .end = 700};
	const auto feed = SampleFeedOf(2, region, 1000);
	// The third pass of the region starts after 250 + 2 * 450 frames
	EXPECT_FALSE(ReleasedFrameOf(feed, region, 1000, 250 + 900).has_value());
	EXPECT_FALSE(ReleasedFrameOf(feed, region, 1000, 250 + 900 + 449).has_value());
	EXPECT_FALSE(ReleasedFrameOf(feed, region, 1000, 250 + 1350 + 10).has_value());
	// The last frame of the second pass still has a pass after it
	EXPECT_EQ(ReleasedFrameOf(feed, region, 1000, 250 + 899), 699);
}

// A voice that played all of its queue, or that holds the sample as one buffer, does not change
TEST(SampleFeed, AReleaseAfterTheEndOrOfAnAttachedVoiceChangesNothing)
{
	const auto region = LoopRegion {.first = 250, .end = 700};
	EXPECT_FALSE(ReleasedFrameOf(SampleFeedOf(2, region, 1000), region, 1000, 1900).has_value());
	EXPECT_FALSE(ReleasedFrameOf(SampleFeedOf(2, region, 1000), region, 1000, 5000).has_value());
	EXPECT_FALSE(ReleasedFrameOf(SampleFeedOf(-1, region, 1000), region, 1000, 300).has_value());
	EXPECT_FALSE(ReleasedFrameOf(SampleFeedOf(0, region, 1000), region, 1000, 300).has_value());
}

// A sample with no region loops over all of its frames, so a release plays the copy that plays to its end
TEST(SampleFeed, AReleaseOfASampleWithNoRegionPlaysToTheEndOfTheCopy)
{
	const auto feed = SampleFeedOf(3, std::nullopt, 1000);
	EXPECT_EQ(ReleasedFrameOf(feed, std::nullopt, 1000, 1200), 200);
	EXPECT_FALSE(ReleasedFrameOf(feed, std::nullopt, 1000, 3200).has_value());
}

// A region that starts at the first frame and stops at the last frame has no part before or after it
TEST(SampleFeed, AReleaseOfARegionWithNoOtherPartsPlaysToTheEnd)
{
	// The village bell: 57855 frames, the region from frame 0 up to frame 27400, five more passes
	const auto region = LoopRegion {.first = 0, .end = 27400};
	const auto feed = SampleFeedOf(5, region, 57855);
	EXPECT_EQ(ReleasedFrameOf(feed, region, 57855, 44100), 16700);
	const auto whole = LoopRegion {.first = 0, .end = 1000};
	EXPECT_EQ(ReleasedFrameOf(SampleFeedOf(1, whole, 1000), whole, 1000, 10), 10);
	EXPECT_FALSE(ReleasedFrameOf(SampleFeedOf(1, whole, 1000), whole, 1000, 1010).has_value());
}
