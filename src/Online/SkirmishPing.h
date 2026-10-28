/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>

namespace openblack::ecs::systems
{
class LobbyClientSystemInterface;
}

// When a skirmish is decided, the game tells the lobby of the player, one time for each event that decides it. The
// message holds no data: the session of the player tells the lobby which account sends it.
//
// The game sends the message only when it has the address of a lobby and the player has a session. A session that
// the store keeps from an earlier run continues first. With no lobby, no session or no network, the game sends nothing
// and shows nothing. The message goes over HTTPS with the web client of the lobby, so no frame and no turn waits.
//
// The message is not part of the world: it changes nothing that the turns share.

namespace openblack::online
{

/// Tells the lobby that a skirmish is decided, a number of times
void SendSkirmishPings(ecs::systems::LobbyClientSystemInterface& lobby, size_t count);

} // namespace openblack::online
