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

#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <GameSocket.h>

/// The snapshots of a game server (deviation 100). The server keeps the shared world of its game on its own disk, so that
/// it can continue the game after a crash or a restart. A snapshot holds the world state of one turn and the room of the
/// game. A journal after each snapshot holds the turns that the server sent after that snapshot. Thus a server that starts
/// again goes back to the last turn that it sent, and the clients come back to the game with no step back.
/// The files hold only the shared world and the room, never a saved game or the profile of a player.
namespace openblack::server::snapshots
{

/// The turns between two snapshots: 10 seconds of play at 10 turns a second
constexpr uint32_t k_DefaultEveryTurns = 100;
/// The snapshots that the folder keeps. When the last snapshot is torn, the server starts from the snapshot before it.
constexpr uint32_t k_DefaultKeep = 3;
/// The folder of the snapshots under the working folder of the server, when the command line gives none
inline constexpr std::string_view k_DefaultFolder = "snapshots";

/// What the command line of the server gives for the snapshots
struct Settings
{
	std::filesystem::path folder {k_DefaultFolder};
	/// The turns between two snapshots. 0 stops the snapshots.
	uint32_t everyTurns {k_DefaultEveryTurns};
	/// The snapshots that the folder keeps, at least 1
	uint32_t keep {k_DefaultKeep};
	/// At its start, the server loads the newest good snapshot of the folder and continues its game
	bool resume {false};
};

/// A god slot of the room, as the server must know it to let the player come back
struct RoomSeat
{
	/// The state of the slot, as the number of LockstepHost::SeatState
	uint8_t state {0};
	/// The secret of the session, which the client of the player sends to come back
	std::string session;
	/// The account id of the player in the lobby, and the name of the account
	std::string account;
	std::string name;

	bool operator==(const RoomSeat&) const = default;
};

/// The room of a game that started: what the game server needs, in addition to the world, to continue the game
struct Room
{
	/// The start of the game, as the text of the frame that the server sends to the clients
	std::string start;
	std::vector<RoomSeat> seats;
	/// The secrets of the sessions of the observers
	std::vector<std::string> watchers;
	/// The last turn of a decided game
	std::optional<uint32_t> stoppedAfter;
	/// The slots of the gods that leave at the next turn, and the slots of the gods that get their hands at the next turn
	std::vector<uint8_t> leaving;
	std::vector<uint8_t> joining;

