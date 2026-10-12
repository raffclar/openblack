/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TextEditRules.h"

#include <algorithm>
#include <utility>

namespace openblack::gui::text_edit
{
namespace
{
int Length(const State& state)
{
	return static_cast<int>(state.text.size());
}

int ClampToText(const State& state, int position)
{
	return std::clamp(position, 0, Length(state));
}

/// Takes out the characters from one place to another, both kept within the text
void Erase(State& state, int from, int to)
{
	const auto first = ClampToText(state, from);
	const auto last = std::max(first, ClampToText(state, to));
	state.text.erase(static_cast<size_t>(first), static_cast<size_t>(last - first));
}

/// After a key or a character: every place kept within the text, the text within its length, the selection in order
/// and the scroll no further on than the caret
void Settle(State& state)
{
	if (state.text.size() > state.maxLength)
	{
		state.text.resize(state.maxLength);
	}
	state.selectionStart = ClampToText(state, state.selectionStart);
	state.selectionEnd = ClampToText(state, state.selectionEnd);
	state.caret = ClampToText(state, state.caret);
	state.scroll = ClampToText(state, state.scroll);
	if (state.selectionEnd < state.selectionStart)
	{
		std::swap(state.selectionStart, state.selectionEnd);
	}
	state.scroll = std::min(state.scroll, state.caret);
}

/// The caret moves to a place with Shift held: the end of the selection it is at moves with it
void ExtendTo(State& state, int place)
{
	if (state.selectionStart != state.selectionEnd && state.caret != state.selectionStart)
	{
		state.selectionEnd = place;
	}
	else
	{
		state.selectionStart = place;
	}
	state.caret = place;
}
} // namespace

void SetFocus(State& state, bool focused)
{
	if (focused)
	{
		state.selectAllOnClick = true;
	}
	state.caret = Length(state);
	state.selectionStart = Length(state);
	state.selectionEnd = Length(state);
	state.scroll = 0;
	if (focused)
	{
		state.selectionStart = 0;
	}
}

void Type(State& state, char16_t character)
{
	const bool selected = state.selectionStart != state.selectionEnd;
	if (character == u'\b')
	{
		if (selected)
		{
			Erase(state, state.selectionStart, state.selectionEnd);
			state.selectionEnd = ClampToText(state, state.selectionStart);
			state.selectionStart = state.selectionEnd;
			state.caret = state.selectionEnd;
		}
		else if (state.caret > 0)
		{
			Erase(state, state.caret - 1, state.caret);
			--state.caret;
			state.selectionStart = state.caret;
			state.selectionEnd = state.caret;
		}
		return;
	}
	if (character < u' ')
	{
		return;
	}
	if (selected)
	{
		Erase(state, state.selectionStart, state.selectionEnd);
		state.selectionEnd = ClampToText(state, state.selectionStart);
		state.caret = state.selectionEnd;
	}
	if (state.text.size() < state.maxLength)
	{
		state.text.insert(static_cast<size_t>(ClampToText(state, state.caret)), 1, character);
	}
	++state.caret;
	state.selectionStart = state.caret;
	state.selectionEnd = state.caret;
	Settle(state);
}

void Press(State& state, Key key, bool shift)
{
	switch (key)
	{
	case Key::Home:
		state.caret = 0;
		state.selectionStart = 0;
		if (!shift)
		{
			state.selectionEnd = 0;
		}
		break;
	case Key::End:
		state.caret = Length(state);
		state.selectionEnd = Length(state);
		if (!shift)
		{
			state.selectionStart = Length(state);
		}
		break;
	case Key::Left:
	case Key::Right:
	{
		const auto place = state.caret + (key == Key::Left ? -1 : 1);
		if (!shift)
		{
			state.caret = place;
			state.selectionStart = place;
			state.selectionEnd = place;
		}
		else if (state.selectionStart == state.selectionEnd)
		{
			// The selection starts: leftwards from its start, rightwards from its end
			if (key == Key::Left)
			{
				state.selectionStart = place;
			}
			else
			{
				state.selectionEnd = place;
			}
			state.caret = place;
		}
		else
		{
			ExtendTo(state, place);
		}
		break;
	}
	case Key::Delete:
		if (state.selectionStart != state.selectionEnd)
		{
			Erase(state, state.selectionStart, state.selectionEnd);
			state.caret = state.selectionStart;
		}
		else
		{
			Erase(state, state.caret, state.caret + 1);
		}
		state.selectionStart = state.caret;
		state.selectionEnd = state.caret;
		break;
	}
	Settle(state);
}

int CharacterAt(const State& state, int left, int x, const Measure& measure)
{
	const auto shown = std::u16string_view(state.text).substr(static_cast<size_t>(ClampToText(state, state.scroll)));
	for (size_t count = 1; count <= shown.size(); ++count)
	{
		if (left + measure(shown.substr(0, count)) + 4 > x)
		{
			return state.scroll + static_cast<int>(count) - 1;
		}
	}
	return Length(state);
}

void MouseDown(State& state, int character)
{
	state.caret = character;
	state.selectionStart = character;
	state.selectionEnd = character;
}

void Drag(State& state, int character)
{
	state.caret = character;
	state.selectionStart = character;
}

void MouseUp(State& state, int character)
{
	state.caret = character;
	state.selectionStart = character;
	if (state.selectionStart > state.selectionEnd)
	{
		std::swap(state.selectionStart, state.selectionEnd);
	}
	if (state.selectionStart == state.selectionEnd && state.selectAllOnClick)
	{
		state.selectionStart = 0;
		state.caret = Length(state);
		state.selectionEnd = Length(state);
	}
	state.selectAllOnClick = false;
}

void Scroll(State& state, int width, const Measure& measure)
{
	const auto text = std::u16string_view(state.text);
	const auto room = width - 9;
	// The furthest it need scroll: the first character from which the rest of the text fits
	int furthest = 0;
	while (furthest < Length(state) && measure(text.substr(static_cast<size_t>(furthest))) >= room)
	{
		++furthest;
	}
	state.scroll = std::clamp(state.scroll, 0, furthest);
	state.scroll = std::min(state.scroll, state.caret);
	const auto caret = ClampToText(state, state.caret);
	while (state.scroll < furthest && measure(text.substr(static_cast<size_t>(state.scroll),
	                                                      static_cast<size_t>(std::max(0, caret - state.scroll)))) >= room)
	{
		++state.scroll;
	}
}

} // namespace openblack::gui::text_edit
