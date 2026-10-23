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

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <GameSocket.h>

#include "Commands/TurnCodec.h"
#include "Commands/TurnCommandSource.h"
#include "Server/TicketCheck.h"
#include "Server/WebSocketServerInterface.h"

namespace openblack::server
{

struct HostSettings
{
	/// The land and the seed that the clients start, which are the server's own
	game_socket::Start start;
	/// The game starts when this many gods joined and are ready
	uint32_t godsToStart {1};
	/// The god slots of the room
	uint8_t godSlots {7};
	/// How long a god's player can be away before the god is defeated (user decision MP-Q6: 5 minutes, set by the server)
	int64_t comeBackMicroseconds {5LL * 60 * 1'000'000};
	/// The turns that the server keeps to send again to a player who comes back: 5 minutes of turns, and some more
	uint32_t keptTurns {3200};
};

/// The game role of the server in a network game (authoritative lockstep, user decision MP-Q1). It lets in the players
/// with a ticket of the lobby, orders their commands into its turns, sends each turn and the checksum of its world after
/// the turn to every client, and compares the clients' checksums with its own. A god's player who loses the socket can
/// come back for a time; then the god is defeated and its slot stays closed (MP-Q5, MP-Q6, MI-9). With no god's player
/// connected, the world does not turn (MP-Q7).
class LockstepHost final: public player_commands::TurnCommandSourceInterface
{
public:
	/// Microseconds of a steady clock of the server
	using Clock = std::function<int64_t()>;
	/// The check of a ticket at this time
	using CheckTicket = std::function<TicketCheck::Result(std::string_view token)>;
	/// A secret for a session: a text that no client can guess
	using MakeSecret = std::function<std::string()>;

	enum class SeatState : uint8_t
	{
		Open,
		/// A connected player has the god
		Taken,
		/// The player of the god lost the socket. The god is idle until the player comes back or the time ends.
		Away,
		/// The god left or was defeated. The slot stays closed.
		Closed,
	};

	struct Compares
	{
		uint32_t compared {0};
		uint32_t same {0};
		std::optional<uint32_t> firstDifferent;
	};

	struct Seat
	{
		SeatState state {SeatState::Open};
		std::optional<ConnectionId> connection;
		std::string session;
		std::string account;
		std::string name;
		bool ready {false};
		int64_t awaySince {0};
		Compares compares;
		uint32_t resumes {0};
		/// The commands of the god that the server put in its turns, by the tag of the command
		std::map<uint8_t, uint32_t> commands;
	};

	LockstepHost(WebSocketServerInterface& sockets, CheckTicket checkTicket, Clock clock, MakeSecret makeSecret,
	             HostSettings settings);
	~LockstepHost() override;
	LockstepHost(const LockstepHost&) = delete;
	LockstepHost& operator=(const LockstepHost&) = delete;
	LockstepHost(LockstepHost&&) = delete;
	LockstepHost& operator=(LockstepHost&&) = delete;

	/// Once a frame, after the poll of the sockets: the start of the game and the end of the time to come back
	void Update();

	/// The commands of the server itself: it has no player
	void Send(const player_commands::Action& action) override;
	/// The world turns when the game started and a god's player is connected
	[[nodiscard]] bool HasTurn(uint32_t turn) const override;
	/// Closes the turn: the blocks that came since the last turn, in the order that they came, go to every client
	[[nodiscard]] std::vector<player_commands::Command> TakeTurn(uint32_t turn) override;
	/// The checksum of the server's world after a turn. It goes to every client.
	void TurnChecksum(uint32_t turn, uint64_t checksum);

	[[nodiscard]] bool Started() const { return _started; }
	/// True when the game started and no god's player is connected
	[[nodiscard]] bool Paused() const;
	/// True when the game started and every god left or was defeated: no player can come back
	[[nodiscard]] bool GameOver() const;
	[[nodiscard]] const std::vector<Seat>& Seats() const { return _seats; }
	[[nodiscard]] size_t Observers() const;
	/// The blocks of commands that the server did not accept: from an observer, before the start, or a command that only
	/// the server sends
	[[nodiscard]] uint32_t RefusedBlocks() const { return _refusedBlocks; }
	/// One line for each god slot that had a player: the checksums compared and the same ones
	[[nodiscard]] std::vector<std::string> SummaryLines() const;

private:
	struct Peer
	{
		bool joined {false};
		game_socket::Role role {game_socket::Role::God};
		std::optional<uint8_t> seat;
	};

	void Opened(ConnectionId connection);
	void Text(ConnectionId connection, std::string_view text);
	void Binary(ConnectionId connection, std::span<const uint8_t> bytes);
	void Closed(ConnectionId connection);
	void Join(ConnectionId connection, Peer& peer, const game_socket::Join& join);
	void Resume(ConnectionId connection, Peer& peer, const game_socket::Resume& resume);
	void Refuse(ConnectionId connection, std::string_view reason);
	void SendTo(ConnectionId connection, const game_socket::Frame& frame);
	void Broadcast(const game_socket::Frame& frame);
	void BroadcastBinary(std::span<const uint8_t> bytes);
	void CloseSeat(uint8_t seat);
	void CompareClient(uint8_t seat, uint32_t turn, uint64_t checksum);
	[[nodiscard]] bool AnyGodConnected() const;

	WebSocketServerInterface& _sockets;
	CheckTicket _checkTicket;
	Clock _clock;
	MakeSecret _makeSecret;
	HostSettings _settings;

	std::vector<Seat> _seats;
	std::map<ConnectionId, Peer> _peers;
	bool _started {false};
	bool _pausedBefore {false};
	uint32_t _turnsDone {0};
	/// The blocks that came since the last turn
	std::vector<turn_codec::StampedBlock> _pending;
	/// The slots whose god leaves at the next turn
	std::vector<uint8_t> _leaving;
	/// The bytes of each turn, to send again to a player who comes back
	std::map<uint32_t, std::vector<uint8_t>> _history;
	/// The server's checksum of each turn
	std::map<uint32_t, uint64_t> _checksums;
	/// The clients' checksums of the turns that the server did not do yet, by seat
	std::map<std::pair<uint8_t, uint32_t>, uint64_t> _early;
	uint32_t _refusedBlocks {0};
};

} // namespace openblack::server
