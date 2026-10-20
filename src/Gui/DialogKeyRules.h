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

#include <optional>
#include <span>

/// How the keyboard works in the game's dialogs: which control has the focus after Tab, and which button of a message
/// box a key or a typed letter presses
namespace openblack::gui::dialog_keys
{

/// The buttons of a message box over a dialog
enum class Buttons
{
	/// One OK button
	Ok,
	/// A Cancel and an OK button
	CancelOk,
	/// A Yes and a No button
	YesNo,
};

/// The button that a key or a letter presses
enum class Answer
{
	None,
	Ok,
	Cancel,
	Yes,
	No,
};

/// The keys that a message box looks at
enum class Key
{
	/// The Enter key of the main keyboard or of the number pad
	Return,
	Escape,
	Other,
};

/// The button that a key pressed in a message box presses:
/// - Return presses OK when the box has an OK button.
/// - Escape presses Cancel when the box has a Cancel button, else OK when it has an OK button.
/// A Yes and No box takes neither key.
[[nodiscard]] Answer AnswerKey(Buttons buttons, Key key);

/// The button that a letter typed in a message box presses. A box with a Yes and a No button takes the first letter of
/// the word on each button, in capitals or not. When the two first letters are the same character, no letter answers.
[[nodiscard]] Answer AnswerTyped(Buttons buttons, char16_t typed, char16_t yesInitial, char16_t noInitial);

/// The control that takes the focus after Tab, or Shift and Tab when backwards. The controls are in the order of Tab,
/// each true when it can take the focus: it is visible and it is not a static text. Tab goes to the next one that can
/// take the focus, after the last one to the first, and with no focus to the first. Backwards it goes the other way,
/// and with no focus to the last. When no other control can take it, the focus stays. None when no control has the
/// focus and none can take it.
[[nodiscard]] std::optional<size_t> NextFocus(std::span<const bool> canFocus, std::optional<size_t> focus, bool backwards);

/// A slider moved by a key: Home puts it at the start, End at the end, and the left and right arrows step it a tenth.
/// The value stays from 0 to 1.
enum class SliderKey
{
	Home,
	End,
	Left,
	Right,
};
[[nodiscard]] float MoveSlider(float value, SliderKey key);

/// A list moved by a key: Home picks the first item, End the last, and the up and down arrows the one before or after.
/// Up from the first item, or from none, picks the last; down from the last picks the first. None when the list is
/// empty.
enum class ListKey
{
	Home,
	End,
	Up,
	Down,
};
[[nodiscard]] std::optional<size_t> MoveList(std::optional<size_t> selection, size_t count, ListKey key);

} // namespace openblack::gui::dialog_keys
