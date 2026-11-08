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

#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <InspectorJson.h>

/// The keys and letters that a tool gives to a box that waits for the player before the first land, such as the new
/// player's name box. The game shows such a box while the inspector answers as loading, so the game's frames do not
/// answer these requests.
namespace openblack::inspector::startup_box
{

/// A key that goes down or up, by its SDL name
struct Key
{
	std::string name;
	bool down {true};
};

/// Letters that the keyboard gives to the text box with the focus
struct Text
{
	std::string text;
};

using Input = std::variant<Key, Text>;

/// The most bytes of one text event, with space for the end of the text and the mark of the game's own input
constexpr size_t k_MostTextEventBytes = 30;

/// The input that input.text or input.key asks for, in the order it goes to the box. A tap is a key down, then a key up.
/// The text goes in pieces of at most k_MostTextEventBytes, each cut between two whole UTF-8 characters. An error
/// tells why the request cannot go to the box.
[[nodiscard]] std::variant<std::vector<Input>, std::string> FromRequest(std::string_view query, const Json& params);

/// Divides UTF-8 text into pieces of at most mostBytes, each cut between two whole characters
[[nodiscard]] std::vector<std::string> TextPieces(std::string_view text, size_t mostBytes);

} // namespace openblack::inspector::startup_box
