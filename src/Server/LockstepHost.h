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
#include <utility>
#include <vector>

#include <GameSocket.h>

#include "Commands/TurnCodec.h"
#include "Commands/TurnCommandSource.h"
#include "Server/GameLifetime.h"
#include "Server/LobbyGame.h"
#include "Server/ServerSnapshots.h"
#include "Server/SessionHash.h"
#include "Server/TicketCheck.h"
#include "Server/WebSocketServerInterface.h"

namespace openblack::server
{

/// The most observers of a room, when the settings give no other limit
constexpr size_t k_MostObservers = 64;

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
	/// A room on the local network with no lobby (deviation 113): a join with no ticket gives the name of the player's
	/// profile, and the room takes the player by that name. A god who finds no free slot joins as an observer.
	bool lanNames {false};
	/// The most observers of the room, connected or away. The room refuses an observer over this with "full".
	size_t mostObservers {k_MostObservers};
	/// Whether the server sends the start data of a god (its creature file) to the players as it is. The server sends
	/// empty data in place of data that this refuses. With none, the server sends all data as it is.
	std::function<bool(std::span<const uint8_t> data)> relaysCreature;
	/// The game of one lobby (deviation 236): the room takes only the members of the lobby, each in its role and slot,
	/// and waits for every god of the lobby until the time to join ends. None for a room with no lobby.
	std::optional<lobby_game::Lobby> lobby;
	/// The build id of the server. The room refuses a client of another build with "version" (deviation 248). Empty: the
	/// room takes every build.
	std::string buildId;
	/// The SHA-256 of the server's file of the land, as 64 hex digits in lower case. The room refuses a client that gives
	/// another SHA-256 with "map". Empty: no check at the join.
	std::string mapSha256;
	/// A lobby game: the time that the gods' players have to load the world (deviation 254). The time counts from the
	/// creation of the room (the server listens). When the game starts after that time, it counts from the start.
	int64_t loadWindowMicroseconds {game_lifetime::k_DefaultJoinWindowSeconds * 1'000'000};
};

/// The claims of a player who joins a room on the local network by the name of the profile, as a god. None when the name
/// is empty, longer than the limit or has a control character.
[[nodiscard]] std::optional<join_ticket::Claims> LanClaims(std::string_view name);

/// The most bytes of the name of a player who joins on the local network
constexpr size_t k_MostLanNameBytes = 64;

/// Whether the server leaves a command of a client out of its turns. A real client never sends these commands in a network
/// game:
/// - The commands that only the server sends (a player joins or leaves).
/// - The commands that only a single player game sends (the quick save, the quick load, the music of the player and the
///   tattoos, which stay on the computer of the player in a network game).
/// - The commands of the test tools, when the test commands of the server are off.
[[nodiscard]] bool LeavesOut(const player_commands::Action& action, bool testCommands) noexcept;

