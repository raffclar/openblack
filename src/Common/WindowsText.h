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

/// Text as an English Windows keeps it in a single byte to a character (code page 1252)
namespace openblack::windows_text
{

/// Text as an English Windows writes it. A character that the code page does not have becomes "?".
[[nodiscard]] std::string ToWindowsText(std::u16string_view text);

/// Text as an English Windows reads it. Each byte gives one character. A byte that the code page does not use gives the
/// character with the same number, as Windows does.
[[nodiscard]] std::u16string FromWindowsText(std::string_view text);

} // namespace openblack::windows_text
