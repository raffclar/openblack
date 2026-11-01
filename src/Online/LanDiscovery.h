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

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <LobbyApi.h>

/// The search for game servers on the local network, with no lobby (deviation 113). A client sends a search to the UDP
/// port of the search on each computer of the network. Each game server that gets the search sends back its room. The
/// client lists each room in the shape of the lobby's list.
namespace openblack::online::lan_discovery
{

/// The UDP port of the search, the port of the game's own lobby on the local network
constexpr uint16_t k_Port = 2613;
/// The client sends the search again after this time, so that the list follows the rooms
constexpr int64_t k_SearchEveryMicroseconds = 2'000'000;
/// The client removes a room from the list when no answer came for this time
constexpr int64_t k_ForgetAfterMicroseconds = 7'000'000;
/// The most bytes of one search or one answer
constexpr size_t k_MostDatagramBytes = 8192;

/// The bytes of the search
[[nodiscard]] std::span<const uint8_t> Search() noexcept;
[[nodiscard]] bool IsSearch(std::span<const uint8_t> bytes) noexcept;

/// The bytes of the answer of a game server
[[nodiscard]] std::vector<uint8_t> AnswerBytes(const lobby_api::LanAnswer& answer);
/// The answer in the bytes, none when the bytes are not an answer
[[nodiscard]] std::optional<lobby_api::LanAnswer> ParseAnswer(std::span<const uint8_t> bytes);

/// The line of the list for an answer from an IPv4 address. The game socket is on the TCP port of the answer, at the
/// computer name of the answer, else at the address.
[[nodiscard]] lobby_api::ServerEntry EntryOf(const lobby_api::LanAnswer& answer, std::string_view address);

/// The rooms that answered the search, in the order of their first answer
class ServerList
{
public:
	/// An answer came at this time: the line of the room starts or changes
	void Heard(lobby_api::ServerEntry entry, int64_t now);
	/// Removes the rooms that did not answer for some time
	void Forget(int64_t now);
	void Clear() noexcept;
	[[nodiscard]] std::span<const lobby_api::ServerEntry> Servers() const noexcept { return _servers; }

private:
	std::vector<lobby_api::ServerEntry> _servers;
	std::vector<int64_t> _heard;
};

} // namespace openblack::online::lan_discovery
