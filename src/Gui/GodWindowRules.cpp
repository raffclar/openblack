/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GodWindowRules.h"

#include <cmath>

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

/// The length of the strip, never less than the view
[[nodiscard]] float StripLength(size_t gods, int column) noexcept
{
	return static_cast<float>(std::max(static_cast<int>(gods) * column, ViewWidth(column)));
}

/// The length that the thumb moves along: the bar less the thumb's inset at each end
[[nodiscard]] float Track(int barLength) noexcept
{
	return static_cast<float>(std::max(barLength - (2 * k_ThumbInset), 1));
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

float MostScroll(size_t gods, int column) noexcept
{
	return static_cast<float>(static_cast<int>(LastFirst(gods)) * column);
}

float ClampScroll(size_t gods, int column, float scroll) noexcept
{
	// A scroll that is not a number goes to the start
	if (!(scroll > 0.0f))
	{
		return 0.0f;
	}
	return std::min(scroll, MostScroll(gods, column));
}

size_t FirstAt(size_t gods, int column, float scroll) noexcept
{
	if (column <= 0)
	{
		return 0;
	}
	const auto nearest = std::lround(ClampScroll(gods, column, scroll) / static_cast<float>(column));
	return Clamp(gods, static_cast<std::ptrdiff_t>(nearest));
}

float Step(size_t gods, int column, float scroll, int steps) noexcept
{
	if (column <= 0 || steps == 0)
	{
		return ClampScroll(gods, column, scroll);
	}
	const auto columns = ClampScroll(gods, column, scroll) / static_cast<float>(column);
	// On, the view goes to the next column edge after the scroll; back, to the column edge before it
	const auto edge = steps > 0 ? std::floor(columns) : std::ceil(columns);
	return ClampScroll(gods, column, (edge + static_cast<float>(steps)) * static_cast<float>(column));
}

float Wheel(size_t gods, int column, float scroll, int notches, int across, int notchLength) noexcept
{
	const auto moved = static_cast<float>((across - notches) * notchLength);
	return ClampScroll(gods, column, ClampScroll(gods, column, scroll) + moved);
}

std::vector<VisibleColumn> Visible(size_t gods, int column, float scroll, int viewLeft)
{
	std::vector<VisibleColumn> visible;
	if (column <= 0)
	{
		return visible;
	}
	const auto start = viewLeft - static_cast<int>(ClampScroll(gods, column, scroll));
	const auto viewRight = viewLeft + ViewWidth(column);
	for (size_t god = 0; god < gods; ++god)
	{
		const auto left = start + (static_cast<int>(god) * column);
		if (left < viewRight && left + column > viewLeft)
		{
			visible.push_back({.god = god, .left = left});
		}
	}
	return visible;
}

Thumb ThumbOf(size_t gods, int column, float scroll, int barLength) noexcept
{
	// As the thumb of a list: the share of the strip before the view and the share up to the end of the view, along the
	// track, each cut towards nothing
	const auto track = Track(barLength);
	const auto strip = StripLength(gods, column);
	const auto clamped = ClampScroll(gods, column, scroll);
	return {.start = static_cast<int>(track * clamped / strip),
	        .end = static_cast<int>(track * (clamped + static_cast<float>(ViewWidth(column))) / strip)};
}

BarPart PartAt(size_t gods, int column, float scroll, int barLength, int along) noexcept
{
	const auto thumb = ThumbOf(gods, column, scroll, barLength);
	const auto inner = along - k_ThumbInset;
	if (inner < thumb.start)
	{
		return BarPart::Before;
	}
	return inner < thumb.end ? BarPart::Thumb : BarPart::After;
}

float Drag(size_t gods, int column, float grabScroll, int moved, int barLength) noexcept
{
	return ClampScroll(gods, column, grabScroll + (static_cast<float>(moved) * StripLength(gods, column) / Track(barLength)));
}

float Page(size_t gods, int column, float scroll, bool on) noexcept
{
	const auto view = static_cast<float>(ViewWidth(column));
	return ClampScroll(gods, column, ClampScroll(gods, column, scroll) + (on ? view : -view));
}

} // namespace openblack::gui::god_window
