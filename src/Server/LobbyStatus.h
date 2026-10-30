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

#include <chrono>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <LobbyApi.h>

#include "Server/LockstepHost.h"

/// The status report of a game server to the lobby. The lobby lists a server and gives tickets for it only while the
/// server reports its room every 5 seconds.
namespace openblack::server::lobby_status
{

/// The time between two reports. The lobby removes a server from its list after 15 seconds with no report.
constexpr auto k_ReportInterval = std::chrono::seconds(5);
/// The longest version text that the lobby keeps
constexpr size_t k_MostVersionCharacters = 16;

/// What the room shows in the list of the lobby
struct Room
{
	/// The name of the room
	std::string hostname {"Game"};
	/// The land of the game
	std::string land;
	/// The gods that the room takes, 1 to 32
	int32_t maxGods {1};
	/// The version of the game server
	std::string version;
};

/// The phase of the room for the lobby
enum class Phase : uint8_t
{
	/// The room takes players before the game starts
	Staging,
	/// The game started: the lobby gives no more tickets
	Playing,
	/// The server stops: the lobby removes it from the list at once
	Exiting,
};

/// The phase that the lobby gets for a host that started or not, or that stops
[[nodiscard]] Phase PhaseOf(bool started, bool stops) noexcept;

/// The account id of a ticket's subject. None when the subject is not a decimal number.
[[nodiscard]] std::optional<int64_t> AccountId(std::string_view subject) noexcept;

/// The report for the room: each seat that a god's player has (connected or away) is a god of the room. A seat with a
/// subject that is not an account id is not in the report.
[[nodiscard]] lobby_api::ServerStatus MakeStatus(const Room& room, Phase phase, std::span<const LockstepHost::Seat> seats);

/// A land of a network game that the players do not have, which the server sends to the lobby (deviation 169). The
/// lobby gives it only to the players with a ticket for the room of the server.
struct CustomMap
{
	/// The name of the file with no folder and no extension, for example "obm_32p_1"
	std::string file;
	/// The name of the land in the list of the lands
	std::string name;
	/// The gods that the land takes
	int32_t players {0};
	/// The bytes of the ".map" file
	std::vector<uint8_t> bytes;
};

/// The time before the server sends the map again after the lobby did not take it
constexpr auto k_MapAgainAfter = std::chrono::seconds(30);

/// The address of the call that sends the map of the room to the lobby: PUT <api>/servers/<id>/map with the name of
/// the file, the name of the land and its gods in the query. The text of each value is percent-encoded.
[[nodiscard]] std::string MapAddress(std::string_view api, std::string_view serverId, const CustomMap& map);

} // namespace openblack::server::lobby_status
