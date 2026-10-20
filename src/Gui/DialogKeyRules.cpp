/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DialogKeyRules.h"

#include <algorithm>

namespace openblack::gui::dialog_keys
{

namespace
{

/// The game makes capitals of the letters a to z only
constexpr char16_t ToUpper(char16_t c)
{
	return c >= u'a' && c <= u'z' ? static_cast<char16_t>(c - u'a' + u'A') : c;
}

constexpr bool HasOk(Buttons buttons)
{
	return buttons == Buttons::Ok || buttons == Buttons::CancelOk;
}

} // namespace

Answer AnswerKey(Buttons buttons, Key key)
{
	switch (key)
	{
	case Key::Return:
		return HasOk(buttons) ? Answer::Ok : Answer::None;
	case Key::Escape:
		if (buttons == Buttons::CancelOk)
		{
			return Answer::Cancel;
		}
		return HasOk(buttons) ? Answer::Ok : Answer::None;
	default:
		return Answer::None;
	}
}

Answer AnswerTyped(Buttons buttons, char16_t typed, char16_t yesInitial, char16_t noInitial)
{
	if (buttons != Buttons::YesNo || yesInitial == noInitial)
	{
		return Answer::None;
	}
	const auto upper = ToUpper(typed);
	if (upper == 0)
	{
		return Answer::None;
	}
	// The game tests No after Yes, so No wins when both letters are the same capital
	auto answer = Answer::None;
	if (upper == ToUpper(yesInitial))
	{
		answer = Answer::Yes;
	}
	if (upper == ToUpper(noInitial))
	{
		answer = Answer::No;
	}
	return answer;
}

std::optional<size_t> NextFocus(std::span<const bool> canFocus, std::optional<size_t> focus, bool backwards)
{
	const auto count = canFocus.size();
	if (count == 0)
	{
		return focus;
	}
	auto step = [count, backwards](std::optional<size_t> from) -> size_t {
		if (!from.has_value())
		{
			return backwards ? count - 1 : 0;
		}
		if (backwards)
		{
			return *from == 0 ? count - 1 : *from - 1;
		}
		return *from + 1 == count ? 0 : *from + 1;
	};
	auto candidate = focus;
	for (size_t i = 0; i < count; ++i)
	{
		candidate = step(candidate);
		if (candidate == focus)
		{
			break;
		}
		if (canFocus[*candidate])
		{
			return candidate;
		}
	}
	// No other control can take the focus. The game keeps it where it was, or with no focus it never stops looking.
	return focus;
}

float MoveSlider(float value, SliderKey key)
{
	switch (key)
	{
	case SliderKey::Home:
		value = 0.0f;
		break;
	case SliderKey::End:
		value = 1.0f;
		break;
	case SliderKey::Left:
		value -= 0.1f;
		break;
	case SliderKey::Right:
		value += 0.1f;
		break;
	}
	return value > 0.0f ? std::min(value, 1.0f) : 0.0f;
}

std::optional<size_t> MoveList(std::optional<size_t> selection, size_t count, ListKey key)
{
	if (count == 0)
	{
		return std::nullopt;
	}
	switch (key)
	{
	case ListKey::Home:
		return 0;
	case ListKey::End:
		return count - 1;
	case ListKey::Up:
		return selection.has_value() && *selection > 0 ? *selection - 1 : count - 1;
	case ListKey::Down:
		return selection.has_value() && *selection + 1 < count ? *selection + 1 : 0;
	}
	return selection;
}

} // namespace openblack::gui::dialog_keys
