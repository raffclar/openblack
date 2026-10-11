/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Windowing/SizeFollower.h"

using openblack::windowing::SizeFollower;

TEST(SizeFollower, NothingIsMadeAgainWhileTheWindowKeepsItsSize)
{
	SizeFollower follower(glm::u16vec2(1280, 1024));
	for (int frame = 0; frame < 3; ++frame)
	{
		EXPECT_FALSE(follower.Follow({1280, 1024}).has_value());
	}
	EXPECT_EQ(follower.Drawn(), glm::u16vec2(1280, 1024));
}

TEST(SizeFollower, AMaximisedWindowIsDrawnAtItsNewSizeOnce)
{
	SizeFollower follower(glm::u16vec2(1024, 768));
	const auto size = follower.Follow({5120, 1377});
	ASSERT_TRUE(size.has_value());
	EXPECT_EQ(*size, glm::u16vec2(5120, 1377));
	EXPECT_FALSE(follower.Follow({5120, 1377}).has_value());
	// Restored, it goes back
	EXPECT_EQ(follower.Follow({1024, 768}), glm::u16vec2(1024, 768));
}

TEST(SizeFollower, EverySizeOfADragIsFollowed)
{
	SizeFollower follower(glm::u16vec2(1920, 1080));
	for (const auto width : {2400, 3000, 3840})
	{
		EXPECT_EQ(follower.Follow({width, 1080}), glm::u16vec2(width, 1080));
	}
	EXPECT_FLOAT_EQ(follower.Aspect(), 3840.0f / 1080.0f);
}

TEST(SizeFollower, AMinimisedWindowKeepsWhatWasDrawn)
{
	SizeFollower follower(glm::u16vec2(1280, 1024));
	EXPECT_FALSE(follower.Follow({0, 0}).has_value());
	EXPECT_FALSE(follower.Follow({1280, 0}).has_value());
	EXPECT_EQ(follower.Drawn(), glm::u16vec2(1280, 1024));
	// Back at the same size: nothing to make again
	EXPECT_FALSE(follower.Follow({1280, 1024}).has_value());
}

TEST(SizeFollower, AFourByThreeWindowStaysExactlyFourByThree)
{
	SizeFollower follower(glm::u16vec2(1920, 1080));
	EXPECT_EQ(follower.Follow({1600, 1200}), glm::u16vec2(1600, 1200));
	EXPECT_EQ(follower.Aspect(), 4.0f / 3.0f);
}

TEST(SizeFollower, BeforeAnythingIsDrawnTheFirstSizeIsTaken)
{
	SizeFollower follower;
	EXPECT_FLOAT_EQ(follower.Aspect(), 1.0f);
	EXPECT_EQ(follower.Follow({800, 600}), glm::u16vec2(800, 600));
}

TEST(SizeFollower, AWindowLargerThanATargetCanBeIsKeptToTheLargest)
{
	SizeFollower follower(glm::u16vec2(800, 600));
	EXPECT_EQ(follower.Follow({70000, 600}), glm::u16vec2(65535, 600));
}
