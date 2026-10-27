/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>
#include <string>
#include <string_view>

/// The message of a land: the land information box that the script of a playground land fills as the land loads. The
/// script gives the title of the box first, and then each line of the list under the title.
namespace openblack::land_message
{

/// One command of the script for the land information box
struct Message
{
	enum class Kind : uint8_t
	{
		/// The box shows with this title and an empty list. Every other box closes.
		Start,
		/// The list of the box gets this line at its end
		AddLine,
	};

	Kind kind {Kind::Start};
	/// The text of the script line
	std::string text;
	/// The number of the script line. With 0, the box shows the text of the script line.
	int32_t number {0};

	bool operator==(const Message&) const = default;
};

/// The help text that the box shows in place of the text of the script line. A number of 0 gives none: the box shows
/// the text of the script line. The numbers 3 and 4 give the lines for two and three gods to defeat. Every other number
/// gives the line for one god to defeat. The title and each line of the list follow the same rule.
[[nodiscard]] constexpr std::optional<std::string_view> HelpTextOf(int32_t number) noexcept
{
	switch (number)
	{
	case 0:
		return std::nullopt;
	case 3:
		return "HELP_TEXT_LAST_LINES_02";
	case 4:
		return "HELP_TEXT_LAST_LINES_03";
	default:
		return "HELP_TEXT_LAST_LINES_01";
	}
}

} // namespace openblack::land_message
