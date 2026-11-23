/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>
#include <string>
#include <string_view>

#include "TextEditRules.h"

/// The text that the player composes with an input method (IME) of the system, before the input method gives the
/// characters. An edit box that has the focus shows this text at its caret. Positions are counted in characters.
namespace openblack::gui::input_method
{

/// The colour of the composed text: orange, as alpha, red, green and blue
inline constexpr uint32_t k_CompositionColour = 0xFFC08020;

/// The text that an edit box shows, with the composed text in it
struct Shown
{
	std::u16string text;
	/// The composed text runs from its start to its end. Both are the caret when the box shows no composed text.
	int compositionStart {0};
	/// The caret shows at the end of the composed text
	int compositionEnd {0};
};

/// A run of the shown text: the first character and the number of characters
struct Run
{
	int first {0};
	int count {0};
};

/// The box puts the composed text into its shown text at the caret. This occurs only when the box has the focus, the
/// input method is open and the caret is in the text. A box that hides its text shows the composed text as it is.
[[nodiscard]] Shown Compose(std::u16string_view shown, int caret, std::u16string_view composition, bool open);

/// The part of the composed text that the box draws in orange, from the first character shown. It is empty when the
/// box shows no composed text or the composed text is before the first character shown.
[[nodiscard]] std::optional<Run> ComposedRun(const Shown& shown, int scroll);

/// How far the shown text scrolls for the box's width to keep the end of the composed text in sight. The scroll
/// starts from its last value, as the scroll of the box does without composed text.
[[nodiscard]] int Scroll(const Shown& shown, int scroll, int width, const text_edit::Measure& measure);

} // namespace openblack::gui::input_method
