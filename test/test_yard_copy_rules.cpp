/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "ECS/YardCopyRules.h"

using namespace openblack::ecs;

TEST(YardCopyRules, TheClockGoesRoundEveryTwoSeconds)
{
	EXPECT_EQ(yard_copy::StepClock(0, 16), 16);
	EXPECT_EQ(yard_copy::StepClock(1990, 10), 2000);
	EXPECT_EQ(yard_copy::StepClock(1990, 20), 10);
	EXPECT_EQ(yard_copy::StepClock(1990, 4020), 10);
	EXPECT_EQ(yard_copy::StepClock(500, 0), 500);
}

TEST(YardCopyRules, TheClockMovesOnceForEachDrawnWorkshop)
{
	// Two workshops in one frame of 16 milliseconds move the one clock by 32 milliseconds
	const int32_t clock = yard_copy::StepClock(yard_copy::StepClock(1980, 16), 16);
	EXPECT_EQ(clock, 12);
}

TEST(YardCopyRules, TheAlphaPulsesFromAThirdOfTheLandAlphaToNothing)
{
	EXPECT_EQ(yard_copy::Alpha(0), 85);
	EXPECT_EQ(yard_copy::Alpha(1), 84);
	EXPECT_EQ(yard_copy::Alpha(250), 72);
	EXPECT_EQ(yard_copy::Alpha(500), 42);
	EXPECT_EQ(yard_copy::Alpha(999), 0);
	EXPECT_EQ(yard_copy::Alpha(1000), 0);
	EXPECT_EQ(yard_copy::Alpha(1500), 42);
	EXPECT_EQ(yard_copy::Alpha(1999), 84);
	EXPECT_EQ(yard_copy::Alpha(2000), 85);
	EXPECT_EQ(yard_copy::Alpha(0, 128), 42);
	EXPECT_EQ(yard_copy::Alpha(500, 128), 21);
}

TEST(YardCopyRules, TheAlphaFallsInTheFirstHalfOfThePeriod)
{
	for (int32_t clock = 1; clock <= 1000; ++clock)
	{
		EXPECT_LE(yard_copy::Alpha(clock), yard_copy::Alpha(clock - 1)) << clock;
	}
}

TEST(YardCopyRules, TheCopyChangesTheReferenceFromAnAlphaOfTwelve)
{
	EXPECT_FALSE(yard_copy::ChangesReference(0));
	EXPECT_FALSE(yard_copy::ChangesReference(11));
	EXPECT_TRUE(yard_copy::ChangesReference(12));
	EXPECT_TRUE(yard_copy::ChangesReference(85));
	EXPECT_EQ(yard_copy::SeeThroughRef(11), std::nullopt);
	EXPECT_EQ(yard_copy::SeeThroughRef(12), std::optional<uint8_t>(0));
}
