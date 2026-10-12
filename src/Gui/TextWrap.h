/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <functional>
#include <string_view>
#include <vector>

// How Black & White breaks a text into lines: the one rule its word-wrapped text uses everywhere (the help bubble of
// the info signs, the loading screen's tips, the front end's dialogs).

namespace openblack::gui::text_wrap
{

/// The blank the help texts' loader puts for a '~': a place to break that takes no room and is never drawn
constexpr char16_t k_Tilde = 0xF8FE;

/// The white space the game breaks lines at: tab, line feed, vertical tab, form feed, carriage return and space, and
/// above U+00FF the characters Windows calls spaces. U+00A0 (no-break space) is not one.
[[nodiscard]] bool IsSpace(char16_t c);

/// How far a character moves the pen, in the font's units. Line breaks and NUL are measured as the glyphs the font
/// gives them (a font without one uses its '?').
using AdvanceFn = std::function<float(char16_t c)>;

/// The text's lines, each a view of it, for a line at most `width` wide, a character `advance(c) * scale` wide.
///
/// - A carriage return or a line feed ends a line, each on its own: "\r\n" leaves an empty line between.
/// - A line too wide breaks at its rightmost space or U+F8FE (the line ends before it, its trailing spaces and U+F8FE
///   trimmed) or just after a hyphen, looking back from the point the line first grew too wide, four characters at
///   a time. The next line starts right after that one break character: further spaces start it.
/// - A word with no place to break is cut to the most characters that fit.
/// - If not even one character fits, the text ends there: no more lines.
/// - A line ended by a carriage return or a line feed keeps the spaces before it, unless the line had to be shortened
///   back to that break.
/// The text ends at its first NUL.
[[nodiscard]] std::vector<std::u16string_view> BreakLines(std::u16string_view text, float width, float scale,
                                                          const AdvanceFn& advance);

} // namespace openblack::gui::text_wrap
