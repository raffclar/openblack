/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <string_view>

#include <gtest/gtest.h>

#include "Gui/InputMethodRules.h"

using namespace openblack::gui::input_method;

namespace
{
/// Every character 10 pixels wide
int TenEach(std::u16string_view text)
{
	return static_cast<int>(text.size()) * 10;
}
} // namespace

TEST(InputMethod, TheComposedTextGoesInAtTheCaret)
{
	const auto shown = Compose(u"abcd", 2, u"XYZ", true);
	EXPECT_EQ(shown.text, u"abXYZcd");
	EXPECT_EQ(shown.compositionStart, 2);
	// The caret shows after the composed text
	EXPECT_EQ(shown.compositionEnd, 5);
}

TEST(InputMethod, TheComposedTextGoesInAtTheStartAndAtTheEnd)
{
	EXPECT_EQ(Compose(u"ab", 0, u"X", true).text, u"Xab");
	const auto atEnd = Compose(u"ab", 2, u"X", true);
	EXPECT_EQ(atEnd.text, u"abX");
	EXPECT_EQ(atEnd.compositionStart, 2);
	EXPECT_EQ(atEnd.compositionEnd, 3);
}

TEST(InputMethod, TheBoxShowsNoComposedTextWithoutTheFocusOrWithTheCaretOutOfTheText)
{
	for (const auto& shown : {Compose(u"ab", 1, u"X", false), Compose(u"ab", 3, u"X", true)})
	{
		EXPECT_EQ(shown.text, u"ab");
		EXPECT_EQ(shown.compositionStart, shown.compositionEnd);
		EXPECT_FALSE(ComposedRun(shown, 0).has_value());
	}
	const auto before = Compose(u"ab", -1, u"X", true);
	EXPECT_EQ(before.text, u"ab");
	EXPECT_EQ(before.compositionEnd, -1);
}

TEST(InputMethod, AnEmptyCompositionShowsNothing)
{
	const auto shown = Compose(u"ab", 1, u"", true);
	EXPECT_EQ(shown.text, u"ab");
	EXPECT_EQ(shown.compositionStart, 1);
	EXPECT_EQ(shown.compositionEnd, 1);
	EXPECT_FALSE(ComposedRun(shown, 0).has_value());
}

TEST(InputMethod, ABoxThatHidesItsTextShowsTheComposedTextAsItIs)
{
	// The box gives its text as '#' signs; the composed text goes in without a change
	EXPECT_EQ(Compose(u"###", 3, u"ab", true).text, u"###ab");
}

TEST(InputMethod, TheOrangeRunStartsAtTheFirstCharacterShown)
{
	const auto shown = Compose(u"abcdef", 2, u"XYZ", true);
	const auto all = ComposedRun(shown, 0);
	ASSERT_TRUE(all.has_value());
	EXPECT_EQ(all->first, 2);
	EXPECT_EQ(all->count, 3);
	// The text scrolled into the composed text: only its part from the first character shown is drawn again
	const auto part = ComposedRun(shown, 3);
	ASSERT_TRUE(part.has_value());
	EXPECT_EQ(part->first, 3);
	EXPECT_EQ(part->count, 2);
	EXPECT_FALSE(ComposedRun(shown, 5).has_value());
}

TEST(InputMethod, TheTextScrollsToKeepTheEndOfTheComposedTextInSight)
{
	// A box 69 wide has room for 6 characters of 10 before the width reaches 60
	const auto shown = Compose(u"abcd", 4, u"WXYZ", true);
	EXPECT_EQ(Scroll(shown, 0, 69, TenEach), 3);
	// Without the composed text the box does not scroll
	EXPECT_EQ(Scroll(Compose(u"abcd", 4, u"WXYZ", false), 0, 69, TenEach), 0);
}

TEST(InputMethod, TheScrollStaysWhenTheComposedTextGetsShorter)
{
	// The long composed text scrolls the text by one character
	const auto scroll = Scroll(Compose(u"abcdefgh", 2, u"WXYZ", true), 0, 69, TenEach);
	EXPECT_EQ(scroll, 1);
	// The scroll starts from its last value, so it stays when the composed text gets shorter
	EXPECT_EQ(Scroll(Compose(u"abcdefgh", 2, u"WX", true), scroll, 69, TenEach), 1);
	// It moves back only when it is after the caret or further than the text needs
	EXPECT_EQ(Scroll(Compose(u"abcd", 4, u"", true), 3, 69, TenEach), 0);
	EXPECT_EQ(Scroll(Compose(u"abcdefgh", 0, u"", true), 3, 69, TenEach), 0);
}

TEST(InputMethod, TheComposedTextIsOrange)
{
	static_assert(k_CompositionColour == 0xFFC08020);
	SUCCEED();
}
