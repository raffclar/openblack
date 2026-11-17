/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <StartupBoxFrames.h>
#include <gtest/gtest.h>

using openblack::StartupBoxFrames;

TEST(StartupBoxFrames, NoLimitNeverStopsTheRun)
{
	StartupBoxFrames frames(0);
	for (int i = 0; i < 100000; ++i)
	{
		ASSERT_FALSE(frames.Step());
	}
}

TEST(StartupBoxFrames, TheRunStopsAtTheFrameAfterTheLimit)
{
	StartupBoxFrames frames(3);
	EXPECT_FALSE(frames.Step());
	EXPECT_FALSE(frames.Step());
	EXPECT_FALSE(frames.Step());
	EXPECT_TRUE(frames.Step());
	EXPECT_EQ(frames.FrameLimit(), 3U);
}

TEST(StartupBoxFrames, ALimitOfOneShowsOneFrame)
{
	StartupBoxFrames frames(1);
	EXPECT_FALSE(frames.Step());
	EXPECT_TRUE(frames.Step());
}
