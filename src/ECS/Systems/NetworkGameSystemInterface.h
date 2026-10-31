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

#include <optional>
#include <string>

#include <GameSocket.h>

#include "Online/GameOrigin.h"

namespace openblack::online
{
class NetworkSession;
enum class StartEvent : uint8_t;
} // namespace openblack::online

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
	/// True for a join from the command line: the game loads the server's land as the server lets the player in (see
	/// TakeStart), then the player is ready and the world waits for the server's start. False for a join from the
	/// multiplayer front end: the player waits in the game room, and the game loads the server's land after the start.
	bool fromCommandLine {true};
	/// Tests: at this turn the world draws one synced random number more, so that it parts from the server's world
	std::optional<uint32_t> partAt;
	/// Tests: the world parts again after each load of the server's world
	bool partAgain {false};
	/// Tests: the creature file that the player sends before the start, in place of the creature of the player's
	/// profile
	std::optional<std::string> testCreatureFile;
	/// Where the game came from: the lobby on the internet or the local network
	online::GameOrigin origin {online::GameOrigin::Lan};
};

/// The network game of this computer: the session with the game server, which gives the turns of the world. A join
/// makes the game a network game (the clock never stops for one player, and the turns read no part of a frame). The
/// world does no turn that the server did not send.
///
/// A join from the multiplayer front end has two parts. In the game room the session only joins and sends the player's
/// Ready, and the land on this computer plays on as before. When the server starts the game, the game takes the start
/// (TakeStart), loads the server's land with the server's seed, and then calls BeginPlay: from then on the world does
/// only the server's turns.
///
/// A join from the command line gets the server's land, seed and start in the server's reply to the join. The game takes
/// them (TakeStart), loads that land with that seed, and calls BeginPlay. The player is then ready, and the world waits,
/// paused, for the server's start. Thus each computer starts the game from the server's world.
class NetworkGameSystemInterface
{
public:
	virtual ~NetworkGameSystemInterface() = default;

	/// Joins a game server, after the land of the game loaded. Gives false when the join cannot start.
	virtual bool Join(NetworkJoin join) = 0;
	/// The player is ready for the start, or cancels the ready, in the game room
	virtual void SetReady(bool ready) = 0;
	/// The god of the player plays for a clan in the game room, or for no clan. The start data of the god tells it to
	/// the game, so that the creature of a clan god does not go to the files of the player's profile.
	virtual void SetPlaysForClan(bool /*playsForClan*/) {}
	/// The player leaves the network game
	virtual void Leave() = 0;
	/// The end box of a decided game opened: the game server stops the turns of the game. When the last turn came, the
	/// clock of this computer stops, as a pause stops it.
	virtual void StopGame() {}
	/// The land, the seed and the start of the server's world, one time: for a join from the front end when the server
	/// starts the game, and for a join from the command line when the server lets the player in. None before that.
	[[nodiscard]] virtual std::optional<game_socket::Start> TakeStart() = 0;
	/// After the game loaded the land of the start: the world does the server's turns from now on. For a join from the
	/// command line, the player is ready and the world waits for the server's start.
	virtual void BeginPlay() = 0;
	/// After the session ended: the world of this computer turns alone from now on, with no server
	virtual void PlayAlone() = 0;
	/// After the player left: the session goes, and the next land that loads is a game of this computer alone, not a
	/// network game
	virtual void EndNetworkGame() {}
	/// Once a frame: moves the session on
	virtual void Update() = 0;
	/// True from the join until the session ends
	[[nodiscard]] virtual bool InGame() const = 0;
	/// The session, while there is one
	[[nodiscard]] virtual const online::NetworkSession* Session() const = 0;
	/// The next step of the start of the game, for the front end to show, one time each
	[[nodiscard]] virtual std::optional<online::StartEvent> TakeStartEvent() = 0;
};

} // namespace openblack::ecs::systems
