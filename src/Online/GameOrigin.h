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

#include <string_view>

namespace openblack::online
{

/// Where a network game came from: the lobby on the internet or the game servers on the local network
enum class GameOrigin : uint8_t
{
	/// A game server on the local network, which the player joins with no ticket of the lobby
	Lan,
	/// A game server of the lobby on the internet, which the player joins with a ticket of the lobby
	InternetLobby,
};

/// The origin of a join from the command line: a ticket of the lobby makes it a game of the internet lobby
[[nodiscard]] constexpr GameOrigin OriginOfJoin(std::string_view ticket) noexcept
{
	return ticket.empty() ? GameOrigin::Lan : GameOrigin::InternetLobby;
}

} // namespace openblack::online
