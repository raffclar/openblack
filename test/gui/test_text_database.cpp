/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <string>

#include <Gui/TextDatabase.h>
#include <gtest/gtest.h>

#include "FakeGameFont.h"

using namespace openblack::gui;
using openblack::gui::test::Utf16Script;

TEST(TextDatabase, AHelpTextKeepsItsNarratorItsFirstNumberAndItsPlace)
{
	// The script gives its narrators values. Each line has the first number, the narrator, the name and the text.
	const std::u16string script = u"GOOD_ADVISOR = 2\n"
	                              u"EVIL_ADVISOR = 3\n"
	                              u"ADD_TEXT(1, GOOD_ADVISOR, \"FIRST\", \"Hello\")\n"
	                              u"ADD_TEXT(0, EVIL_ADVISOR, \"SECOND\", \"Line\\none\")\n";
	TextDatabase texts;
	ASSERT_EQ(texts.AddHelpScript(Utf16Script(script)), 2u);
	ASSERT_EQ(texts.GetHelpTextCount(), 2u);
	const auto& first = texts.GetHelpText(0);
	EXPECT_EQ(first.narrator, 2);
	EXPECT_TRUE(first.important);
	EXPECT_EQ(first.text, u"Hello");
	const auto& second = texts.GetHelpText(1);
	EXPECT_EQ(second.narrator, 3);
	EXPECT_FALSE(second.important);
	EXPECT_EQ(second.text, u"Line\none");
	// The scripts also find the texts by name
	EXPECT_EQ(texts.Get("SECOND"), u"Line\none");
	EXPECT_EQ(texts.GetHelpNames()[1], "SECOND");
}
