/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <gtest/gtest.h>

#include "Audio/SampleFeed.h"

using openblack::audio::LoopRegion;
using openblack::audio::LoopRegionOf;
using openblack::audio::SampleFeed;
using openblack::audio::SampleFeedOf;

// A voice that loops until it stops holds the sample as one buffer, so that the mixer loops over the loop region.
// The food scoop of the hand loops this way: its sample loops over its last part and not from its start.
TEST(SampleFeed, AVoiceThatLoopsUntilItStopsHoldsTheSampleAsOneBuffer)
{
	EXPECT_EQ(SampleFeedOf(-1), (SampleFeed {.attached = true, .queued = 0}));
	EXPECT_EQ(SampleFeedOf(-5), (SampleFeed {.attached = true, .queued = 0}));
}

// The sound library plays a sample its loop count plus one times
TEST(SampleFeed, OtherVoicesQueueTheSampleOnceForEachPlay)
{
	EXPECT_EQ(SampleFeedOf(0), (SampleFeed {.attached = false, .queued = 1}));
	EXPECT_EQ(SampleFeedOf(5), (SampleFeed {.attached = false, .queued = 6}));
}

// The bank header gives the first and last frames of the region. The region includes the last frame.
TEST(SampleFeed, TheLoopRegionIncludesTheLastFrameOfTheHeader)
{
	// The food scoop sample: 110079 frames, with the loop region from frame 47743 to its last frame
	EXPECT_EQ(LoopRegionOf(47743, 110078, 110079), (LoopRegion {.first = 47743, .end = 110079}));
	EXPECT_EQ(LoopRegionOf(0, 5406, 10000), (LoopRegion {.first = 0, .end = 5407}));
}

// The region stops at the end of the sample
TEST(SampleFeed, TheLoopRegionStopsAtTheEndOfTheSample)
{
	EXPECT_EQ(LoopRegionOf(100, 5000, 1000), (LoopRegion {.first = 100, .end = 1000}));
}

// The header marks no region with -1, or with a first frame that is not before the last frame
TEST(SampleFeed, SomeHeadersGiveNoLoopRegion)
{
	EXPECT_FALSE(LoopRegionOf(-1, -1, 1000).has_value());
	EXPECT_FALSE(LoopRegionOf(-1, 500, 1000).has_value());
	EXPECT_FALSE(LoopRegionOf(500, 500, 1000).has_value());
	EXPECT_FALSE(LoopRegionOf(600, 500, 1000).has_value());
	EXPECT_FALSE(LoopRegionOf(1000, 2000, 1000).has_value());
}
