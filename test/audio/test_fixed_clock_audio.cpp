/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Audio/Device/FixedClockSampleOutput.h's timeline: a sample sounds for its length / pitch times its
// passes, on the ticks it is given, so runs with OPENBLACK_FIXED_FRAME_MS repeat

#include <gtest/gtest.h>

#include "Audio/Device/FixedClockSampleOutput.h"

using Timeline = openblack::audio::FixedClockSampleOutput::Timeline;

TEST(FixedClockAudio, OncePlaysItsLength)
{
	Timeline t;
	t.Start(1000, 500.0, 1.0f, 0);
	EXPECT_TRUE(t.Sounding(1000));
	EXPECT_TRUE(t.Sounding(1499));
	EXPECT_FALSE(t.Sounding(1500));
	EXPECT_EQ(t.PositionMs(1250), 250);
	EXPECT_EQ(t.PositionMs(1500), -1);
}

TEST(FixedClockAudio, PitchScalesTheTime)
{
	Timeline t;
	t.Start(0, 500.0, 2.0f, 0); // twice as fast: 250 ticks
	EXPECT_TRUE(t.Sounding(249));
	EXPECT_FALSE(t.Sounding(250));
}

TEST(FixedClockAudio, APitchChangeKeepsWhatWasPlayed)
{
	Timeline t;
	t.Start(0, 1000.0, 1.0f, 0);
	t.ChangePitch(400, 0.5f); // 400 ms played, then half speed: 600 more ms take 1200 ticks
	EXPECT_TRUE(t.Sounding(1599));
	EXPECT_FALSE(t.Sounding(1600));
}

TEST(FixedClockAudio, FiniteLoopsAndForEver)
{
	Timeline twice;
	twice.Start(0, 100.0, 1.0f, 2); // 2 more passes: 3 in all
	EXPECT_TRUE(twice.Sounding(299));
	EXPECT_FALSE(twice.Sounding(300));
	EXPECT_EQ(twice.PositionMs(150), 50);

	Timeline ever;
	ever.Start(0, 100.0, 1.0f, -1);
	EXPECT_TRUE(ever.Sounding(1000000));
}

TEST(FixedClockAudio, ReleaseLoopEndsTheCurrentPass)
{
	Timeline t;
	t.Start(0, 100.0, 1.0f, -1);
	t.ReleaseLoop(250); // in the third pass (200..300): it ends at 300
	EXPECT_TRUE(t.Sounding(299));
	EXPECT_FALSE(t.Sounding(300));
}

TEST(FixedClockAudio, ANotPlayedSampleIsSilent)
{
	Timeline t;
	t.Start(0, 0.0, 1.0f, 0); // the real Play failed: length 0
	EXPECT_FALSE(t.Sounding(0));
	EXPECT_EQ(t.PositionMs(0), -1);
}

TEST(FixedClockAudio, TheTickCounterWraps)
{
	Timeline t;
	t.Start(0xFFFFFF00u, 500.0, 1.0f, 0);  // GetTickCount wraps at 2^32: now - start in uint32_t
	EXPECT_TRUE(t.Sounding(0x000000E0u));  // 0x100 + 0xE0 = 480 ticks
	EXPECT_FALSE(t.Sounding(0x00000110u)); // 0x100 + 0x110 = 528 ticks
	EXPECT_TRUE(t.Sounding(0xFFFFFFFFu));  // 255 ticks
}

TEST(FixedClockAudio, ReleaseLoopNeverRevivesAnEndedSample)
{
	Timeline once;
	once.Start(0, 100.0, 1.0f, 0);
	once.ReleaseLoop(500); // ended at 100: nothing to release
	EXPECT_FALSE(once.Sounding(500));

	Timeline looped;
	looped.Start(0, 100.0, 1.0f, 1); // two passes: ends at 200
	looped.ReleaseLoop(50);          // in the first pass: loops can only go down, to 0
	EXPECT_TRUE(looped.Sounding(99));
	EXPECT_FALSE(looped.Sounding(100));
}
