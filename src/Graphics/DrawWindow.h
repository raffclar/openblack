/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <bitset>
#include <span>

namespace openblack::graphics
{

/// Lets one run of the draws into some views go to the GPU and holds back the others. The window counts the draws into
/// its views in the order that they come. A draw into another view always goes.
class DrawWindow
{
public:
	static constexpr uint16_t k_Views = 256;

	/// From now on, only the draws first to first + count - 1 into the views go
	void Open(uint32_t first, uint32_t count, std::span<const uint16_t> views) noexcept
	{
		_views.reset();
		for (const auto view : views)
		{
			if (view < k_Views)
			{
				_views.set(view);
			}
		}
		_first = first;
		_count = count;
		_seen = 0;
		_open = true;
	}

	/// Stops the window, so that all draws go again. Gives the number of draws into its views while it was open.
	uint32_t Close() noexcept
	{
		_open = false;
		return _seen;
	}

	[[nodiscard]] bool IsOpen() const noexcept { return _open; }

	/// Counts a draw into the view. Gives true if the draw goes, false if the window holds it back.
	[[nodiscard]] bool Admit(uint16_t view) noexcept
	{
		if (!_open || view >= k_Views || !_views.test(view))
		{
			return true;
		}
		const auto index = _seen++;
		return index >= _first && index - _first < _count;
	}

private:
	std::bitset<k_Views> _views;
	uint32_t _first {0};
	uint32_t _count {0};
	uint32_t _seen {0};
	bool _open {false};
};

/// The window of draws for one part of a warm-up: part n holds the draws n * drawsPerPart and after
struct WarmUpPart
{
	uint32_t first;
	uint32_t count;
};

[[nodiscard]] constexpr WarmUpPart WarmUpPartWindow(uint32_t part, uint32_t drawsPerPart) noexcept
{
	return {.first = part * drawsPerPart, .count = drawsPerPart};
}

/// Gives true if a warm-up part that counted the draws given held back draws for a later part
[[nodiscard]] constexpr bool WarmUpPartsLeft(uint32_t part, uint32_t drawsCounted, uint32_t drawsPerPart) noexcept
{
	return drawsCounted > (part + 1) * drawsPerPart;
}

} // namespace openblack::graphics
