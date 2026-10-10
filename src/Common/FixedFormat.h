/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <string>
#include <string_view>

/// Numbers written as the game's text writes them: in fixed point, rounded as its C library rounds, half away from
/// nothing, so 12.5 shows as 13
namespace openblack::fixed_format
{

/// A number in fixed point with some digits after the point, right aligned to at least a width, as "%3.0f" writes it
[[nodiscard]] std::string Fixed(double value, int width, int precision);

/// A text with a number written into it: each "%f" in it, with any width and digits after the point ("%3.0f"), is the
/// number, and "%%" is a percent sign. Anything else is left as it is.
[[nodiscard]] std::u16string WithNumber(std::u16string_view text, double value);

} // namespace openblack::fixed_format
