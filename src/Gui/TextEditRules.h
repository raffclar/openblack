/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <functional>
#include <string>
#include <string_view>

/// The text of the dialogs' edit boxes as the player types into it: a caret, a selection and how far the text has
/// scrolled to keep the caret in sight. Positions are counted in characters.
namespace openblack::gui::text_edit
{

/// The keys an edit box acts on
enum class Key : uint8_t
{
	Home,
	End,
	Left,
	Right,
	Delete,
};

struct State
{
	std::u16string text;
	/// The most characters the text can have
	size_t maxLength {0};
	int caret {0};
	/// The selection runs from its start to its end; it is empty when they are the same
	int selectionStart {0};
	int selectionEnd {0};
	/// The first character shown
	int scroll {0};
	/// The next click selects the whole text, as the first click into a box that has just taken the focus does
	bool selectAllOnClick {false};
};

/// The width of a run of text, in whole pixels of the dialog space
using Measure = std::function<int(std::u16string_view)>;

/// The box takes the focus, with the whole text selected and the next click selecting it all again, or loses it
void SetFocus(State& state, bool focused);
/// Typing a character: Backspace (8) takes out the selection or the character before the caret; a printable character
/// replaces the selection, and goes in at the caret while the text has room, the caret moving on even when it doesn't.
/// Other control characters do nothing.
void Type(State& state, char16_t character);
/// A key: Home and End go to either end, Left and Right a character, Delete takes out the selection or the character
/// after the caret. With Shift held the selection follows the caret instead.
void Press(State& state, Key key, bool shift);
/// The character a point of the box is before: the first whose width from the first shown, 4 pixels in from the box's
/// left, ends beyond the point
[[nodiscard]] int CharacterAt(const State& state, int left, int x, const Measure& measure);
/// The mouse button goes down at a character, or is held there
void MouseDown(State& state, int character);
void Drag(State& state, int character);
/// The mouse button is let go at a character: the selection is ordered, and a click that selected nothing selects all
/// of the text when the box has just taken the focus
void MouseUp(State& state, int character);
/// How far the text scrolls for the box's width to keep the caret in sight, no further than needed to show its end
void Scroll(State& state, int width, const Measure& measure);

} // namespace openblack::gui::text_edit
