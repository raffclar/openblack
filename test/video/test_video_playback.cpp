/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// debug::gui::VideoPlayback (src/Debug/VideoPlayback.h): the pacing of the Video debug window, with fake times.

#include <gtest/gtest.h>

#include "Debug/VideoPlayback.h"

using openblack::debug::gui::VideoPlayback;

TEST(VideoPlayback, StartsPausedOnTheFirstFrame)
{
	VideoPlayback playback;
	playback.Reset(10, 24, 1);
	EXPECT_FALSE(playback.Playing());
	EXPECT_EQ(playback.Due(), 1u);
	playback.Advance(1000);
	EXPECT_EQ(playback.Due(), 1u); // paused: time does not count
}

TEST(VideoPlayback, PlaysAtTheFilmsRate)
{
	VideoPlayback playback;
	playback.Reset(100, 24, 1);
	playback.Play();
	EXPECT_EQ(playback.Due(), 1u);
	playback.Advance(41); // frame 1 is due at 1000 / 24 = 41.67 ms
	EXPECT_EQ(playback.Due(), 1u);
	playback.Advance(1);
	EXPECT_EQ(playback.Due(), 2u);
	playback.Advance(958); // 1 s: frames 0 to 24
	EXPECT_EQ(playback.Due(), 25u);
	EXPECT_EQ(playback.PlayedMs(), 1000u);
}

TEST(VideoPlayback, AFractionalRate)
{
	VideoPlayback playback;
	playback.Reset(100, 30000, 1001); // 29.97 fps: frame 3 at 100.1 ms
	playback.Play();
	playback.Advance(100);
	EXPECT_EQ(playback.Due(), 3u);
	playback.Advance(1);
	EXPECT_EQ(playback.Due(), 4u);
}

TEST(VideoPlayback, PauseAndPlayGoOnFromTheFrameOnScreen)
{
	VideoPlayback playback;
	playback.Reset(100, 10, 1);
	playback.Play();
	playback.Advance(250); // frames 0, 1, 2
	EXPECT_EQ(playback.Due(), 3u);
	playback.Pause();
	playback.Advance(5000);
	EXPECT_EQ(playback.Due(), 3u);
	playback.Play(); // frame 2 on screen: frame 3 a frame's time later
	EXPECT_EQ(playback.Due(), 3u);
	EXPECT_EQ(playback.PlayedMs(), 0u);
	playback.Advance(99);
	EXPECT_EQ(playback.Due(), 3u);
	playback.Advance(1);
	EXPECT_EQ(playback.Due(), 4u);
}

TEST(VideoPlayback, StepAndRestart)
{
	VideoPlayback playback;
	playback.Reset(3, 15, 1);
	playback.Step();
	EXPECT_EQ(playback.Due(), 2u);
	playback.Play();
	playback.Step(); // a step pauses
	EXPECT_FALSE(playback.Playing());
	EXPECT_EQ(playback.Due(), 3u);
	playback.Step(); // not past the last frame
	EXPECT_EQ(playback.Due(), 3u);
	playback.Restart();
	EXPECT_EQ(playback.Due(), 1u);
	EXPECT_FALSE(playback.Playing());
}

TEST(VideoPlayback, StopsOnTheLastFrame)
{
	VideoPlayback playback;
	playback.Reset(5, 25, 1);
	playback.Play();
	playback.Advance(10000);
	EXPECT_EQ(playback.Due(), 5u);
	EXPECT_FALSE(playback.Playing());
	playback.Play(); // at the end: nothing to play
	EXPECT_FALSE(playback.Playing());
}

TEST(VideoPlayback, NoFilm)
{
	VideoPlayback playback;
	playback.Reset(0, 24, 1);
	EXPECT_EQ(playback.Due(), 0u);
	playback.Play();
	EXPECT_FALSE(playback.Playing());
	playback.Step();
	EXPECT_EQ(playback.Due(), 0u);
}
