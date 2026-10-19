/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The box that the game shows while it saves or loads

#include <gtest/gtest.h>

#include "Gui/SavingBox.h"

using namespace openblack::gui;

TEST(SavingBox, OnlyTheAutomaticSavesLeaveTheScreenAsItIs)
{
	EXPECT_TRUE(saving_box::DarkensScreen(0));
	EXPECT_TRUE(saving_box::DarkensScreen(14));
	EXPECT_FALSE(saving_box::DarkensScreen(15));
	EXPECT_FALSE(saving_box::DarkensScreen(16));
	EXPECT_FALSE(saving_box::DarkensScreen(17));
	EXPECT_TRUE(saving_box::DarkensScreen(19));
	EXPECT_TRUE(saving_box::DarkensScreen(20));
}

TEST(SavingBox, TheBandCrossesTheMiddleWithItsEndsShadedOverTwoFifths)
{
	// The screen of 800 by 600 that the game measures the box on
	const auto layout = saving_box::LayoutFor(0, 800);
	EXPECT_EQ(layout.top, 270);
	EXPECT_EQ(layout.bottom, 330);
	EXPECT_EQ(layout.lineFrom, -1);
	EXPECT_EQ(layout.lineTo, 800);
	// The darker screen covers it all
	EXPECT_EQ(layout.quads[0].corners[0], glm::vec2(-1.0f, -1.0f));
	EXPECT_EQ(layout.quads[0].corners[2], glm::vec2(801.0f, 601.0f));
	EXPECT_EQ(layout.quads[0].colours[0], saving_box::k_DarkColour);
	// The soft shadows are darkest next to the band
	EXPECT_EQ(layout.quads[1].corners[0].y, 255.0f);
	EXPECT_EQ(layout.quads[1].colours[0], 0u);
	EXPECT_EQ(layout.quads[1].colours[3], saving_box::k_ShadowColour);
	EXPECT_EQ(layout.quads[2].corners[2].y, 345.0f);
	EXPECT_EQ(layout.quads[2].colours[0], saving_box::k_ShadowColour);
	EXPECT_EQ(layout.quads[2].colours[2], 0u);
	// The left end to 320, the middle to 480, the right end to the edge
	EXPECT_EQ(layout.quads[3].corners[1].x, 320.0f);
	EXPECT_EQ(layout.quads[3].colours[0], saving_box::k_BandDark);
	EXPECT_EQ(layout.quads[3].colours[1], saving_box::k_BandLight);
	EXPECT_EQ(layout.quads[4].corners[0].x, 320.0f);
	EXPECT_EQ(layout.quads[4].corners[1].x, 480.0f);
	EXPECT_EQ(layout.quads[5].corners[0].x, 480.0f);
	EXPECT_EQ(layout.quads[5].colours[1], saving_box::k_BandDark);
}

TEST(SavingBox, TheTextIsCentredUpAndDownWhenItFits)
{
	// One line of the big text in the rectangle 200 high: 82.5 down, cut to 82
	EXPECT_EQ(saving_box::TextTop(saving_box::k_TextRect, 35.0f), 282);
	// Text as high as the rectangle starts at its top
	EXPECT_EQ(saving_box::TextTop(saving_box::k_TextRect, 200.0f), 200);
	EXPECT_EQ(saving_box::TextTop(saving_box::k_TextRect, 250.0f), 200);
}
