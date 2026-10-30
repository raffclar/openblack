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

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <GameSocket.h>

#include "Commands/TurnCodec.h"
#include "Commands/TurnCommandSource.h"
#include "Server/ServerSnapshots.h"
#include "Server/SessionHash.h"
#include "Server/TicketCheck.h"
#include "Server/WebSocketServerInterface.h"

namespace openblack::server
{

/// The god slots of a room when the land gives none, such as the testbed: each god of a network land
constexpr uint8_t k_DefaultGodSlots = 7;

/// The god slots of the room of a land. Each slot up to the last one with a citadel can have a player, so that each
/// citadel can have a human. A land with no citadel gives the default slots.
[[nodiscard]] uint8_t GodSlotsOfLand(std::span<const bool> slotHasCitadel) noexcept;

/// The rule of the start of the room, as the server presses the host's Start: each god in the room is ready, and there
/// are at least the fewest gods of the room
[[nodiscard]] bool RoomStarts(size_t godsInRoom, size_t readyGods, uint32_t minGods) noexcept;

struct HostSettings
{
	/// The land and the seed that the clients start, which are the server's own
	game_socket::Start start;
	/// The checksum of the server's land file, which each player's land file must have before the start
	uint64_t landChecksum {0};
	/// The fewest gods with a player that start the game. The start comes when each god in the room is ready. In a room
	/// with no human god (tools), the world turns when this many observers joined.
	uint32_t minGods {1};
	/// The god slots of the room: the citadels of the land. A god who joins when all the slots have a player is refused.
	uint8_t godSlots {k_DefaultGodSlots};
	/// How long a god's player can be away before the god is defeated (user decision MP-Q6: 5 minutes, set by the server)
	int64_t comeBackMicroseconds {5LL * 60 * 1'000'000};
	/// The turns that the server keeps to send again to a player who comes back: 5 minutes of turns, and some more
	uint32_t keptTurns {3200};
	/// Tests: the commands of the test tools (player_commands::CreatureFightCheat) go into the turns. Without this, the
	/// server leaves them out.
	bool testCommands {false};
	/// Makes the salted hashes of the secrets of sessions, which the snapshots keep in place of the secrets. With none, a
	/// restored room has no sessions, and no player can come back with a secret.
	std::shared_ptr<const SessionHasherInterface> sessionHasher;
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
		/// The last checksum that the server compared differed from the server's
		bool differs {false};
	};

	struct Seat
	{
		SeatState state {SeatState::Open};
		std::optional<ConnectionId> connection;
		/// The secret of the session. After a restore, it is empty until the player comes back with the secret.
		std::string session;
		/// The salted hash of the secret of the session
		std::optional<SessionHash> sessionHash;
		std::string account;
		std::string name;
		bool ready {false};
		int64_t awaySince {0};
		Compares compares;
		uint32_t resumes {0};
		/// The commands of the god that the server put in its turns, by the tag of the command
		std::map<uint8_t, uint32_t> commands;
	};

	/// The session of an observer. An observer has no god, but it can come back with the secret of its session, as the
	/// player of a god can. Observers are an addition of openblack to the game.
	struct Watcher
	{
		std::string session;
		std::optional<SessionHash> sessionHash;
		/// None while the observer is away
		std::optional<ConnectionId> connection;
		int64_t awaySince {0};
		uint32_t resumes {0};
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

