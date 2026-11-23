/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "InputMethodRules.h"

#include <algorithm>
#include <limits>

namespace openblack::gui::input_method
{

Shown Compose(std::u16string_view shown, int caret, std::u16string_view composition, bool open)
{
	Shown result {.text = std::u16string(shown), .compositionStart = caret, .compositionEnd = caret};
	if (!open || caret < 0 || caret > static_cast<int>(shown.size()))
	{
		return result;
	}
	result.text.insert(static_cast<size_t>(caret), composition);
	result.compositionEnd = caret + static_cast<int>(composition.size());
	return result;
}

std::optional<Run> ComposedRun(const Shown& shown, int scroll)
{
	const auto first = std::max(shown.compositionStart, scroll);
	const auto last = std::max(shown.compositionEnd, scroll);
	if (last <= first)
	{
		return std::nullopt;
	}
	return Run {.first = first, .count = last - first};
}

int Scroll(const Shown& shown, int scroll, int width, const text_edit::Measure& measure)
{
	// The scroll rule of the box, with the end of the composed text in the place of the caret
	text_edit::State state {
	    .text = shown.text,
	    .maxLength = std::numeric_limits<size_t>::max(),
	    .caret = shown.compositionEnd,
	    .selectionStart = shown.compositionEnd,
	    .selectionEnd = shown.compositionEnd,
	    .scroll = scroll,
	};
	text_edit::Scroll(state, width, measure);
	return state.scroll;
}

} // namespace openblack::gui::input_method
