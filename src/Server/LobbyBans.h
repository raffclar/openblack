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

#include <functional>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Online/HttpClientInterface.h"
#include "Server/LockstepHost.h"

/// The bans of a game server. The lobby keeps the bans of each server. The game server reads them with each status
/// report and refuses a banned god before the start (deviation 146).
namespace openblack::server::lobby_bans
{

/// The address of the bans of a server: the address of the lobby's API, then "/servers/<id>/bans"
[[nodiscard]] std::string BansAddress(std::string_view lobbyApi, std::string_view serverId);

/// The account ids in the answer of the lobby. None when the status is not 200 or the body is not a list of bans.
[[nodiscard]] std::optional<std::set<int64_t>> BannedAccounts(const online::HttpResponse& response);

/// True when the subject of a ticket is an account id that the lobby banned
[[nodiscard]] bool IsBanned(std::string_view subject, const std::set<int64_t>& banned) noexcept;

/// The god slots that a ban takes out of the room: each slot that a player has with a banned account
[[nodiscard]] std::vector<uint8_t> BannedSeats(std::span<const LockstepHost::Seat> seats, const std::set<int64_t>& banned);

/// Gets the bans of the server from the lobby, one request at a time
class Reader
{
public:
	/// Gets the account ids each time that the lobby answers with a list of bans
	using Listener = std::function<void(std::set<int64_t> banned)>;

	Reader(online::HttpClientInterface& lobby, std::string address, Listener listener);

	/// Asks the lobby for the bans, unless the last request did not come back yet. A failed request keeps the last bans.
	void Fetch();
	[[nodiscard]] bool Fetching() const { return _fetching; }

private:
	online::HttpClientInterface& _lobby;
	std::string _address;
	Listener _listener;
	bool _fetching {false};
	/// The last request failed: the server writes a line to the log only when this changes
	bool _failed {false};
};

} // namespace openblack::server::lobby_bans
