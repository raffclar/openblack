/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GodWindowRules.h"

#include <algorithm>
#include <string>

namespace openblack::gui::god_window
{

namespace
{
/// The last first god that still shows a full window
[[nodiscard]] size_t LastFirst(size_t gods) noexcept
{
	return gods > k_Shown ? gods - k_Shown : 0;
}

[[nodiscard]] std::u16string Number(size_t value)
{
	const auto digits = std::to_string(value);
	return {digits.begin(), digits.end()};
}
} // namespace

size_t Clamp(size_t gods, std::ptrdiff_t first) noexcept
{
	if (first <= 0)
	{
		return 0;
	}
	return std::min(static_cast<size_t>(first), LastFirst(gods));
}

size_t Move(size_t gods, size_t first, int pages) noexcept
{
	const auto step = static_cast<std::ptrdiff_t>(k_Shown) * pages;
	return Clamp(gods, static_cast<std::ptrdiff_t>(Clamp(gods, static_cast<std::ptrdiff_t>(first))) + step);
}

bool CanMoveBack(size_t gods, size_t first) noexcept
{
	return Clamp(gods, static_cast<std::ptrdiff_t>(first)) > 0;
}

bool CanMoveOn(size_t gods, size_t first) noexcept
{
	return Clamp(gods, static_cast<std::ptrdiff_t>(first)) < LastFirst(gods);
}

std::vector<size_t> Shown(size_t gods, size_t first)
{
	const auto start = Clamp(gods, static_cast<std::ptrdiff_t>(first));
	std::vector<size_t> shown;
	for (size_t i = start; i < gods && i < start + k_Shown; ++i)
	{
		shown.push_back(i);
	}
	return shown;
}

std::u16string Label(size_t gods, size_t first)
{
	const auto shown = Shown(gods, first);
	if (shown.empty())
	{
		return {};
	}
	return u"Players " + Number(shown.front() + 1) + u"-" + Number(shown.back() + 1) + u" of " + Number(gods);
}

} // namespace openblack::gui::god_window
