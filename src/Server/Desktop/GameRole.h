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

#include <memory>
#include <optional>
#include <string>

#include <GameSocket.h>

#include "Server/DedicatedServer.h"
#include "Server/LockstepHost.h"

namespace openblack::server
{

/// The game role of the dedicated server on the desktop: the game socket, the check of the tickets with the keys of
/// the lobby, and the lockstep host
class GameRole
{
public:
	GameRole();
	~GameRole();
	GameRole(const GameRole&) = delete;
	GameRole& operator=(const GameRole&) = delete;
	GameRole(GameRole&&) = delete;
	GameRole& operator=(GameRole&&) = delete;

	/// Gets the keys of the tickets and starts to listen. Gives the error, or none.
	[[nodiscard]] std::optional<std::string> Start(const NetworkSettings& settings, game_socket::Start start);
	/// Once a frame, before the turns: the sockets, the joins, the start and the defeat of the gods that did not come back
	void Update();
	[[nodiscard]] LockstepHost& Host();
	[[nodiscard]] uint16_t Port() const;

	struct Parts;

private:
	std::unique_ptr<Parts> _parts;
};

} // namespace openblack::server
