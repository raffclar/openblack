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
#include <string>
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

namespace
{
/// The text with each byte that is not a letter, a digit or one of "-._~" as "%" and two hex digits
std::string PercentEncoded(std::string_view text)
{
	constexpr std::string_view k_Hex = "0123456789ABCDEF";
	std::string out;
	for (const char c : text)
	{
		const auto u = static_cast<unsigned char>(c);
		const bool plain = (u >= 'a' && u <= 'z') || (u >= 'A' && u <= 'Z') || (u >= '0' && u <= '9') || u == '-' || u == '.' ||
		                   u == '_' || u == '~';
		if (plain)
		{
			out += c;
		}
		else
		{
			out += '%';
			out += k_Hex[u >> 4];
			out += k_Hex[u & 0xF];
		}
	}
	return out;
}
} // namespace

std::string MapAddress(std::string_view api, std::string_view serverId, const CustomMap& map)
{
	std::string base(api);
	while (base.ends_with('/'))
	{
		base.pop_back();
	}
	return base + "/servers/" + PercentEncoded(serverId) + "/map?file=" + PercentEncoded(map.file) +
	       "&name=" + PercentEncoded(map.name) + "&players=" + std::to_string(map.players);
}

} // namespace openblack::server::lobby_status
