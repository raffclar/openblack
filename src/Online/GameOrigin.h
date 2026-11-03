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

#include <algorithm>
#include <optional>
#include <string>
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

/// The ticket in the text of a ticket file (the option --join-ticket-file): the text with the spaces and the line ends
/// before and after it removed. None when no text stays, or when a space or a control character stays in it.
[[nodiscard]] inline std::optional<std::string> TicketFileText(std::string_view text)
{
	constexpr std::string_view k_Around = " \t\r\n";
	const auto first = text.find_first_not_of(k_Around);
	if (first == std::string_view::npos)
	{
		return std::nullopt;
	}
	const auto ticket = text.substr(first, text.find_last_not_of(k_Around) - first + 1);
	if (std::ranges::any_of(ticket, [](char c) { return static_cast<unsigned char>(c) <= ' ' || c == 0x7F; }))
	{
		return std::nullopt;
	}
	return std::string(ticket);
}

} // namespace openblack::online
