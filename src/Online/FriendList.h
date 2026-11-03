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

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ECS/ChatRules.h"

/// The friends of the gathering box that the profile of the player keeps. The number of each friend is the id of its
/// lobby account. The file is text in UTF-8: a first line with the name and the version of the format, then one line
/// for each friend from the top, with the number, a tab and the name.
namespace openblack::online::friend_list
{

/// The name of the file in the folder of the profile
constexpr std::string_view k_FileName = "friendlist.txt";
/// The first line of the file
constexpr std::string_view k_Header = "openblack friend list 1";

/// The text of the file. A tab, a carriage return or a line feed in a name becomes a space.
[[nodiscard]] std::string Encode(std::span<const chat::people::FriendRecord> friends);
/// The friends in a file, from the top. A file with a different first line gives none. A line that is not a number
/// greater than 0, a tab and a name is ignored. The list keeps at most the friends that the profile keeps.
[[nodiscard]] std::vector<chat::people::FriendRecord> Decode(std::span<const uint8_t> bytes);

} // namespace openblack::online::friend_list
