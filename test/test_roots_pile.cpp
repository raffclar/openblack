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

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Blast/RootsPile.h"

using namespace openblack;

TEST(RootsPile, AHoleIsTheHalfWidthAndHalfDepthOfTheTreeTimesItsSizeAndTheShare)
{
	// The float of each step, in the order of the game: the depth plus the width, times the size, times the share
	EXPECT_EQ(roots_pile::TreeHoleScale({1.5f, 9.0f, 2.25f}, 1.2f), 0x1.59999ap+0f);
	EXPECT_EQ(roots_pile::TreeHoleScale({0.35f, 4.0f, 0.7f}, 0.993f), 0x1.404d54p-2f);
	// The height of the tree has no effect
	EXPECT_EQ(roots_pile::TreeHoleScale({1.5f, 1.0f, 2.25f}, 1.2f), roots_pile::TreeHoleScale({1.5f, 30.0f, 2.25f}, 1.2f));
}

TEST(RootsPile, APileLastsFifteenSecondsOfGameTimeWhateverTheFrameRate)
{
	// Frames of 7 ms, 16 ms (60 Hz), 33 ms (30 Hz) and 100 ms (one game turn) all run it down in the same game time
	for (const int32_t frame : {7, 16, 33, 100})
	{
		roots_pile::Pile pile;
		int32_t lived = 0;
		int32_t drawn = 0;
		while (roots_pile::Age(pile, frame))
		{
			lived += frame;
			++drawn;
		}
		lived += frame;
		EXPECT_GE(lived, roots_pile::k_Milliseconds) << frame;
		EXPECT_LT(lived, roots_pile::k_Milliseconds + frame) << frame;
		// The frame that uses the last of the time does not draw it
		EXPECT_EQ(drawn, (roots_pile::k_Milliseconds - 1) / frame) << frame;
	}
	// A frame that passes no game time (paused) leaves it as it was
	roots_pile::Pile paused {.millisecondsLeft = 5000};
	EXPECT_TRUE(roots_pile::Age(paused, 0));
	EXPECT_EQ(paused.millisecondsLeft, 5000);
	EXPECT_FALSE(paused.alpha.has_value());
}

TEST(RootsPile, APileFadesOverItsLastSecondByTheTimeThatStaysBeforeTheFrame)
{
	const auto alphaAfter = [](int32_t left) {
		roots_pile::Pile pile {.millisecondsLeft = left};
		static_cast<void>(roots_pile::Age(pile, 16));
		return pile.alpha;
	};
	// Not blended until its last second
	EXPECT_FALSE(alphaAfter(1016).has_value());
	EXPECT_FALSE(alphaAfter(1001).has_value());
	// Then the alpha is the time that stays before the frame, times 0.255 as a float, cut to a whole number
	EXPECT_EQ(alphaAfter(1000), std::optional<uint8_t>(255));
	EXPECT_EQ(alphaAfter(999), std::optional<uint8_t>(254));
	EXPECT_EQ(alphaAfter(500), std::optional<uint8_t>(127));
	EXPECT_EQ(alphaAfter(16), std::optional<uint8_t>(4));
	// It goes when no whole millisecond stays
	roots_pile::Pile last {.millisecondsLeft = 17};
	EXPECT_TRUE(roots_pile::Age(last, 16));
	EXPECT_EQ(last.millisecondsLeft, 1);
	EXPECT_FALSE(roots_pile::Age(last, 1));
}
