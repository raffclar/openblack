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
using openblack::hand_visibility::ShowsMiracleEffects;
using openblack::hand_visibility::TakesPresses;

namespace
{
using openblack::hand_visibility::ShowFacts;

constexpr ShowFacts k_InPlay {.dialogOpen = false, .interfaceActive = true};
} // namespace

TEST(HandVisibility, TheHandIsOutInPlay)
{
	static_assert(IsShown(k_InPlay));
	EXPECT_TRUE(IsShown(k_InPlay));
}

TEST(HandVisibility, ADialogsPointerPutsItAway)
{
	EXPECT_FALSE(IsShown({.dialogOpen = true, .interfaceActive = true}));
}

TEST(HandVisibility, AScriptsCinemaBarsPutItAway)
{
	// Put away, it is still and bends no trees, so nothing near where it was moves for it
	// The same rule applies inside the temple, while the advisors introduce a room
	EXPECT_FALSE(IsShown({.dialogOpen = false, .interfaceActive = false}));
	EXPECT_FALSE(IsShown({.dialogOpen = true, .interfaceActive = false}));
}

TEST(HandVisibility, TheOnlineFrontEndPutsItAwayInTheTemple)
{
	// The boxes of the online front end show their own pointer over the future room, with or without cinema bars
	EXPECT_FALSE(IsShown({.dialogOpen = false, .interfaceActive = true, .onlineFrontEnd = true}));
	EXPECT_FALSE(IsShown({.dialogOpen = false, .interfaceActive = false, .onlineFrontEnd = true}));
}

TEST(HandVisibility, TheHandComesBackWhenTheOnlineFrontEndCloses)
{
	// The game starts on its land, with the hand out
	EXPECT_TRUE(IsShown({.dialogOpen = false, .interfaceActive = true, .onlineFrontEnd = false}));
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

TEST(HandVisibility, TheHandThatIsOutShowsItsMiracle)
{
	static_assert(ShowsMiracleEffects(true, false));
	EXPECT_TRUE(ShowsMiracleEffects(true, false));
}

TEST(HandVisibility, TheHandThatIsAwayStopsItsMiracleEffects)
{
	// The bands in flight stop where they are, and continue when the hand comes back
	EXPECT_FALSE(ShowsMiracleEffects(false, false));
	EXPECT_FALSE(ShowsMiracleEffects(false, true));
}

TEST(HandVisibility, InsideTheTempleTheHandShowsNoMiracleEffects)
{
	// The temple draws the hand, but not its glow, its bands or its bracelets
	EXPECT_FALSE(ShowsMiracleEffects(true, true));
}
