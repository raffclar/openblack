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
#include <cstdint>

#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

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
	/// The name of the player's profile, on the local network only, where no ticket names the player
	std::string name;
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

/// Server to client: "game.start". The world starts to turn from this turn.
struct Start
{
	/// The script of the land, as a path under the Scripts folder of the game (for example "Land1.txt" or
	/// "Playgrounds/construct.txt"). None for the testbed and the network map for 32 gods.
	std::string land;
	bool testbed {false};
	uint32_t seed {0};
	/// The turn that the world is at when the game starts
	uint32_t turn {0};
	/// How the new game on the land starts, as the game's --skip-opening gives it (for example "cow"). Empty for the
	/// testbed.
	std::string opening;
	/// The network map for 32 gods in place of a land ("gods_ring_map", optional)
	bool godsRingMap {false};
	/// Tools: every god with a citadel is a computer god, and the room has no human god ("all_computer_gods", optional).
	/// Only a developer build plays such a game.
	bool allComputerGods {false};
	/// Tools: the seed of the personalities of the computer gods ("ai_seed", optional). None: the built-in personalities.
	std::optional<uint32_t> aiSeed;
	/// Tools: the id of the testbed scenario that every computer plays from the first turn ("scenario", optional). Only a
	/// developer build plays such a game.
	std::optional<std::string> scenario;
	/// The names of the players of the gods, by god slot ("names", optional). A slot with no player has an empty name.
	std::vector<std::string> names;
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
	/// The land, the seed and the start of the server's world ("start", optional). A client that joins from the command
	/// line loads this land with this seed before it is ready, so the game starts from the same world on each computer.
	std::optional<Start> start;
};

/// Server to client: "room.refused". The server closes the socket after this frame.
struct Refused
{
	/// A short word for the reason: "ticket", "full", "started", "closed", "protocol", "resync", "kicked"
	std::string reason;
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

/// The start of the game has steps. When the gods are ready, the server asks each player in the room if it has the
/// land (start.query). Each player replies with the checksum of its land file (start.reply). When a player does not
/// have the land, the server stops the start (start.cancel). When a checksum is not the server's, the server also
/// removes that player from the room (room.refused "kicked"). When all players have the land, the server asks the gods
/// for their creatures (start.transfer). Each god sends its creature file (start.creature), and the server sends each
/// creature file to all players. When a player leaves during these steps, the server stops the start (start.stop).
/// After the last creature file, the server sends game.start.

/// Server to client: "start.query". The gods are ready, and the server asks if the player has the land of the game.
struct StartQuery
{
	/// The script of the land under the Scripts folder of the game, or none for the testbed
	std::string land;
	bool testbed {false};
	/// The checksum of the server's land file
	uint64_t checksum {0};
};

/// Client to server: "start.reply"
struct StartReply
{
	/// The player has the land
	bool have {false};
	/// The checksum of the player's land file
	uint64_t checksum {0};
};

/// Server to client: "start.cancel". A player does not have the land, so the game does not start.
struct StartCancel
{
};

/// Server to client: "start.stop". A player left the room during the start, so the game does not start.
struct StartStop
{
};

/// Server to client: "start.transfer". All players have the land. Each god sends its creature file.
struct StartTransfer
{
};

/// The largest creature file that a start.creature frame holds
inline constexpr size_t k_MostCreatureFileBytes = 512 * 1024;

/// "start.creature". Client to server: the creature file of the player's god, empty when the player has no creature.
/// Server to client: the creature file of the god of a slot.
struct StartCreature
{
	/// The god's slot. None from a client.
	std::optional<uint8_t> slot;
	std::vector<uint8_t> file;
	/// The alignment that the god starts the game at: the alignment of its player's profile, from -1 (evil) to 1 (good)
	float alignment {0.0f};
};

/// Client to server: "game.stop_request". The end box of a decided game opened on the client. The server stops the turns
/// of the game, if they did not stop before.
struct StopRequest
{
};

/// Server to client: "game.stop". The server stopped the turns of the game. No turn comes after this turn.
struct GameStop
{
	uint32_t turn {0};
};

using Frame = std::variant<Join, Resume, Ready, Leave, Joined, Refused, Start, Resumed, PlayerState, StartQuery, StartReply,
                           StartCancel, StartStop, StartTransfer, StartCreature, StopRequest, GameStop>;

[[nodiscard]] std::string ToJson(const Frame& frame);
[[nodiscard]] std::optional<Frame> Parse(std::string_view text);

[[nodiscard]] std::string_view RoleName(Role role) noexcept;
[[nodiscard]] std::optional<Role> RoleFrom(std::string_view name) noexcept;

} // namespace openblack::game_socket
