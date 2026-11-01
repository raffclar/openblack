/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LanRoom.h"

#include <algorithm>

namespace openblack::server::lan_room
{

lobby_api::LanAnswer AnswerOf(const lobby_status::Room& room, bool started, std::span<const LockstepHost::Seat> seats,
                              uint16_t port, const std::string& host)
{
	lobby_api::LanAnswer answer {
	    .hostname = room.hostname,
	    .land = room.land,
	    .gamemode = started ? "closedplaying" : "openstaging",
	    .password = false,
	    .numplayers = 0,
	    .maxplayers = std::clamp(room.maxGods, 1, 32),
	    .gamever = room.version.substr(0, std::min(room.version.size(), lobby_status::k_MostVersionCharacters)),
	    .port = port,
	    .host = host,
	    .players = {},
	};
	for (const auto& seat : seats)
	{
		if (seat.state != LockstepHost::SeatState::Taken && seat.state != LockstepHost::SeatState::Away)
		{
			continue;
		}
		answer.players.push_back({.account = {.id = 0, .name = seat.name}, .role = "god", .clan = std::nullopt});
	}
	answer.numplayers = static_cast<int32_t>(answer.players.size());
	return answer;
}

} // namespace openblack::server::lan_room
