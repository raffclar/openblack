/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>

#include <span>
#include <string>
#include <string_view>

/// The web page of a creature, which the game makes from the template Scripts/creature_html.tmpl in the game folder.
///
/// The template is a text file. Each mark [$name$] in it shows where the value of a name goes. The game reads the
/// template as a Windows text file: it stops at the first end-of-file character (0x1A), and each carriage return
/// before a line feed is removed. It writes the page as a Windows text file too, with a carriage return before each
/// line feed.
namespace openblack::creaturehtml
{

/// The longest part of a line that the game fills in one step. A longer line is filled in parts of this length.
constexpr size_t k_LongestPart = 2047;

/// A value for the marks of a name
struct Value
{
	std::string name;
	std::string value;
};

/// Fills a template with values. Each mark [$name$] becomes the value of the name. When more than one value has the
/// same name, the last one counts. The rules are those of the game:
/// - The "$" after a mark's name and the character after that "$" are removed, whatever that character is.
/// - A mark with a name that has no value stops the part of the line that it is in: the rest of that part, its line
///   feed too, does not go into the page.
/// - A mark that has no "$" before the end of its part takes the rest of the part as its name.
/// - Each part of a line stops at the first zero byte in it.
[[nodiscard]] std::string Fill(std::string_view page, std::span<const Value> values);

} // namespace openblack::creaturehtml