/// True when a line of the room's chat can go to the players: it has a character above the space, as the game sends no
/// line of only spaces, and it has at most the characters that the chat box of the game takes. The game counts the
/// characters in UTF-16 code units.
[[nodiscard]] bool IsRoomChatLine(std::string_view text) noexcept;

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
		/// The player's computer loaded the world of the started game (game.loaded)
		bool loaded {false};
		int64_t awaySince {0};
		Compares compares;
		uint32_t resumes {0};
		/// The commands of the god that the server put in its turns, by the tag of the command
		std::map<uint8_t, uint32_t> commands;
		/// The number of the session of the player. Each new session of any seat gets the next number.
		uint32_t sessionNumber {0};
		/// The number of the last block of commands of the session that the server took. The server leaves out a block
		/// that the client sends again after it came back (deviation 100).
		std::optional<uint16_t> lastBlock;
		/// The number of the last block of commands of the session that a closed turn holds
		std::optional<uint16_t> lastBlockInTurn;
		/// The blocks that the client sent again and that the server had, so left out
		uint32_t blocksAgain {0};
		/// The blocks whose commands the server refused because the commands of the god did not fit in the turn
		uint32_t refusedBlocks {0};
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
		/// The name of the observer's account, for the room's chat and for the leave. Empty for a session that a restore gave.
		std::string name;
		/// The number of the observer in the game, from 1. The turns stamp the blocks of the observer with it.
		uint8_t number {0};
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
	/// Gets the bytes of each turn that the server closes, before the server sends them, and the last block of each god
	/// that a closed turn holds. The snapshots of the server keep them in a journal (deviation 100).
	using TurnJournal =
	    std::function<void(uint32_t turn, std::span<const uint8_t> bytes, const snapshots::BlockSequences& blocks)>;
	void SetTurnJournal(TurnJournal journal) { _turnJournal = std::move(journal); }
	/// The room of the game, as a snapshot of the server keeps it
	[[nodiscard]] snapshots::Room RoomRecord() const;
	/// The number of the last block of commands of the session of each god slot that a closed turn holds
	[[nodiscard]] snapshots::BlockSequences BlocksInTurns() const;
	/// Continues a game from a snapshot, before any client connects. The world of the server is at the start of the turn
	/// turnsDone. The gods of the room wait for their players, who come back with the secrets of their sessions. The
	/// world turns the replayed turns first, with no player: these are the turns that the server sent before it stopped.
	/// The server leaves out the blocks of commands up to the last block of each god that the sent turns hold.
	void Restore(const snapshots::Room& room, uint32_t turnsDone, std::map<uint32_t, std::vector<uint8_t>> replayed,
	             const snapshots::BlockSequences& blocks = {});
	/// True while the world must turn the replayed turns of a restored game. The server reads no socket until then.
	[[nodiscard]] bool Replaying() const { return !_replayed.empty(); }
	/// The next turn that the server closes
	[[nodiscard]] uint32_t TurnsDone() const { return _turnsDone; }
	/// Tools: gets the god slot and the turn each time that the world of a client starts to differ from the server's world
	using PartListener = std::function<void(uint8_t seat, uint32_t turn)>;
	void SetPartListener(PartListener listener) { _partListener = std::move(listener); }
	/// The hash of each part of the server's world at one of the last turns, empty for a turn that the server does not
	/// keep. When a client sends its parts of the turn at which its world parted, the log names the parts that differ.
	using PartsOfTurn = std::function<std::vector<std::pair<std::string, uint64_t>>(uint32_t turn)>;
	void SetPartsOfTurn(PartsOfTurn parts) { _partsOfTurn = std::move(parts); }
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
	/// The time to join of the lobby's members ended: the room then starts with the fewest gods of the room
	void EndJoinWindow();
	/// A lobby game: true while the turns wait for the players of the gods to load the world (deviation 254)
	[[nodiscard]] bool WaitsForLoads() const { return _loadWait; }
	/// The slots of the gods whose players did not load the world yet, while the turns wait for them
	[[nodiscard]] std::vector<uint8_t> LoadingSeats() const;
	/// The whole seconds until the wait for the loads ends, rounded up. 0 when the turns do not wait.
	[[nodiscard]] uint32_t LoadSecondsLeft() const;
	/// The time to load ended: each god whose player did not load the world leaves the game, and the turns start with
	/// the other gods
	void EndLoadWait();
	/// The gods with a player in the room
	[[nodiscard]] size_t GodsInRoom() const;
	/// The gods for which the room waits before the start
	[[nodiscard]] uint32_t GodsToWaitFor() const { return _godsToWaitFor; }
	/// The last turn of the game, after a client asked the server to stop the turns of a decided game. None before.
	[[nodiscard]] std::optional<uint32_t> StoppedAfter() const { return _stoppedAfter; }
	[[nodiscard]] const std::vector<Seat>& Seats() const { return _seats; }
	[[nodiscard]] size_t Observers() const;
	/// The sessions of the observers that are connected or can come back
	[[nodiscard]] const std::vector<Watcher>& Watchers() const { return _watchers; }
	/// The god of the observers: the god of the slot after the last god slot of the room, and at least player seven
	[[nodiscard]] PlayerNames ObserversGod() const;
	/// True when the turns sent the god of the observers into the game, and no turn sent it out again
	[[nodiscard]] bool ObserversIn() const { return _observersIn; }
	/// The blocks of commands that the server did not accept: before the start, a command that only the server sends, or
	/// a command that an observer does not send
	[[nodiscard]] uint32_t RefusedBlocks() const { return _refusedBlocks; }
	/// The numbers of the observers that the server gave and whose leave is not in a turn yet, in order
	[[nodiscard]] std::vector<uint8_t> ObserverNumbers() const;
	/// One line for each god slot that had a player: the checksums compared and the same ones
	[[nodiscard]] std::vector<std::string> SummaryLines() const;

