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
#include <cstdint>

#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "GameSocket.h"

/// The members of a lobby that a game server of the pool waits for. The lobby writes them into a file, and the game
/// server takes only these accounts (deviation 236):
///
///     {"members": [{"account": "101", "role": "god", "slot": 0}, {"account": "205", "role": "observer"}]}
///
/// The account is the subject of the member's join ticket. A god can have a slot of the room (from 0); with no slot, the
/// god takes the first free slot.
namespace openblack::lobby_members
{

/// The most members that a file can name: the god slots of the map for 32 gods and the most observers
inline constexpr size_t k_MostMembers = 64;
/// The most bytes of an account
inline constexpr size_t k_MostAccountBytes = 64;
/// The god slots that a member can name: 0 to 31
inline constexpr uint8_t k_MostSlots = 32;

struct Member
{
	std::string account;
	game_socket::Role role {game_socket::Role::God};
	/// The god slot of the room, from 0. None for an observer, and for a god that takes the first free slot.
	std::optional<uint8_t> slot;

	[[nodiscard]] bool operator==(const Member&) const = default;
};

/// The members of the text, or why the text is not a list of members: not JSON, a field of the wrong type, an empty or
/// a repeated account, a slot of an observer, a slot out of the limits or a slot of two gods, no god, or too many members.
[[nodiscard]] std::variant<std::vector<Member>, std::string> ParseMembers(std::string_view text);

} // namespace openblack::lobby_members
