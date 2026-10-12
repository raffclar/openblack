/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <string>
#include <string_view>
#include <vector>

#include <Gui/TextWrap.h>
#include <gtest/gtest.h>

using namespace openblack::gui::text_wrap;

namespace
{
/// Ten units a character, NUL and the line breaks included; the hidden blank takes no room
float Advance(char16_t c)
{
	return c == k_Tilde ? 0.0f : 10.0f;
}

/// The lines at a width of `characters` characters, each in brackets
std::u16string Lines(std::u16string_view text, int characters)
{
	std::u16string result;
	for (const auto line : BreakLines(text, static_cast<float>(characters) * 10.0f, 1.0f, Advance))
	{
		result += u"[";
		result += line;
		result += u"]";
	}
	return result;
}
} // namespace

TEST(TextWrap, BreaksAtTheLastSpaceThatFits)
{
	EXPECT_EQ(Lines(u"short", 10), u"[short]");
	EXPECT_EQ(Lines(u"one two three four", 10), u"[one two][three four]");
	// The spaces before the break are trimmed
	EXPECT_EQ(Lines(u"aaaa    bbbbbbb", 10), u"[aaaa][bbbbbbb]");
	// The scale turns the units into the width's
	EXPECT_EQ(BreakLines(u"ab cd", 4.0f, 0.1f, Advance).size(), 2);
}

TEST(TextWrap, KeepsTheSpacesAfterTheBreak)
{
	// Measured four characters at a time, the line first grows too wide at eight characters, so it breaks at the
	// first of the two spaces; the second starts the next line
	EXPECT_EQ(Lines(u"aaaaaaa  bb", 7), u"[aaaaaaa][ bb]");
	// A space after a line break starts its line too
	EXPECT_EQ(Lines(u"ab\n cd", 10), u"[ab][ cd]");
}

TEST(TextWrap, KeepsTheHyphenAndCutsWordsWithNowhereToBreak)
{
	EXPECT_EQ(Lines(u"well-known words", 10), u"[well-known][words]");
	EXPECT_EQ(Lines(u"well-knownwords", 10), u"[well-][knownwords]");
	EXPECT_EQ(Lines(u"abcdefghijklmnop", 10), u"[abcdefghij][klmnop]");
}

TEST(TextWrap, StopsWhenNotACharacterFits)
{
	EXPECT_TRUE(BreakLines(u"abc", 5.0f, 1.0f, Advance).empty());
	EXPECT_TRUE(BreakLines(u"a\nb", 5.0f, 1.0f, Advance).empty());
}

TEST(TextWrap, EndsALineAtEachLineFeedOrCarriageReturn)
{
	EXPECT_EQ(Lines(u"ab\ncd", 10), u"[ab][cd]");
	EXPECT_EQ(Lines(u"ab\rcd", 10), u"[ab][cd]");
	// Both of a CR LF break the line: an empty line between
	EXPECT_EQ(Lines(u"ab\r\ncd", 10), u"[ab][][cd]");
	EXPECT_EQ(Lines(u"ab\n\ncd", 10), u"[ab][][cd]");
	// A line feed first is an empty line; one last adds none
	EXPECT_EQ(Lines(u"\nab", 10), u"[][ab]");
	EXPECT_EQ(Lines(u"ab\n", 10), u"[ab]");
	EXPECT_EQ(Lines(u"ab\n\n", 10), u"[ab][]");
}

TEST(TextWrap, ShowsABackslashAndNAsTheyAre)
{
	// Only the loader turns a backslash and n into a line feed; a text that still has them shows them
	EXPECT_EQ(Lines(u"ab\\ncd", 10), u"[ab\\ncd]");
}

TEST(TextWrap, KeepsTheSpacesBeforeALineBreakUnlessShortenedBackToIt)
{
	// The line is measured on past the line feed into the next words, and fits back at the space after "cd": the
	// line feed then ends it, the space before the line feed kept
	EXPECT_EQ(Lines(u"ab \ncd ef", 10), u"[ab ][cd ef]");
	// With no space after the line feed the line goes back to the line feed itself and is trimmed there
	EXPECT_EQ(Lines(u"ab \ncd", 10), u"[ab][cd]");
}

TEST(TextWrap, BreaksAtTheHiddenBlankAndTrimsIt)
{
	const std::u16string hidden(1, k_Tilde);
	EXPECT_EQ(Lines(u"aaaaaa" + hidden + u"bbbbbbbbb", 10), u"[aaaaaa][bbbbbbbbb]");
	EXPECT_EQ(Lines(u"aaaaaa " + hidden + u"bbbbbbbbb", 10), u"[aaaaaa][bbbbbbbbb]");
}

TEST(TextWrap, BreaksOnlyAtTheGamesSpaces)
{
	EXPECT_TRUE(IsSpace(u' '));
	EXPECT_TRUE(IsSpace(u'\t'));
	EXPECT_TRUE(IsSpace(u'\n'));
	EXPECT_TRUE(IsSpace(u'\v'));
	EXPECT_TRUE(IsSpace(u'\f'));
	EXPECT_TRUE(IsSpace(u'\r'));
	EXPECT_TRUE(IsSpace(u'\x3000'));
	EXPECT_FALSE(IsSpace(u'\xA0'));
	EXPECT_FALSE(IsSpace(u'\x85'));
	EXPECT_FALSE(IsSpace(k_Tilde));
	EXPECT_FALSE(IsSpace(u'-'));
	// A no-break space is no place to break: the word is cut where it reaches the end
	EXPECT_EQ(Lines(u"aaaa\xA0"
	                u"bbbbbbb",
	                10),
	          u"[aaaa\xA0"
	          u"bbbbb][bb]");
	EXPECT_EQ(Lines(u"aaaa\tbbbbbbb", 10), u"[aaaa][bbbbbbb]");
}

TEST(TextWrap, EndsTheTextAtANul)
{
	EXPECT_EQ(Lines(std::u16string_view(u"ab\0cd", 5), 10), u"[ab]");
}
