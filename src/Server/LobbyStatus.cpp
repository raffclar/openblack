/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LobbyStatus.h"

#include <charconv>

#include <algorithm>
#include <system_error>

namespace openblack::server::lobby_status
{

Phase PhaseOf(bool started, bool stops) noexcept
{
	if (stops)
	{
		return Phase::Exiting;
	}
	return started ? Phase::Playing : Phase::Staging;
}

std::optional<int64_t> AccountId(std::string_view subject) noexcept
{
	if (subject.empty() || !std::ranges::all_of(subject, [](char c) { return c >= '0' && c <= '9'; }))
	{
		return std::nullopt;
	}
	int64_t value {0};
	const auto [end, error] = std::from_chars(subject.data(), subject.data() + subject.size(), value);
	if (error != std::errc() || end != subject.data() + subject.size() || value <= 0)
	{
		return std::nullopt;
	}
	return value;
}

lobby_api::ServerStatus MakeStatus(const Room& room, Phase phase, std::span<const LockstepHost::Seat> seats)
{
	lobby_api::ServerStatus status {
	    .hostname = room.hostname,
	    .land = room.land,
	    .gamemode = phase == Phase::Exiting ? "exiting" : (phase == Phase::Playing ? "closedplaying" : "openstaging"),
	    .locked = false,
	    .password = false,
	    .maxplayers = std::clamp(room.maxGods, 1, 32),
	    .gamever = room.version.substr(0, std::min(room.version.size(), k_MostVersionCharacters)),
	    .players = {},
	};
	for (const auto& seat : seats)
	{
		if (seat.state != LockstepHost::SeatState::Taken && seat.state != LockstepHost::SeatState::Away)
		{
			continue;
		}
		const auto account = AccountId(seat.account);
		if (!account.has_value())
		{
			continue;
		}
		status.players.push_back({.account = *account, .role = "god", .clan = std::nullopt});
	}
	return status;
}

} // namespace openblack::server::lobby_status
