/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LobbyBans.h"

#include <utility>

#include <LobbyApiJson.h>
#include <spdlog/spdlog.h>

#include "Server/LobbyStatus.h"

namespace openblack::server::lobby_bans
{

std::string BansAddress(std::string_view lobbyApi, std::string_view serverId)
{
	while (lobbyApi.ends_with('/'))
	{
		lobbyApi.remove_suffix(1);
	}
	return std::string(lobbyApi) + "/servers/" + std::string(serverId) + "/bans";
}

std::optional<std::set<int64_t>> BannedAccounts(const online::HttpResponse& response)
{
	if (response.error != online::HttpError::None || response.status != 200)
	{
		return std::nullopt;
	}
	const auto list = lobby_api::ParseServerBanList(response.Text());
	if (!list.has_value())
	{
		return std::nullopt;
	}
	std::set<int64_t> banned;
	for (const auto& ban : list->bans)
	{
		banned.insert(ban.account.id);
	}
	return banned;
}

bool IsBanned(std::string_view subject, const std::set<int64_t>& banned) noexcept
{
	const auto account = lobby_status::AccountId(subject);
	return account.has_value() && banned.contains(*account);
}

std::vector<uint8_t> BannedSeats(std::span<const LockstepHost::Seat> seats, const std::set<int64_t>& banned)
{
	std::vector<uint8_t> found;
	for (size_t i = 0; i < seats.size(); ++i)
	{
		const auto& seat = seats[i];
		const bool held = seat.state == LockstepHost::SeatState::Taken || seat.state == LockstepHost::SeatState::Away;
		if (held && IsBanned(seat.account, banned))
		{
			found.push_back(static_cast<uint8_t>(i));
		}
	}
	return found;
}

Reader::Reader(online::HttpClientInterface& lobby, std::string address, Listener listener)
    : _lobby(lobby)
    , _address(std::move(address))
    , _listener(std::move(listener))
{
}

void Reader::Fetch()
{
	if (_fetching)
	{
		return;
	}
	_fetching = true;
	_lobby.Send(online::HttpRequest {.method = online::HttpMethod::Get, .url = _address, .headers = {}, .body = {}},
	            [this](online::HttpResponse&& response) {
		            _fetching = false;
		            auto banned = BannedAccounts(response);
		            const bool failed = !banned.has_value();
		            if (failed != _failed)
		            {
			            if (auto logger = spdlog::get("game"); logger && failed)
			            {
				            logger->warn("The bans of the server did not come from the lobby (status {}): the server keeps the "
				                         "last bans",
				                         response.status);
			            }
			            else if (logger)
			            {
				            logger->info("The bans of the server come from the lobby again");
			            }
		            }
		            _failed = failed;
		            if (banned.has_value() && _listener)
		            {
			            _listener(std::move(*banned));
		            }
	            });
}

} // namespace openblack::server::lobby_bans
