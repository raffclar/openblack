/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>
#include <string>

namespace openblack::online
{
class NetworkSession;
}

namespace openblack::ecs::systems
{

/// How the game joins a game server
struct NetworkJoin
{
	/// The game socket of the server: a wss:// address on the desktop, a path in the browser
	std::string url;
	/// The lobby's join ticket
	std::string ticket;
	std::string gamePassword;
	/// Tests on this computer only: a PEM file of more certificate authorities, for a test server on a loopback address.
	/// The join refuses it for any other address.
	std::optional<std::string> testAuthorities;
};

/// The network game of this computer: the session with the game server, which gives the turns of the world. A join
/// makes the game a network game (the clock never stops for one player, and the turns read no part of a frame). The
/// world does no turn that the server did not send.
class NetworkGameSystemInterface
{
public:
	virtual ~NetworkGameSystemInterface() = default;

	/// Joins a game server, after the land of the game loaded. Gives false when the join cannot start.
	virtual bool Join(NetworkJoin join) = 0;
	/// The player leaves the network game
	virtual void Leave() = 0;
	/// Once a frame: moves the session on
	virtual void Update() = 0;
	/// True from the join until the session ends
	[[nodiscard]] virtual bool InGame() const = 0;
	/// The session, while there is one
	[[nodiscard]] virtual const online::NetworkSession* Session() const = 0;
};

} // namespace openblack::ecs::systems
