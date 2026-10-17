/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <vector>

#include <gtest/gtest.h>

#include "Audio/ClockedAudioPlayer.h"

using namespace openblack::audio;

namespace
{

/// A clock that the test moves on by hand
struct FakeClock
{
	uint32_t now {1000};
};

/// One second of mono sound at a rate of 1000 frames a second
std::vector<int16_t> OneSecond()
{
	return std::vector<int16_t>(1000, 0);
}

} // namespace

TEST(ClockedAudioPlayer, ASoundLastsItsLengthByTheClock)
{
	FakeClock clock;
	ClockedAudioPlayer player([&clock] { return clock.now; });
	const auto buffer = player.CreateBuffer(ChannelLayout::Mono, OneSecond(), 1000);
	EXPECT_FLOAT_EQ(player.GetDuration(buffer), 1.0f);
	const auto source = player.CreateSource(1.0f, false);
	EXPECT_EQ(player.GetStatus(source), AudioStatus::Initial);
	player.QueueBuffer(source, buffer);
	player.PlaySource(source, 1.0f, false);
	clock.now += 999;
	EXPECT_EQ(player.GetStatus(source), AudioStatus::Playing);
	clock.now += 1;
	EXPECT_EQ(player.GetStatus(source), AudioStatus::Stopped);
}

TEST(ClockedAudioPlayer, APausedSoundWaits)
{
	FakeClock clock;
	ClockedAudioPlayer player([&clock] { return clock.now; });
	const auto source = player.CreateSource(1.0f, false);
	player.QueueBuffer(source, player.CreateBuffer(ChannelLayout::Mono, OneSecond(), 1000));
	player.StartSource(source);
	clock.now += 600;
	player.PauseSource(source);
	clock.now += 5000;
	EXPECT_EQ(player.GetStatus(source), AudioStatus::Paused);
	player.StartSource(source);
	clock.now += 300;
	EXPECT_EQ(player.GetStatus(source), AudioStatus::Playing);
	clock.now += 100;
	EXPECT_EQ(player.GetStatus(source), AudioStatus::Stopped);
}

TEST(ClockedAudioPlayer, PitchChangesTheLength)
{
	FakeClock clock;
	ClockedAudioPlayer player([&clock] { return clock.now; });
	const auto source = player.CreateSource(2.0f, false);
	player.QueueBuffer(source, player.CreateBuffer(ChannelLayout::Mono, OneSecond(), 1000));
	player.StartSource(source);
	clock.now += 500;
	EXPECT_EQ(player.GetStatus(source), AudioStatus::Stopped);
}

TEST(ClockedAudioPlayer, ALoopingSoundDoesNotStop)
{
	FakeClock clock;
	ClockedAudioPlayer player([&clock] { return clock.now; });
	const auto source = player.CreateSource(1.0f, false);
	player.QueueBuffer(source, player.CreateBuffer(ChannelLayout::Stereo, std::vector<int16_t>(2000, 0), 1000));
	player.PlaySource(source, 1.0f, true);
	clock.now += 10500;
	EXPECT_EQ(player.GetStatus(source), AudioStatus::Playing);
	EXPECT_EQ(player.GetSampleOffset(source), 500u);
	player.StopSource(source);
	EXPECT_EQ(player.GetStatus(source), AudioStatus::Stopped);
}

TEST(ClockedAudioPlayer, AStreamGivesBackTheBuffersThatItPlayed)
{
	FakeClock clock;
	ClockedAudioPlayer player([&clock] { return clock.now; });
	const auto source = player.CreateSource(1.0f, false);
	const auto first = player.CreateBuffer(ChannelLayout::Mono, OneSecond(), 1000);
	const auto second = player.CreateBuffer(ChannelLayout::Mono, OneSecond(), 1000);
	player.QueueBuffer(source, first);
	player.QueueBuffer(source, second);
	player.StartSource(source);
	clock.now += 1200;
	const auto played = player.UnqueueProcessedBuffers(source);
	ASSERT_EQ(played.size(), 1u);
	EXPECT_EQ(played[0], first);
	EXPECT_EQ(player.GetSampleOffset(source), 200u);
	EXPECT_FLOAT_EQ(player.GetProgress(2000, source), 0.2f);
	EXPECT_EQ(player.GetStatus(source), AudioStatus::Playing);
}

TEST(ClockedAudioPlayer, AClockThatGoesBackCountsAsNoTime)
{
	FakeClock clock;
	ClockedAudioPlayer player([&clock] { return clock.now; });
	const auto source = player.CreateSource(1.0f, false);
	player.QueueBuffer(source, player.CreateBuffer(ChannelLayout::Mono, OneSecond(), 1000));
	player.StartSource(source);
	clock.now += 400;
	EXPECT_EQ(player.GetStatus(source), AudioStatus::Playing);
	clock.now = 0;
	EXPECT_EQ(player.GetStatus(source), AudioStatus::Playing);
	clock.now = 599;
	EXPECT_EQ(player.GetStatus(source), AudioStatus::Playing);
	clock.now = 600;
	EXPECT_EQ(player.GetStatus(source), AudioStatus::Stopped);
}

TEST(ClockedAudioPlayer, AGoneSourceHasStopped)
{
	ClockedAudioPlayer player([] { return 0u; });
	EXPECT_EQ(player.GetStatus(77), AudioStatus::Stopped);
	const auto source = player.CreateSource(1.0f, false);
	player.DeleteSource(source);
	EXPECT_EQ(player.GetStatus(source), AudioStatus::Stopped);
}
