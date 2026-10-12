/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "Gui/TextEditRules.h"

using namespace openblack::gui::text_edit;

namespace
{
/// Every character 10 pixels wide
int TenEach(std::u16string_view text)
{
	return static_cast<int>(text.size()) * 10;
}

State Box(std::u16string text, size_t maxLength = 29)
{
	State state {.text = std::move(text), .maxLength = maxLength};
	const auto end = static_cast<int>(state.text.size());
	state.caret = end;
	state.selectionStart = end;
	state.selectionEnd = end;
	return state;
}
} // namespace

TEST(TextEdit, TypingGoesInAtTheCaretUntilTheBoxIsFull)
{
	auto state = Box(u"", 3);
	for (const auto c : std::u16string_view(u"Matt"))
	{
		Type(state, c);
	}
	EXPECT_EQ(state.text, u"Mat");
	EXPECT_EQ(state.caret, 3);
	// Control characters other than Backspace do nothing
	Type(state, u'\r');
	EXPECT_EQ(state.text, u"Mat");
	Type(state, u'\b');
	EXPECT_EQ(state.text, u"Ma");
	EXPECT_EQ(state.caret, 2);
	Press(state, Key::Home, false);
	Type(state, u'X');
	EXPECT_EQ(state.text, u"XMa");
	EXPECT_EQ(state.caret, 1);
	Press(state, Key::Delete, false);
	EXPECT_EQ(state.text, u"Xa");
}

TEST(TextEdit, TakingTheFocusSelectsAllAndTheFirstClickSelectsAllAgain)
{
	auto state = Box(u"Matt");
	SetFocus(state, true);
	EXPECT_EQ(state.selectionStart, 0);
	EXPECT_EQ(state.selectionEnd, 4);
	// The click goes down at a character, but selects it all as it is let go
	MouseDown(state, 2);
	MouseUp(state, 2);
	EXPECT_EQ(state.selectionStart, 0);
	EXPECT_EQ(state.selectionEnd, 4);
	// Typing replaces the selection
	Type(state, u'A');
	EXPECT_EQ(state.text, u"A");
	// A second click just places the caret
	MouseDown(state, 0);
	MouseUp(state, 0);
	EXPECT_EQ(state.caret, 0);
	EXPECT_EQ(state.selectionStart, state.selectionEnd);
	// Losing the focus puts the caret at the end, nothing selected
	SetFocus(state, false);
	EXPECT_EQ(state.caret, 1);
	EXPECT_EQ(state.selectionStart, 1);
	EXPECT_EQ(state.selectionEnd, 1);
}

TEST(TextEdit, DraggingAndShiftSelect)
{
	auto state = Box(u"abcdef");
	MouseDown(state, 1);
	Drag(state, 4);
	MouseUp(state, 4);
	EXPECT_EQ(state.selectionStart, 1);
	EXPECT_EQ(state.selectionEnd, 4);
	Press(state, Key::Delete, false);
	EXPECT_EQ(state.text, u"aef");
	EXPECT_EQ(state.caret, 1);
	Press(state, Key::Right, true);
	Press(state, Key::Right, true);
	EXPECT_EQ(state.selectionStart, 1);
	EXPECT_EQ(state.selectionEnd, 3);
	Press(state, Key::Home, true);
	EXPECT_EQ(state.selectionStart, 0);
	EXPECT_EQ(state.selectionEnd, 3);
	Press(state, Key::Left, false);
	EXPECT_EQ(state.caret, 0);
	EXPECT_EQ(state.selectionStart, state.selectionEnd);
}

TEST(TextEdit, AClickFallsBeforeTheCharacterItIsOn)
{
	auto state = Box(u"abcd");
	// The box's left at 100, its text 4 pixels in: the first character ends at 114
	EXPECT_EQ(CharacterAt(state, 100, 113, TenEach), 0);
	EXPECT_EQ(CharacterAt(state, 100, 115, TenEach), 1);
	EXPECT_EQ(CharacterAt(state, 100, 500, TenEach), 4);
	state.scroll = 2;
	EXPECT_EQ(CharacterAt(state, 100, 115, TenEach), 3);
}

TEST(TextEdit, TheTextScrollsToKeepTheCaretInTheBox)
{
	// A box 59 wide shows less than 50 pixels of text: four characters
	auto state = Box(u"abcdefgh");
	Scroll(state, 59, TenEach);
	EXPECT_EQ(state.scroll, 4);
	Press(state, Key::Home, false);
	Scroll(state, 59, TenEach);
	EXPECT_EQ(state.scroll, 0);
	// Text that fits doesn't scroll
	auto fits = Box(u"abc");
	Scroll(fits, 59, TenEach);
	EXPECT_EQ(fits.scroll, 0);
}
