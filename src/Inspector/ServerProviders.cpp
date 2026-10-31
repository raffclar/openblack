/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ServerProviders.h"

#include <cstdint>

#include <optional>
#include <string>
#include <utility>

namespace
{

using openblack::inspector::Json;

Json OptionalTurn(const std::optional<uint32_t>& turn)
{
	return turn.has_value() ? Json(*turn) : Json(nullptr);
}

Json SeatJson(const openblack::server::admin::SeatReport& seat)
{
	return {
	    {"seat", seat.seat},
	    {"state", std::string(seat.state)},
	    {"name", seat.name},
	    {"account", seat.account},
	    {"ready", seat.ready},
	    {"connected", seat.connected},
	    {"session_number", seat.sessionNumber},
	    {"resumes", seat.resumes},
	    {"commands", seat.commands},
	    {"blocks_again", seat.blocksAgain},
	    {"checksums_compared", seat.checksumsCompared},
	    {"checksums_same", seat.checksumsSame},
	    {"first_different_turn", OptionalTurn(seat.firstDifferentTurn)},
	    {"differs", seat.differs},
	};
}

} // namespace

openblack::inspector::Json openblack::inspector::ToJson(const server::admin::RoomReport& room)
{
	Json seats = Json::array();
	for (const auto& seat : room.seats)
	{
		seats.push_back(SeatJson(seat));
	}
	return {
	    {"starting", room.starting},
	    {"started", room.started},
	    {"paused", room.paused},
	    {"game_over", room.gameOver},
	    {"replaying", room.replaying},
	    {"turns_done", room.turnsDone},
	    {"stopped_after", OptionalTurn(room.stoppedAfter)},
	    {"observers", room.observers},
	    {"refused_blocks", room.refusedBlocks},
	    {"bans", room.bans},
	    {"seats", std::move(seats)},
	};
}

std::unique_ptr<openblack::inspector::ProviderInterface>
openblack::inspector::MakeServerProvider(std::function<server::admin::RoomReport()> room)
{
	auto provider = std::make_unique<FunctionProvider>(std::string(k_ServerProviderName));
	provider->Add(
	    {
	        .name = "room",
	        .description = "The room of the network game of the game server: the start, the turns, the observers, the "
	                       "bans and each god slot with its player and its checksums. It has no secret of a session",
	        .parameters = {},
	        .kind = ResultKind::Object,
	        .needsNear = false,
	        .writes = false,
	    },
	    [room = std::move(room)](const QueryContext& /*context*/) { return QueryResult::Value(ToJson(room())); });
	return provider;
}
