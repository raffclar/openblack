/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Hand/HandVisibility.h"

using openblack::hand_visibility::IsShown;
using openblack::hand_visibility::TakesPresses;

TEST(HandVisibility, TheHandIsOutInPlay)
{
	static_assert(IsShown(false, true, false));
	EXPECT_TRUE(IsShown(false, true, false));
}

TEST(HandVisibility, ADialogsPointerPutsItAway)
{
	EXPECT_FALSE(IsShown(true, true, false));
	EXPECT_FALSE(IsShown(true, true, true));
}

TEST(HandVisibility, AScriptsCinemaBarsPutItAway)
{
	// Put away, it is still and bends no trees, so nothing near where it was moves for it
	EXPECT_FALSE(IsShown(false, false, false));
	EXPECT_FALSE(IsShown(true, false, false));
}

TEST(HandVisibility, InsideTheTempleTheCinemaBarsLeaveItOut)
{
	// While the advisors introduce the temple, the player still moves the hand about its rooms
	EXPECT_TRUE(IsShown(false, false, true));
	EXPECT_FALSE(IsShown(true, false, true));
}

TEST(HandVisibility, TheHandTakesPressesInPlay)
{
	static_assert(TakesPresses(true));
	EXPECT_TRUE(TakesPresses(true));
}

TEST(HandVisibility, AScriptsCinemaBarsKeepPressesFromTheHand)
{
	// A click during the intro's cinema grips no land, so it throws up no dust
	EXPECT_FALSE(TakesPresses(false));
}
