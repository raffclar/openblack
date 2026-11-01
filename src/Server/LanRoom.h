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

#include <span>
#include <string>

#include <LobbyApi.h>

#include "Server/LobbyStatus.h"
#include "Server/LockstepHost.h"

/// The room of a game server on the local network, as the server gives it to a search (deviation 113)
namespace openblack::server::lan_room
{

/// The answer to a search: the room as the lobby's list shows it, the gods by the names of their players, and the TCP
/// port of the game socket. The host is the computer name of the game socket, empty for the address of the server.
[[nodiscard]] lobby_api::LanAnswer AnswerOf(const lobby_status::Room& room, bool started,
                                            std::span<const LockstepHost::Seat> seats, uint16_t port, const std::string& host);

} // namespace openblack::server::lan_room