private:
	struct Peer
	{
		bool joined {false};
		game_socket::Role role {game_socket::Role::God};
		std::optional<uint8_t> seat;
		/// The time that the connection opened. A connection that does not join in its time closes.
		int64_t openedAt {0};
		/// The name of the player's account
		std::string name;
		/// The number of an observer in the game, from 1. 0 for a god, and for an observer that has no number yet.
		uint8_t observer {0};
		/// The account of the ticket. Empty for a player that joined by name or came back with a session.
		std::string account;
		/// The "sync.parts" frames that the connection sent. The server answers only the first few.
		uint32_t partReports {0};
	};

	/// The most "sync.parts" frames of one connection that the server answers
	static constexpr uint32_t k_MostPartReports = 4;

	/// The blocks of commands of an observer that the server took, as the seat of a god keeps them
	struct ObserverBlocks
	{
		/// The number of the last block of the observer that the server took
		std::optional<uint16_t> lastBlock;
		/// The number of the last block of the observer that a closed turn holds
		std::optional<uint16_t> lastBlockInTurn;
	};

	void Opened(ConnectionId connection);
	void Text(ConnectionId connection, std::string_view text);
	void Binary(ConnectionId connection, std::span<const uint8_t> bytes);
	void Closed(ConnectionId connection, uint16_t code);
	/// A client sent the hash of each part of its world at the turn at which its world parted: the log names the parts
	/// that differ from the server's, and the client gets the server's parts
	void PartsFrom(ConnectionId connection, Peer& peer, const game_socket::SyncParts& parts);
	/// The name of a connection for the log: the name of the player, or "not joined"
	[[nodiscard]] std::string WhoIs(ConnectionId connection) const;
	void Join(ConnectionId connection, Peer& peer, const game_socket::Join& join);
	void Resume(ConnectionId connection, Peer& peer, const game_socket::Resume& resume);
	/// An observer comes back with the secret of its session. False when no observer has the secret.
	bool ResumeObserver(ConnectionId connection, Peer& peer, const game_socket::Resume& resume, bool world);
	/// Sends the kept turns from a turn, with the server's checksums, and the stop of the turns if they stopped
	void SendKeptTurns(ConnectionId connection, uint32_t from);
	/// Gives a new session to an observer on a connection, with the number of the observer, and returns its secret. With
	/// no secrets, the observer cannot come back.
	[[nodiscard]] std::string NewWatcher(ConnectionId connection, std::string name, uint8_t number);
	/// A line of the room's chat from a player: it goes to every other player in a room on the local network, until the
	/// start
	void ChatInRoom(ConnectionId connection, const Peer& peer, const game_socket::RoomChat& chat);
	/// The name of a player in the room: the name of the god's seat, or the name of the observer. Empty for none.
	[[nodiscard]] std::string PeerName(ConnectionId connection, const Peer& peer) const;
	/// The session of an observer ended after the start: the other players see that the observer left the game
	void ObserverLeft(std::string_view name);
	/// A new observer comes into the game: it gets the lowest number that no observer has, and the next turn tells the
	/// worlds. 0 when every number is taken.
	[[nodiscard]] uint8_t ObserverCame(std::string_view name);
	/// The session of the observer with the number ended: the next turn tells the worlds. The number is free again after
	/// that turn.
	void ObserverGone(uint8_t number);
	/// The lowest number that no observer has, none when every number is taken
	[[nodiscard]] std::optional<uint8_t> FreeObserverNumber() const;
	/// Takes a block of commands from an observer into the next turn
	void ObserverBlock(const Peer& peer, turn_codec::CommandBlock& block);
	/// The blocks of the server that tell the worlds of the god of the observers and of the observers that came and left
	void AddObserverBlocks(turn_codec::TurnBundle& bundle, uint32_t turn);
	/// The observers that are in the game: each observer that is connected, and each observer that can come back
	[[nodiscard]] size_t ObserversInGame() const;
	/// The salted hash of a secret. None when the secret is empty or the host has no hasher.
	[[nodiscard]] std::optional<SessionHash> HashOf(std::string_view secret) const;
	/// True when the secret that a client gives is the secret of a session. A session that a restore gave has only the
	/// hash of its secret.
	[[nodiscard]] bool SessionMatches(std::string_view session, const std::optional<SessionHash>& hash,
	                                  std::string_view given) const;
	/// A join after the start: the player of a waiting god comes back into it, and any other player is an observer
	/// A join by the name of the profile on the local network never takes a god.
	void JoinRunning(ConnectionId connection, Peer& peer, const join_ticket::Claims& claims, bool byName);
	/// True when the room has as many observers as it takes
	[[nodiscard]] bool ObserversFull() const;
	/// Sends the server's world at the start of the next turn to a connection. False when the server has no world state.
	bool SendWorld(ConnectionId connection);
	/// Refuses a connection with the short reason that the client gets. The log also gets the detail.
	void Refuse(ConnectionId connection, std::string_view reason, std::string_view detail = {});
	/// Takes the players of the accounts that the lobby banned out of the started game
	void RemoveBannedFromGame();
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
	/// The gods for which the room waits before the start: every god of the lobby until the time to join ends, then the
	/// fewest gods of the room
	uint32_t _godsToWaitFor {1};
	/// A lobby game that started waits for the loads of the gods' players. The wait ends one time and does not come back.
	bool _loadWait {false};
	/// The names in the last game.wait that the server sent
	std::vector<std::string> _loadingSent;
	/// The time when the room was made, by the clock of the room
	int64_t _createdAt {0};
	/// The time when the wait for the loads ends, by the clock of the room
	int64_t _loadWaitEndsAt {0};
	/// Sends game.wait when the list of the players who load changed, and ends the wait when the list is empty
	void UpdateLoadWait();
	/// Sends the wait for the loads to one connection, while the turns wait
	void SendLoadWait(ConnectionId connection);
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
	/// A block that came since the last turn, with the god slot and the session that sent it, or the number of the
	/// observer that sent it
	struct PendingBlock
	{
		turn_codec::StampedBlock block;
		uint8_t seat {0};
		uint32_t session {0};
		uint8_t observer {0};
	};
	/// The blocks that came since the last turn
	std::vector<PendingBlock> _pending;
	/// The bytes of the commands of each god slot in the blocks that came since the last turn
	std::map<uint8_t, size_t> _pendingBytes;
	/// The bytes of the commands of each observer in the blocks that came since the last turn, by its number
	std::map<uint8_t, size_t> _pendingObserverBytes;
	/// The blocks of each observer by its number. A number is in the map from the come of the observer until a turn holds
	/// its leave, so that no other observer gets the number before the worlds know of the leave.
	std::map<uint8_t, ObserverBlocks> _observerBlocks;
	/// The observers that came and that no turn told yet: the number and the name of each
	std::vector<std::pair<uint8_t, std::string>> _observerArrivals;
	/// The numbers of the observers that left and that no turn told yet
	std::vector<uint8_t> _observerDepartures;
	/// The number of the last session that a god's player got
	uint32_t _sessions {0};
	/// The slots whose god leaves at the next turn
	std::vector<uint8_t> _leaving;
	/// The slots of the gods that get their hands at the first turn
	std::vector<uint8_t> _joining;
	/// The turns sent the god of the observers into the game, and no turn sent it out again
	bool _observersIn {false};
	/// The bytes of each turn, to send again to a player who comes back
	std::map<uint32_t, std::vector<uint8_t>> _history;
	/// The server's checksum of each turn
	std::map<uint32_t, uint64_t> _checksums;
	/// The clients' checksums of the turns that the server did not do yet, by seat
	std::map<std::pair<uint8_t, uint32_t>, uint64_t> _early;
	uint32_t _refusedBlocks {0};
	WorldState _worldState;
	PartListener _partListener;
	PartsOfTurn _partsOfTurn;
	TurnJournal _turnJournal;
	/// The game continues from a snapshot of the server
	bool _restored {false};
	/// The turns that a restored game turns again, by turn
	std::map<uint32_t, std::vector<uint8_t>> _replayed;
	/// The account ids that the lobby banned from the server
	std::set<int64_t> _bans;
};

} // namespace openblack::server
