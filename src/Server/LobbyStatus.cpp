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

#include <LobbyApiJson.h>

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

std::vector<lobby_api::ServerStatusPlayer> StatusPlayers(std::span<const LockstepHost::Seat> seats)
{
	std::vector<lobby_api::ServerStatusPlayer> players;
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
		players.push_back(
		    {.account = *account, .role = "god", .clan = std::nullopt, .away = seat.state == LockstepHost::SeatState::Away});
	}
	return players;
}

lobby_api::ServerStatus MakeStatus(const Room& room, Phase phase, std::span<const LockstepHost::Seat> seats,
                                   size_t mostVersionCharacters)
{
	return {
	    .hostname = room.hostname,
	    .land = room.land,
	    .gamemode = phase == Phase::Exiting ? "exiting" : (phase == Phase::Playing ? "closedplaying" : "openstaging"),
	    .locked = false,
	    .password = false,
	    .maxplayers = std::clamp(room.maxGods, 1, 32),
	    .gamever = room.version.substr(0, std::min(room.version.size(), mostVersionCharacters)),
	    .players = StatusPlayers(seats),
	};
}

bool RefusedTheVersionLength(int32_t status, std::string_view body)
{
	// The lobby answers 422 with the fields that it did not take. The server cuts the build id only when the build id is
	// longer than an older lobby keeps.
	constexpr int32_t k_FieldsNotValid = 422;
	if (status != k_FieldsNotValid)
	{
		return false;
	}
	const auto problem = lobby_api::ParseProblem(body);
	return problem.has_value() &&
	       std::ranges::find(problem->errorFields, std::string_view("gamever")) != problem->errorFields.end();
}

std::string MapAddress(std::string_view api, std::string_view serverId, const CustomMap& map)
{
	using lobby_api::PercentEncoded;
	std::string base(api);
	while (base.ends_with('/'))
	{
		base.pop_back();
	}
	auto address = base + "/servers/" + PercentEncoded(serverId) + "/map?file=" + PercentEncoded(map.file) +
	               "&name=" + PercentEncoded(map.name) + "&players=" + std::to_string(map.players);
	if (!map.packageSha256.empty())
	{
		address += "&package=" + PercentEncoded(map.packageSha256) + "&package_size=" + std::to_string(map.packageSize);
	}
	return address;
}

} // namespace openblack::server::lobby_status
