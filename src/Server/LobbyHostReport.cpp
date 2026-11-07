/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LobbyHostReport.h"

#include <algorithm>

#include <LobbyApiJson.h>

#include "PlayerSlots.h"
#include "Server/LobbyStatus.h"

namespace openblack::server::lobby_host_report
{

namespace host_report = gathering_box::host_report;

std::string HostReportAddress(std::string_view lobbyApi, std::string_view serverId)
{
	return std::string(lobbyApi) + "/servers/" + std::string(serverId) + "/host-report";
}

host_report::TeamTable TeamsOf(std::span<const LockstepHost::Seat> seats, std::span<const lobby_api::ServerStatusClan> clans)
{
	host_report::TeamTable teams {};
	for (size_t slot = 0; slot < seats.size(); ++slot)
	{
		const auto& seat = seats[slot];
		const auto player = player_slots::PlayerOfSlot(slot);
		const auto account = seat.state == LockstepHost::SeatState::Open ? std::nullopt : lobby_status::AccountId(seat.account);
		if (!player.has_value() || !account.has_value() || static_cast<size_t>(*player) >= host_report::k_TeamCount)
		{
			continue;
		}
		const auto clan = std::ranges::find(clans, *account, &lobby_api::ServerStatusClan::account);
		teams.at(static_cast<size_t>(*player)).front() = {
		    .number = static_cast<uint32_t>(*account),
		    .clan = clan != clans.end() ? static_cast<uint32_t>(clan->clan) : 0,
		};
	}
	return teams;
}

std::array<std::optional<host_report::Condition>, host_report::k_ConditionCount>
ConditionsOf(std::span<const game_end::ConditionPick> picks, const ReportedStandings& standings)
{
	std::array<std::optional<host_report::Condition>, host_report::k_ConditionCount> conditions {};
	for (const auto& pick : picks)
	{
		const auto index = static_cast<size_t>(pick.condition);
		if (index >= conditions.size())
		{
			continue;
		}
		host_report::Condition condition {.aim = pick.value};
		for (size_t player = 0; player < standings.size(); ++player)
		{
			if (!standings.at(player).has_value())
			{
				continue;
			}
			if (const auto& each = standings.at(player)->conditions.at(index); each.has_value())
			{
				condition.players.at(player) = {.current = each->current, .met = each->completed};
			}
		}
		conditions.at(index) = condition;
	}
	return conditions;
}

std::string BodyOf(std::span<const uint8_t> report)
{
	return R"({"report":")" + lobby_api::Base64Encode(report) + R"("})";
}

} // namespace openblack::server::lobby_host_report
