/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The interface's tooltips are the help system's: what they submit, in the help system's terms

#include <gtest/gtest.h>

#include "Gui/ToolTips.h"

using namespace openblack::gui;

TEST(ToolTips, AreTheHelpSystemsTooltipTexts)
{
	EXPECT_EQ(ToolTips::k_Count, 170u);
	// The first is HELP_TEXT_TOOLTIP_01, the help system's first tooltip
	EXPECT_EQ(ToolTips::TextOf(0), openblack::help::tooltips::k_First);
	EXPECT_EQ(ToolTips::TextOf(ToolTips::k_Count - 1), openblack::help::tooltips::k_First + 169u);
}

TEST(ToolTips, ShowTheButtonOfTheirAction)
{
	// The left button's binding selects, the right button's applies, and a tooltip of just words shows none
	EXPECT_EQ(ToolTips::BindingOf(ToolTipAction::Select), 1);
	EXPECT_EQ(ToolTips::BindingOf(ToolTipAction::Apply), 2);
	EXPECT_EQ(ToolTips::BindingOf(ToolTipAction::None), -1);
}

TEST(ToolTips, ArrowsAreTheHandsOwn)
{
	// The hand's tooltips ask for the same arrows: up and down, or all four
	EXPECT_EQ(ToolTipArrows::k_UpDown, 0x300u);
	EXPECT_EQ(ToolTipArrows::k_All, 0xF00u);
	EXPECT_EQ(ToolTipArrows::k_None, 0u);
}
