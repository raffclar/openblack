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

#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include <LobbyApi.h>

#include "ECS/Components/GameStanding.h"
#include "ECS/GameEndConditions.h"
#include "ECS/GatheringBoxRules.h"
#include "Server/LockstepHost.h"

/// The report of the host of an internet game to the lobby. The game server is the host. While the game runs, it sends
/// the members of the god teams and the state of the winning conditions each 20 seconds, and one time when the game is
/// decided. The lobby keeps the last report.
namespace openblack::server::lobby_host_report
{

/// The address of the report of a server: the address of the lobby's API, then "/servers/<id>/host-report"
[[nodiscard]] std::string HostReportAddress(std::string_view lobbyApi, std::string_view serverId);

/// The god teams of a network game with one human for each god. The god of the first slot is the first team, and the
/// human of the god is the first member of the team. A slot with no player, or with a subject that is not an account
/// id, is an empty team. The clan of a team is the clan that the lobby gave the account.
[[nodiscard]] gathering_box::host_report::TeamTable TeamsOf(std::span<const LockstepHost::Seat> seats,
                                                            std::span<const lobby_api::ServerStatusClan> clans);

/// The standings of the first four players, by player number. None for a player with no standing.
using ReportedStandings =
    std::array<std::optional<ecs::components::GameStanding>, gathering_box::host_report::k_ReportedPlayers>;

/// The conditions of the report: each winning condition of the game with its value as the aim, and the state of the
/// condition for each of the first four players. A player that does not have the condition reports 0.
[[nodiscard]] std::array<std::optional<gathering_box::host_report::Condition>, gathering_box::host_report::k_ConditionCount>
ConditionsOf(std::span<const game_end::ConditionPick> picks, const ReportedStandings& standings);

/// The body of the request: {"report": "<the bytes of the report in base64>"}
[[nodiscard]] std::string BodyOf(std::span<const uint8_t> report);

} // namespace openblack::server::lobby_host_report
