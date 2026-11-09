/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Gui/BoxButtonsHeld.h"

using namespace openblack::gui::box_buttons;

namespace
{
constexpr Buttons k_None {};
constexpr Buttons k_Left {.left = true};
constexpr Buttons k_Right {.right = true};
constexpr Buttons k_Middle {.middle = true};
} // namespace

TEST(BoxButtonsHeld, TheDrawReadsTheButtonsOfTheFrameBefore)
{
	HeldFlag flag;
	flag.Frame(k_None, true);
	EXPECT_FALSE(flag.Drawn(false));
	flag.Frame(k_Right, false);
	EXPECT_FALSE(flag.Drawn(false));
	flag.Frame(k_Right, false);
	EXPECT_TRUE(flag.Drawn(false));
	flag.Frame(k_None, false);
	EXPECT_TRUE(flag.Drawn(false));
	flag.Frame(k_None, false);
	EXPECT_FALSE(flag.Drawn(false));
}

TEST(BoxButtonsHeld, TheFirstFrameOfTheBoxReadsTheLeftButtonOnly)
{
	HeldFlag flag;
	flag.Frame(k_Middle, true);
	EXPECT_FALSE(flag.Drawn(false));
	EXPECT_FALSE(flag.Drawn(true));
	// The pass after the first draw reads all three buttons
	flag.Frame(k_Middle, false);
	EXPECT_TRUE(flag.Drawn(false));

	HeldFlag left;
	left.Frame(k_Left, true);
	EXPECT_TRUE(left.Drawn(false));
}

TEST(BoxButtonsHeld, AGameTurnBeforeTheDrawGivesTheButtonsOfThisFrame)
{
	HeldFlag flag;
	flag.Frame(k_None, true);
	flag.Frame(k_Right, false);
	EXPECT_FALSE(flag.Drawn(false));
	EXPECT_TRUE(flag.Drawn(true));
	flag.Frame(k_None, false);
	EXPECT_TRUE(flag.Drawn(false));
	EXPECT_FALSE(flag.Drawn(true));
}

TEST(BoxButtonsHeld, TheFlagTakesTheButtonsOfTheBoxAgainWhenTheBoxOpensAgain)
{
	HeldFlag flag;
	flag.Frame(k_Right, true);
	flag.Frame(k_Right, false);
	EXPECT_TRUE(flag.Drawn(false));
	// The box closes and opens again with the right button still held
	flag.Frame(k_Right, true);
	EXPECT_FALSE(flag.Drawn(false));
}