	bool operator==(const Room&) const = default;
};

[[nodiscard]] std::vector<uint8_t> EncodeRoom(const Room& room);
/// The room of the bytes. None when the bytes do not read.
[[nodiscard]] std::optional<Room> DecodeRoom(std::span<const uint8_t> bytes);

/// True when the start of a snapshot is the start of the game that the server makes now: the same land, seed and modes
[[nodiscard]] bool SameGame(const game_socket::Start& saved, const game_socket::Start& now);

/// The state of a game server at the start of a turn
struct Snapshot
{
	/// The next turn of the game: the world state is the state at the start of this turn
	uint32_t turn {0};
	Room room;
	/// The world state of the turn, as SaveGameSystemInterface::WorldState gives it
	std::vector<uint8_t> world;
};

/// The bytes of a snapshot file, which end with a checksum of all the bytes before it
[[nodiscard]] std::vector<uint8_t> EncodeSnapshot(const Snapshot& snapshot);
/// The snapshot of the bytes. None when the file is torn or its checksum is not correct.
[[nodiscard]] std::optional<Snapshot> DecodeSnapshot(std::span<const uint8_t> bytes);

/// The kind of a record of a journal
enum class RecordKind : uint8_t
{
	/// The bytes of a turn that the server sent, after the number of the turn
	Turn = 1,
	/// The room, after a change
	Room = 2,
};

struct Record
{
	RecordKind kind {RecordKind::Turn};
	std::vector<uint8_t> bytes;
};

/// The bytes of one record of a journal, with a checksum
[[nodiscard]] std::vector<uint8_t> EncodeRecord(RecordKind kind, std::span<const uint8_t> bytes);
/// The bytes of a turn record: the number of the turn, then the bytes that the server sent
[[nodiscard]] std::vector<uint8_t> TurnRecordBytes(uint32_t turn, std::span<const uint8_t> bytes);
/// The number of the turn and the bytes that the server sent, from the bytes of a turn record
[[nodiscard]] std::optional<std::pair<uint32_t, std::vector<uint8_t>>> ReadTurnRecord(std::span<const uint8_t> bytes);
/// The first bytes of a journal file
[[nodiscard]] std::vector<uint8_t> JournalHeader();
/// The records of a journal file, in order. The read stops at the first record that is torn: the server stopped while it
/// wrote that record. The second value is true when the file had a torn or bad part.
[[nodiscard]] std::pair<std::vector<Record>, bool> ReadJournal(std::span<const uint8_t> bytes);

/// The name of the snapshot file of a turn, and of the journal that starts at a turn
[[nodiscard]] std::string SnapshotFileName(uint32_t turn);
[[nodiscard]] std::string JournalFileName(uint32_t turn);
/// The turn in the name of a snapshot file or of a journal file. None for another file.
[[nodiscard]] std::optional<uint32_t> SnapshotTurnOf(std::string_view fileName);
[[nodiscard]] std::optional<uint32_t> JournalTurnOf(std::string_view fileName);

/// What a server loads to continue its game
struct Restored
{
	Snapshot snapshot;
	/// The room after the last change that the journals keep
	Room room;
	/// The turns that the server sent after the snapshot, by turn, with no gap
	std::map<uint32_t, std::vector<uint8_t>> turns;
	/// The snapshot files that did not read, newest first
	std::vector<std::string> skipped;
	/// The journals ended with a torn or bad record
	bool tornJournal {false};
};

/// The folder of the snapshots of one game server. A snapshot goes into a temporary file first, and only a complete file
/// gets the name of a snapshot. Thus a crash during a write leaves the last snapshot as it was.
/// The writer thread calls Write. The game thread calls the functions of the journal. The two use different files.
class Store
{
public:
	Store(std::filesystem::path folder, uint32_t keep);
	~Store();
	Store(const Store&) = delete;
	Store& operator=(const Store&) = delete;
	Store(Store&&) = delete;
	Store& operator=(Store&&) = delete;

	[[nodiscard]] const std::filesystem::path& Folder() const { return _folder; }

	/// Writes the bytes of a snapshot of a turn, then removes the snapshots and journals after the newest snapshots
	/// that the folder keeps. Gives false when the snapshot did not get written.
	[[nodiscard]] bool Write(uint32_t turn, std::span<const uint8_t> bytes) const;

	/// Starts the journal of the turns from a turn, with the room at that turn. The journal before it closes.
	[[nodiscard]] bool OpenJournal(uint32_t turn, const Room& room);
	/// Adds a turn that the server sends, before it sends it. Nothing when no journal is open.
	void AddTurn(uint32_t turn, std::span<const uint8_t> bytes);
	/// Adds the room after a change. Nothing when no journal is open.
	void AddRoom(const Room& room);
	[[nodiscard]] bool JournalOpen() const { return _journal.is_open(); }

	/// The newest snapshot that reads, with the turns and the room changes of the journals after it. None when the folder
	/// has no snapshot that reads.
	[[nodiscard]] std::optional<Restored> LoadNewest() const;
	/// Removes all the snapshots and journals, at the end of the game
	void Clear();

private:
	void AddRecord(RecordKind kind, std::span<const uint8_t> bytes);
	/// Removes the snapshots after the newest that the folder keeps, the journals before the oldest snapshot that it
	/// keeps, and the temporary files
	void Rotate() const;

	std::filesystem::path _folder;
	uint32_t _keep;
	std::ofstream _journal;
};

} // namespace openblack::server::snapshots
