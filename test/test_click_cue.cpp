/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Help/ClickCue.h"

using namespace openblack::help::click_cue;

TEST(ClickCue, TheButtonBlinksTwoTenthsOfEveryHalfSecond)
{
	EXPECT_TRUE(ButtonLit(0));
	EXPECT_TRUE(ButtonLit(199));
	EXPECT_FALSE(ButtonLit(200));
	EXPECT_FALSE(ButtonLit(499));
	EXPECT_TRUE(ButtonLit(500));
}

TEST(ClickCue, SitsAtTheRightOfTheBoxsBottomThird)
{
	// A box 90 rows tall: the cue a third of it, its word two thirds of that
	const auto cue = Layout(1280, 600, 689, 1.0f, 50.0f, false, 0);
	ASSERT_TRUE(cue.has_value());
	EXPECT_EQ(cue->mouse.min, glm::vec2(1246.0f, 660.0f));
	EXPECT_EQ(cue->mouse.max, glm::vec2(1276.0f, 690.0f));
	EXPECT_FLOAT_EQ(cue->labelSize, 20.0f);
	EXPECT_EQ(cue->labelAt, glm::vec2(1194.0f, 665.0f));
	EXPECT_EQ(cue->labelGlow.min, glm::vec2(1194.0f, 665.0f));
	EXPECT_EQ(cue->labelGlow.max, glm::vec2(1244.0f, 685.0f));
	EXPECT_EQ(LabelSize(600, 689, true), 24);
}

TEST(ClickCue, TheLeftButtonIsTheRightOneMirrored)
{
	const auto lit = Layout(1280, 600, 689, 1.0f, 50.0f, false, 0);
	ASSERT_TRUE(lit.has_value());
	EXPECT_EQ(lit->uvMin, glm::vec2(0.5f, 0.5f));
	EXPECT_EQ(lit->uvMax, glm::vec2(0.25f, 0.75f));
	const auto dark = Layout(1280, 600, 689, 1.0f, 50.0f, false, 300);
	ASSERT_TRUE(dark.has_value());
	EXPECT_EQ(dark->uvMin, glm::vec2(0.25f, 0.5f));
	EXPECT_EQ(dark->uvMax, glm::vec2(0.0f, 0.75f));
}

TEST(ClickCue, FadesInWithItsGlows)
{
	// Too faint to draw at first
	EXPECT_FALSE(Layout(1280, 600, 689, 0.01f, 50.0f, false, 0).has_value());
	const auto half = Layout(1280, 600, 689, 0.5f, 50.0f, false, 0);
	ASSERT_TRUE(half.has_value());
	EXPECT_FLOAT_EQ(half->labelColour.a, 127.0f / 255.0f);
	EXPECT_FLOAT_EQ(half->mouseColour.a, 127.0f / 255.0f);
	EXPECT_FLOAT_EQ(half->mouseGlowColour.a, 64.0f / 255.0f);
	EXPECT_FLOAT_EQ(half->labelGlowColour.a, 21.0f / 255.0f);
	EXPECT_EQ(half->labelColour, glm::vec4(1.0f, 1.0f, 0.0f, 127.0f / 255.0f));
	EXPECT_EQ(half->shadowColour, glm::vec4(0.0f, 0.0f, 0.0f, 127.0f / 255.0f));
}
