/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "ECS/PuzzleTotemRules.h"

namespace totem = openblack::ecs::puzzle_totem;

TEST(PuzzleTotemRules, SinkDepthHidesAllButAThirdOfTheModel)
{
	// A model 20 high (half height 10) at scale 2 is 40 high; all but 0.35 of it goes under the land
	EXPECT_FLOAT_EQ(totem::SinkDepth(10.0f, 2.0f), 26.0f);
	EXPECT_FLOAT_EQ(totem::SinkDepth(0.0f, 2.0f), 0.0f);
}

TEST(PuzzleTotemRules, ShareIsHeightOverGreatestHeight)
{
	EXPECT_FLOAT_EQ(totem::ShareOf(0, 100), 0.0f);
	EXPECT_FLOAT_EQ(totem::ShareOf(50, 100), 0.5f);
	EXPECT_FLOAT_EQ(totem::ShareOf(100, 100), 1.0f);
	// A totem that only takes taps always stands fully up
	EXPECT_FLOAT_EQ(totem::ShareOf(0, totem::k_TapOnly), 1.0f);
}

TEST(PuzzleTotemRules, OffsetGoesFromSinkDepthToTheLand)
{
	EXPECT_FLOAT_EQ(totem::OffsetFor(0.0f, 26.0f), -26.0f);
	EXPECT_FLOAT_EQ(totem::OffsetFor(0.5f, 26.0f), -13.0f);
	EXPECT_FLOAT_EQ(totem::OffsetFor(1.0f, 26.0f), 0.0f);
}

TEST(PuzzleTotemRules, HeightForShareTruncatesAndClamps)
{
	EXPECT_EQ(totem::HeightForShare(0.999f, 100), 99);
	EXPECT_EQ(totem::HeightForShare(0.5f, 100), 50);
	EXPECT_EQ(totem::HeightForShare(1.5f, 100), 100);
	EXPECT_EQ(totem::HeightForShare(0.5f, totem::k_TapOnly), totem::k_TapOnly);
}

TEST(PuzzleTotemRules, ShareForOffsetStaysBetweenNoneAndAll)
{
	EXPECT_FLOAT_EQ(totem::ShareForOffset(-13.0f, 26.0f), 0.5f);
	EXPECT_FLOAT_EQ(totem::ShareForOffset(0.0f, 26.0f), 1.0f);
	EXPECT_FLOAT_EQ(totem::ShareForOffset(-30.0f, 26.0f), 0.0f);
	// A totem with no model has no sink depth: the share is none, not a number that is not a number
	EXPECT_FLOAT_EQ(totem::ShareForOffset(0.0f, 0.0f), 0.0f);
}

TEST(PuzzleTotemRules, HoldAfterSlideMovesByAPartOfTheSlide)
{
	EXPECT_FLOAT_EQ(totem::HoldAfterSlide(-10.0f, 3.0f, 26.0f), -8.0f);
	EXPECT_FLOAT_EQ(totem::HoldAfterSlide(-10.0f, -30.0f, 26.0f), -26.0f);
	EXPECT_FLOAT_EQ(totem::HoldAfterSlide(-1.0f, 6.0f, 26.0f), 0.0f);
}

TEST(PuzzleTotemRules, HeightForHoldAndHoldOffsetAgree)
{
	EXPECT_EQ(totem::HeightForHold(-13.0f, 26.0f, 100), 50);
	EXPECT_EQ(totem::HeightForHold(-6.5f, 26.0f, 100), 75);
	EXPECT_EQ(totem::HeightForHold(0.0f, 26.0f, 100), 100);
	EXPECT_EQ(totem::HeightForHold(-26.0f, 26.0f, 100), 0);
	EXPECT_FLOAT_EQ(totem::HoldOffset(75, 100, 26.0f), -6.5f);
	EXPECT_FLOAT_EQ(totem::HoldOffset(100, 100, 26.0f), 0.0f);
	EXPECT_FLOAT_EQ(totem::HoldOffset(0, 100, 26.0f), -26.0f);
}
