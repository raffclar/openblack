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

#include <GameSocket.h>

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
	/// The name of the player's profile, on the local network only, where no ticket names the player
	std::string name;
	/// Tests on this computer only: a PEM file of more certificate authorities, for a test server on a loopback address.
	/// The join refuses it for any other address.
	std::optional<std::string> testAuthorities;
	/// True when the land of the game loaded before the join, as for a join from the command line: the player is ready
	/// at once and the world waits for the server's start. False for a join from the multiplayer front end: the player
	/// waits in the game room, and the game loads the server's land after the start (see TakeStart).
	bool landLoaded {true};
};

/// The network game of this computer: the session with the game server, which gives the turns of the world. A join
/// makes the game a network game (the clock never stops for one player, and the turns read no part of a frame). The
/// world does no turn that the server did not send.
///
/// A join from the multiplayer front end has two parts. In the game room the session only joins and sends the player's
/// Ready, and the land on this computer plays on as before. When the server starts the game, the game takes the start
/// (TakeStart), loads the server's land with the server's seed, and then calls BeginPlay: from then on the world does
/// only the server's turns.
class NetworkGameSystemInterface
{
public:
	virtual ~NetworkGameSystemInterface() = default;

	/// Joins a game server, after the land of the game loaded. Gives false when the join cannot start.
	virtual bool Join(NetworkJoin join) = 0;
	/// The player is ready for the start, or cancels the ready, in the game room
	virtual void SetReady(bool ready) = 0;
	/// The player leaves the network game
	virtual void Leave() = 0;
	/// The start of the game that the server sent to a join from the front end, one time. None for a join from the
	/// command line and before the start.
	[[nodiscard]] virtual std::optional<game_socket::Start> TakeStart() = 0;
	/// After the game loaded the land of the start: the world does the server's turns from now on
	virtual void BeginPlay() = 0;
	/// Once a frame: moves the session on
	virtual void Update() = 0;
	/// True from the join until the session ends
	[[nodiscard]] virtual bool InGame() const = 0;
	/// The session, while there is one
	[[nodiscard]] virtual const online::NetworkSession* Session() const = 0;
};

} // namespace openblack::ecs::systems
