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

TEST(HandVisibility, TheHandIsOutInPlay)
{
	static_assert(IsShown(false, true));
	EXPECT_TRUE(IsShown(false, true));
}

TEST(HandVisibility, ADialogsPointerPutsItAway)
{
	EXPECT_FALSE(IsShown(true, true));
}

TEST(HandVisibility, AScriptsCinemaBarsPutItAway)
{
	// Put away, it is still and bends no trees, so nothing near where it was moves for it
	EXPECT_FALSE(IsShown(false, false));
	EXPECT_FALSE(IsShown(true, false));
}
