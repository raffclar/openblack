/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <algorithm>
#include <chrono>

namespace openblack::online
{

/// The wait before a client connects again after a loss. Each loss doubles the wait, to the most. A connection that
/// opens sets the wait to the first wait again.
class ReconnectBackoff
{
public:
	constexpr ReconnectBackoff(std::chrono::milliseconds first, std::chrono::milliseconds most) noexcept
	    : _first(first)
	    , _most(std::max(first, most))
	    , _current(first)
	{
	}

	/// The wait before the next attempt
	[[nodiscard]] constexpr std::chrono::milliseconds Current() const noexcept { return _current; }

	/// Gives the wait before the next attempt, and doubles the wait for the attempt after it
	constexpr std::chrono::milliseconds Next() noexcept
	{
		const auto wait = _current;
		_current = std::min(_current * 2, _most);
		return wait;
	}

	/// The connection opened: the next loss waits the first wait
	constexpr void Reset() noexcept { _current = _first; }

private:
	std::chrono::milliseconds _first;
	std::chrono::milliseconds _most;
	std::chrono::milliseconds _current;
};

} // namespace openblack::online
