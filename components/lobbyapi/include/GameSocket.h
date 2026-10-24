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
#include <string_view>
#include <variant>

/// The JSON text frames of the game socket between a client and a game server, after the socket opens and next to the
/// binary frames of the turns (see the turn codec of the game). Each frame is an object with a "type" member. Nothing
/// here throws. A parse gives none when the text is not a frame that it knows.
namespace openblack::game_socket
{

/// What a ticket lets the account do in the room
enum class Role : uint8_t
{
	/// The account takes a god slot and plays
	God,
	/// The account only watches. It has no god slot and sends no commands.
	Observer,
};

/// Client to server: "room.join". The first frame on a new socket.
struct Join
{
	/// The lobby's join ticket (a PASETO v4.public token)
	std::string ticket;
	/// The game's password, when the game needs one
	std::string gamePassword;
};

/// Client to server: "room.resume". A client that lost its socket opens a new one and sends this in place of a join. It
/// keeps its world, so the server sends the turns after the last turn that the client did.
struct Resume
{
	/// The secret of the session that the server gave in "room.joined"
	std::string session;
	/// The last turn that the client did
	uint32_t turn {0};
	/// The world of the client parted from the server's: the server sends its world state first ("world", optional)
	bool world {false};
};

/// Client to server: "room.ready". The client loaded the land and waits for the start of the game.
struct Ready
{
	bool ready {true};
};

/// Client to server: "room.leave". The player leaves the game. The server then defeats the god.
struct Leave
{
};

/// Server to client: "room.joined". The server accepted the ticket.
struct Joined
{
	Role role {Role::God};
	/// The god slot of the player, from 0. None for an observer.
	std::optional<uint8_t> slot;
	/// The secret that the client sends in a resume
	std::string session;
	/// The name of the account
	std::string name;
};

/// Server to client: "room.refused". The server closes the socket after this frame.
struct Refused
{
	/// A short word for the reason: "ticket", "full", "started", "closed", "protocol", "resync"
	std::string reason;
};

/// Server to client: "game.start". The world starts to turn from this turn.
struct Start
{
	/// The script of the land under the Scripts folder of the game, or none for the testbed
	std::string land;
	bool testbed {false};
	uint32_t seed {0};
	/// The turn that the world is at when the game starts
	uint32_t turn {0};
};

/// Server to client: "room.resumed". The server accepted the resume. The turns after the client's turn follow.
struct Resumed
{
	/// The last turn that the server did
	uint32_t turn {0};
	/// The server's world state at the start of that turn follows as world parts, before the turns ("world", optional)
	bool world {false};
};

/// Server to client: "player.state". A god's player lost the socket, came back, or left the game.
struct PlayerState
{
	enum class State : uint8_t
	{
		/// The socket closed. The god stays idle until the player comes back or the time to come back ends.
		Away,
		Back,
		/// The god is defeated and its slot is closed
		Left,
	};
	uint8_t slot {0};
	State state {State::Away};
};

using Frame = std::variant<Join, Resume, Ready, Leave, Joined, Refused, Start, Resumed, PlayerState>;

[[nodiscard]] std::string ToJson(const Frame& frame);
[[nodiscard]] std::optional<Frame> Parse(std::string_view text);

[[nodiscard]] std::string_view RoleName(Role role) noexcept;
[[nodiscard]] std::optional<Role> RoleFrom(std::string_view name) noexcept;

} // namespace openblack::game_socket