	/// The server's world state at the start of the next turn, which a client whose world parted loads as it comes back
	using WorldState = std::function<std::vector<uint8_t>()>;
	void SetWorldState(WorldState worldState) { _worldState = std::move(worldState); }
	/// Gets the bytes of each turn that the server closes, before the server sends them. The snapshots of the server keep
	/// them in a journal (deviation 100).
	using TurnJournal = std::function<void(uint32_t turn, std::span<const uint8_t> bytes)>;
	void SetTurnJournal(TurnJournal journal) { _turnJournal = std::move(journal); }
	/// The room of the game, as a snapshot of the server keeps it
	[[nodiscard]] snapshots::Room RoomRecord() const;
	/// Continues a game from a snapshot, before any client connects. The world of the server is at the start of the turn
	/// turnsDone. The gods of the room wait for their players, who come back with the secrets of their sessions. The
	/// world turns the replayed turns first, with no player: these are the turns that the server sent before it stopped.
	void Restore(const snapshots::Room& room, uint32_t turnsDone, std::map<uint32_t, std::vector<uint8_t>> replayed);
	/// True while the world must turn the replayed turns of a restored game. The server reads no socket until then.
	[[nodiscard]] bool Replaying() const { return !_replayed.empty(); }
	/// The next turn that the server closes
	[[nodiscard]] uint32_t TurnsDone() const { return _turnsDone; }
	/// Tools: gets the god slot and the turn each time that the world of a client starts to differ from the server's world
	using PartListener = std::function<void(uint8_t seat, uint32_t turn)>;
	void SetPartListener(PartListener listener) { _partListener = std::move(listener); }
	/// The account ids that the lobby banned from the server. Before the start, a banned god leaves the room at once with
	/// "kicked", and the room refuses the join of a banned god. After the start, a ban changes nothing (deviation 146).
	void SetBans(std::set<int64_t> accounts);
	[[nodiscard]] const std::set<int64_t>& Bans() const { return _bans; }

	/// The commands of the server itself: it has no player
	void Send(const player_commands::Action& action) override;
	/// The world turns when the game started and a god's player is connected
	[[nodiscard]] bool HasTurn(uint32_t turn) const override;
	/// Closes the turn: the blocks that came since the last turn, in the order that they came, go to every client
	[[nodiscard]] std::vector<player_commands::Command> TakeTurn(uint32_t turn) override;
	/// The world of the server turns by its own clock
	[[nodiscard]] bool IsBehind(uint32_t /*turn*/) const override { return false; }
	/// The creature file that the god of the slot sent before the start. The world of the server makes the same
	/// creatures as the worlds of the clients from these files.
	[[nodiscard]] std::span<const uint8_t> StartCreatureFile(PlayerNames god) const override;
	[[nodiscard]] std::optional<float> StartAlignment(PlayerNames god) const override;
	/// The checksum of the server's world after a turn. It goes to every client.
	void TurnChecksum(uint32_t turn, uint64_t checksum);

	[[nodiscard]] bool Started() const { return _started; }
	/// True from the query of the lands until the start of the game or a stop of the start
	[[nodiscard]] bool Starting() const { return _startStep != StartStep::None; }
	/// True when the game started and no god's player is connected
	[[nodiscard]] bool Paused() const;
	/// True when the game started and every god left or was defeated: no player can come back. A room with no human god
	/// is over when no observer is connected and no observer can come back.
	[[nodiscard]] bool GameOver() const;
	/// The last turn of the game, after a client asked the server to stop the turns of a decided game. None before.
	[[nodiscard]] std::optional<uint32_t> StoppedAfter() const { return _stoppedAfter; }
	[[nodiscard]] const std::vector<Seat>& Seats() const { return _seats; }
	[[nodiscard]] size_t Observers() const;
	/// The sessions of the observers that are connected or can come back
	[[nodiscard]] const std::vector<Watcher>& Watchers() const { return _watchers; }
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
	/// An observer comes back with the secret of its session. False when no observer has the secret.
	bool ResumeObserver(ConnectionId connection, Peer& peer, const game_socket::Resume& resume, bool world);
	/// Sends the kept turns from a turn, with the server's checksums, and the stop of the turns if they stopped
	void SendKeptTurns(ConnectionId connection, uint32_t from);
	/// Gives a new session to an observer on a connection, and returns its secret. With no secrets, the observer cannot
	/// come back.
	[[nodiscard]] std::string NewWatcher(ConnectionId connection);
	/// The salted hash of a secret. None when the secret is empty or the host has no hasher.
	[[nodiscard]] std::optional<SessionHash> HashOf(std::string_view secret) const;
	/// True when the secret that a client gives is the secret of a session. A session that a restore gave has only the
	/// hash of its secret.
	[[nodiscard]] bool SessionMatches(std::string_view session, const std::optional<SessionHash>& hash,
	                                  std::string_view given) const;
	/// A join after the start: the player of a waiting god comes back into it, and any other player is an observer
	void JoinRunning(ConnectionId connection, Peer& peer, const join_ticket::Claims& claims);
	/// Sends the server's world at the start of the next turn to a connection. False when the server has no world state.
	bool SendWorld(ConnectionId connection);
	void Refuse(ConnectionId connection, std::string_view reason);
	void StartReply(ConnectionId connection, const Peer& peer, const game_socket::StartReply& reply);
	void StartCreature(ConnectionId connection, const Peer& peer, const game_socket::StartCreature& creature);
	/// The gods are ready: the server asks the gods if they have the land
	void QueryLands();
	/// Every god has the land: the gods send their creature files
	void TransferCreatures();
	/// The last creature file came: the world starts to turn
	void StartGame();
	/// The start stops before the game starts: the frame tells the players why
	void StopStart(const game_socket::Frame& why);
	/// The human gods of the seats, with their names, for the start of a player
	[[nodiscard]] std::vector<game_socket::God> Gods() const;
	/// The connections of the gods with a player
	[[nodiscard]] std::set<ConnectionId> GodConnections() const;
	/// The commands of a replayed turn of a restored game. The server sends the turn again only to the players who come
	/// back.
	[[nodiscard]] std::vector<player_commands::Command> TakeReplayedTurn(uint32_t turn);
	void SendTo(ConnectionId connection, const game_socket::Frame& frame);
	void Broadcast(const game_socket::Frame& frame);
	void BroadcastBinary(std::span<const uint8_t> bytes);
	void CloseSeat(uint8_t seat);
	void CompareClient(uint8_t seat, uint32_t turn, uint64_t checksum);
	/// The end box of a decided game opened on a client: no turn comes after the turns closed so far
	void StopTurns();
	[[nodiscard]] bool AnyGodConnected() const;
	/// Tools: the room has no human god. The game starts at once, and observers watch it.
	[[nodiscard]] bool AllComputerGods() const { return _settings.start.allComputerGods; }

