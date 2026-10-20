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

#include "Hand/HandKnock.h"

using namespace openblack::hand_knock;
using namespace std::chrono_literals;

namespace
{
constexpr uint32_t k_ClipMs = 100;
}

TEST(HandKnock, WithNoKnockTheHandDoesNotTap)
{
	Tap tap;
	EXPECT_FALSE(tap.Hold(false));
	EXPECT_FALSE(tap.Point().has_value());
}

TEST(HandKnock, TheTapPlaysOneTimeFromItsStartAtThePointOfTheKnock)
{
	Tap tap;
	tap.Knock({1.0f, 2.0f, 3.0f});
	ASSERT_TRUE(tap.Hold(false));
	EXPECT_EQ(*tap.Point(), glm::vec3(1.0f, 2.0f, 3.0f));
	// Each frame shows the time before the frame's time is added
	EXPECT_EQ(tap.Play(40ms, k_ClipMs), 0u);
	EXPECT_EQ(tap.Play(40ms, k_ClipMs), 40u);
	EXPECT_TRUE(tap.Hold(false));
	EXPECT_EQ(tap.Play(40ms, k_ClipMs), 80u);
	// The time reached the length of the clip: the tap stops
	EXPECT_FALSE(tap.Hold(false));
	EXPECT_FALSE(tap.Point().has_value());
}

TEST(HandKnock, AKnockWhileTheTapPlaysMovesThePointButDoesNotStartTheTapAgain)
{
	Tap tap;
	tap.Knock({1.0f, 0.0f, 0.0f});
	tap.Play(60ms, k_ClipMs);
	tap.Knock({5.0f, 0.0f, 0.0f});
	EXPECT_EQ(*tap.Point(), glm::vec3(5.0f, 0.0f, 0.0f));
	EXPECT_EQ(tap.Play(10ms, k_ClipMs), 60u);
}

TEST(HandKnock, HoldingSomethingPutsTheTapOffAndItStartsAgainOnTheLetGo)
{
	Tap tap;
	tap.Knock({0.0f, 0.0f, 0.0f});
	tap.Play(60ms, k_ClipMs);
	EXPECT_FALSE(tap.Hold(true));
	EXPECT_EQ(tap.Time(), 0us);
	EXPECT_TRUE(tap.Hold(false));
	EXPECT_EQ(tap.Play(10ms, k_ClipMs), 0u);
}

TEST(HandKnock, WithNoTapClipTheTapStopsAtOnce)
{
	Tap tap;
	tap.Knock({0.0f, 0.0f, 0.0f});
	EXPECT_EQ(tap.Play(16ms, 0), 0u);
	EXPECT_FALSE(tap.Hold(false));
}

TEST(HandKnock, TheTapStopsOnTheFrameThatReachesTheClipsLength)
{
	Tap tap;
	tap.Knock({0.0f, 0.0f, 0.0f});
	tap.Play(99ms, k_ClipMs);
	EXPECT_TRUE(tap.Hold(false));
	tap.Play(1ms, k_ClipMs);
	EXPECT_FALSE(tap.Hold(false));
}
