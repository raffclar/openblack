/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <ctime>

#include <array>
#include <string>

namespace openblack::ecs::save_date
{

/// The text of the date label in the save game room for a broken-down time: the time first, then the long date. The
/// C library gives the text. Builds whose C++ library has no time zone data use this.
[[nodiscard]] inline std::string FormatLabel(const std::tm& time)
{
	std::array<char, 128> text {};
	const auto length = std::strftime(text.data(), text.size(), "%H:%M:%S %A, %d %B %Y", &time);
	return std::string(text.data(), length);
}

} // namespace openblack::ecs::save_date
