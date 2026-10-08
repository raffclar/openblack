/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <chrono>

#include <gtest/gtest.h>

#include "Gui/Controls.h"

using openblack::gui::IsCaretShown;
using std::chrono::milliseconds;

TEST(EditBoxCaret, BlinksEveryQuarterSecond)
{
	EXPECT_TRUE(IsCaretShown(milliseconds(0)));
	EXPECT_TRUE(IsCaretShown(milliseconds(249)));
	EXPECT_FALSE(IsCaretShown(milliseconds(250)));
	EXPECT_FALSE(IsCaretShown(milliseconds(499)));
	EXPECT_TRUE(IsCaretShown(milliseconds(500)));
	EXPECT_FALSE(IsCaretShown(milliseconds(1000 * 3600 + 750)));
}