	WebSocketServerInterface& _sockets;
	CheckTicket _checkTicket;
	Clock _clock;
	MakeSecret _makeSecret;
	HostSettings _settings;

	std::vector<Seat> _seats;
	std::map<ConnectionId, Peer> _peers;
	std::vector<Watcher> _watchers;
	bool _started {false};
	enum class StartStep : uint8_t
	{
		None,
		/// The server waits for the replies of the gods to its query of the land
		Query,
		/// The server waits for the creature files of the gods
		Transfer,
	};
	StartStep _startStep {StartStep::None};
	/// The connections whose reply or creature file the server waits for in the step of the start
	std::set<ConnectionId> _awaited;
	/// The creature files that the gods sent in the start, by god slot. The server sends each file to all the players and
	/// keeps it only in memory, for its own world.
	std::map<uint8_t, std::vector<uint8_t>> _startCreatures;
	/// The alignments that the gods sent with their creature files, by god slot
	std::map<uint8_t, float> _startAlignments;
	bool _pausedBefore {false};
	/// Tools, a room with no human god: the world turns from the time that as many observers as the room waits for joined
	bool _observersJoined {false};
	/// Tools, a room with no human god: at least one observer joined, so a room with no observer left is over
	bool _observerSeen {false};
	uint32_t _turnsDone {0};
	/// The last turn of the game, once the turns stopped
	std::optional<uint32_t> _stoppedAfter;
	/// The blocks that came since the last turn
	std::vector<turn_codec::StampedBlock> _pending;
	/// The slots whose god leaves at the next turn
	std::vector<uint8_t> _leaving;
	/// The slots of the gods that get their hands at the first turn
	std::vector<uint8_t> _joining;
	/// The bytes of each turn, to send again to a player who comes back
	std::map<uint32_t, std::vector<uint8_t>> _history;
	/// The server's checksum of each turn
	std::map<uint32_t, uint64_t> _checksums;
	/// The clients' checksums of the turns that the server did not do yet, by seat
	std::map<std::pair<uint8_t, uint32_t>, uint64_t> _early;
	uint32_t _refusedBlocks {0};
	WorldState _worldState;
	PartListener _partListener;
	TurnJournal _turnJournal;
	/// The game continues from a snapshot of the server
	bool _restored {false};
	/// The turns that a restored game turns again, by turn
	std::map<uint32_t, std::vector<uint8_t>> _replayed;
	/// The account ids that the lobby banned from the server
	std::set<int64_t> _bans;
};

} // namespace openblack::server
