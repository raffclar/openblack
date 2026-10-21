/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Temple/FutureRoomRules.h"

using namespace openblack::temple;

TEST(FutureRoom, TheWordsWaitUntilTheCameraHasComeIn)
{
	future_room::Words words;
	EXPECT_FALSE(future_room::Step(words, false, false, 100.0f).has_value());
	EXPECT_FALSE(words.shown);
	EXPECT_FLOAT_EQ(words.seconds, 0.0f);
}

TEST(FutureRoom, TheWordsFadeInInWholeStepsToHalfOverTwoSeconds)
{
	future_room::Words words;
	// The frame the camera comes in counts
	EXPECT_FLOAT_EQ(*future_room::Step(words, true, false, 10.0f), 0.0f);
	EXPECT_FLOAT_EQ(*future_room::Step(words, true, false, 10.0f), 1.0f);
	EXPECT_FLOAT_EQ(*future_room::Step(words, true, false, 1000.0f), 65.0f);
	EXPECT_FLOAT_EQ(*future_room::Step(words, true, false, 1000.0f), 128.0f);
	EXPECT_FLOAT_EQ(*future_room::Step(words, true, false, 1000.0f), 128.0f);
}

TEST(FutureRoom, ADialogUpWhenTheCameraComesInHoldsTheWordsBack)
{
	future_room::Words words;
	EXPECT_FALSE(future_room::Step(words, true, true, 500.0f).has_value());
	EXPECT_FALSE(words.shown);
	EXPECT_FLOAT_EQ(*future_room::Step(words, true, false, 500.0f), 32.0f);
	// Once shown, a dialog does not hide them
	EXPECT_FLOAT_EQ(*future_room::Step(words, true, true, 500.0f), 64.0f);
	EXPECT_TRUE(words.shown);
}

TEST(FutureRoom, TheFadeStartsAgainWhenTheCameraLosesTheRoom)
{
	future_room::Words words;
	EXPECT_TRUE(future_room::Step(words, true, false, 1000.0f).has_value());
	EXPECT_FALSE(future_room::Step(words, false, false, 1000.0f).has_value());
	EXPECT_FLOAT_EQ(words.seconds, 1.0f);
	// The frame the camera lost the room still counts
	EXPECT_FLOAT_EQ(*future_room::Step(words, true, false, 500.0f), 96.0f);
}
