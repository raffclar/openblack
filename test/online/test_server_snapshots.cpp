/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include "Server/Desktop/Sha256SessionHasher.h"
#include "Server/ServerSnapshots.h"

using namespace openblack::server::snapshots;
using openblack::server::SessionHash;
using openblack::server::Sha256SessionHasher;

namespace
{

/// A hash with fixed bytes, so that two rooms of MakeRoom are the same
SessionHash FixedHash(uint8_t value)
{
	SessionHash hash;
	hash.salt.fill(value);
	hash.digest.fill(static_cast<uint8_t>(value + 1));
	return hash;
}

Room MakeRoom()
{
	return Room {.start = R"({"type":"room.start","land":"Land1.txt"})",
	             .seats = {{.state = 1, .session = FixedHash(1), .account = "11", .name = "alice", .editor = true},
	                       {.state = 2, .session = FixedHash(3), .account = "12", .name = "bob"},
	                       {.state = 0, .session = std::nullopt, .account = {}, .name = {}}},
	             .watchers = {FixedHash(5)},
	             .stoppedAfter = std::nullopt,
	             .leaving = {1},
	             .joining = {},
	             .watcherNames = {"carol"},
	             .observersIn = true,
	             .watcherNumbers = {2},
	             .arrivingNumbers = {3},
	             .arrivingNames = {"dave"},
	             .departingNumbers = {1},
	             .watcherEditors = {true},
	             .clock = {.paused = true, .speed = 2.0f, .steps = 1}};
}

/// The bytes of the clock at the end of the room of format 7: the pause (1 byte), the speed (4 bytes) and the steps
/// (4 bytes)
constexpr size_t k_ClockBytes = 9;

/// The bytes of the claims at the end of the room of format 6: two lists, each with a 4-byte count and one byte for each
/// god slot or observer
size_t ClaimBytes(const Room& room)
{
	return 8 + room.seats.size() + room.watcherEditors.size();
}

/// True when the bytes have the text in them
bool HasText(const std::vector<uint8_t>& bytes, std::string_view text)
{
	return !std::ranges::search(bytes, text, [](uint8_t byte, char letter) {
		        return byte == static_cast<uint8_t>(letter);
	        }).empty();
}

/// A 64-bit FNV-1a hash of the bytes, as the snapshot files have at their end
uint64_t Checksum(const std::vector<uint8_t>& bytes)
{
	uint64_t hash = 14695981039346656037ull;
	for (const auto byte : bytes)
	{
		hash = (hash ^ byte) * 1099511628211ull;
	}
	return hash;
}

std::vector<uint8_t> World(uint32_t turn)
{
	return std::vector<uint8_t>(64, static_cast<uint8_t>(turn));
}

std::vector<uint8_t> TurnBytes(uint32_t turn)
{
	return {static_cast<uint8_t>(turn), static_cast<uint8_t>(turn >> 8), 7};
}

std::vector<uint8_t> ReadAll(const std::filesystem::path& path)
{
	std::ifstream file(path, std::ios::binary);
	return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

void WriteAll(const std::filesystem::path& path, const std::vector<uint8_t>& bytes)
{
	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

/// A folder of its own for each test, which the test removes at its end
class SnapshotStoreTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
		folder = std::filesystem::temp_directory_path() / (std::string("openblack_snapshots_") + info->name());
		std::error_code error;
		std::filesystem::remove_all(folder, error);
	}

	void TearDown() override
	{
		std::error_code error;
		std::filesystem::remove_all(folder, error);
	}

	/// A snapshot of a turn, then the journal of the turns after it until the turn before the next snapshot
	void Play(Store& store, uint32_t snapshotTurn, uint32_t endTurn)
	{
		const auto room = MakeRoom();
		ASSERT_TRUE(store.OpenJournal(snapshotTurn, room));
		ASSERT_TRUE(store.Write(
		    snapshotTurn, EncodeSnapshot({.turn = snapshotTurn, .room = room, .world = World(snapshotTurn), .blocks = {}})));
		for (auto turn = snapshotTurn; turn < endTurn; ++turn)
		{
			store.AddTurn(turn, TurnBytes(turn));
		}
	}

	std::filesystem::path folder;
};

} // namespace

TEST(ServerSnapshots, TheRoomReadsBackAsItWasWritten)
{
	auto room = MakeRoom();
	room.stoppedAfter = 42;
	EXPECT_EQ(DecodeRoom(EncodeRoom(room)), room);
	auto bytes = EncodeRoom(room);
	bytes.pop_back();
	EXPECT_FALSE(DecodeRoom(bytes).has_value());
}

// A room of format 3 has no names of the observers and no god of the observers. It still reads.
TEST(ServerSnapshots, TheRoomOfTheThirdFormatReadsWithNoNames)
{
	auto room = MakeRoom();
	room.watcherNames.clear();
	room.observersIn = false;
	room.watcherNumbers.clear();
	room.arrivingNumbers.clear();
	room.arrivingNames.clear();
	room.departingNumbers.clear();
	auto bytes = EncodeRoom(room);
	// The four empty lists of the observers (4 bytes each) end the room of format 5. The count of the names (4 bytes)
	// and the god of the observers (1 byte) end the room of format 4.
	bytes.resize(bytes.size() - k_ClockBytes - ClaimBytes(room) - 16 - 5);
	const auto old = DecodeRoom(bytes);
	ASSERT_TRUE(old.has_value());
	EXPECT_EQ(old->watchers, room.watchers);
	EXPECT_TRUE(old->watcherNames.empty());
	EXPECT_FALSE(old->observersIn);
}

// The room keeps the editor claims of the gods and the observers, so that they keep the claim after a restart
TEST(ServerSnapshots, TheRoomKeepsTheEditorClaimsOfTheGodsAndTheObservers)
{
	const auto room = MakeRoom();
	const auto read = DecodeRoom(EncodeRoom(room));
	ASSERT_TRUE(read.has_value());
	ASSERT_EQ(read->seats.size(), 3u);
	EXPECT_TRUE(read->seats[0].editor);
	EXPECT_FALSE(read->seats[1].editor);
	EXPECT_FALSE(read->seats[2].editor);
	EXPECT_EQ(read->watcherEditors, (std::vector<bool> {true}));
}

// The room keeps the debugger claims in the same bytes as the editor claims
TEST(ServerSnapshots, TheRoomKeepsTheDebuggerClaimsOfTheGodsAndTheObservers)
{
	auto room = MakeRoom();
	room.seats[1].debugger = true;
	room.watcherDebuggers = {true};
	const auto bytes = EncodeRoom(room);
	EXPECT_EQ(bytes.size(), EncodeRoom(MakeRoom()).size());
	const auto read = DecodeRoom(bytes);
	ASSERT_TRUE(read.has_value());
	EXPECT_TRUE(read->seats[0].editor);
	EXPECT_FALSE(read->seats[0].debugger);
	EXPECT_FALSE(read->seats[1].editor);
	EXPECT_TRUE(read->seats[1].debugger);
	EXPECT_EQ(read->watcherDebuggers, (std::vector<bool> {true}));
	EXPECT_EQ(*read, room);
	// A room with no debugger claim keeps no list of them
	const auto plain = DecodeRoom(EncodeRoom(MakeRoom()));
	ASSERT_TRUE(plain.has_value());
	EXPECT_TRUE(plain->watcherDebuggers.empty());
}

// A room of format 5 has no claims. It still reads, and no god or observer has the editor claim.
TEST(ServerSnapshots, TheRoomOfTheFifthFormatReadsWithNoClaims)
{
	const auto room = MakeRoom();
	auto bytes = EncodeRoom(room);
	bytes.resize(bytes.size() - k_ClockBytes - ClaimBytes(room));
	const auto old = DecodeRoom(bytes);
	ASSERT_TRUE(old.has_value());
	EXPECT_EQ(old->departingNumbers, room.departingNumbers);
	EXPECT_TRUE(std::ranges::none_of(old->seats, [](const RoomSeat& seat) { return seat.editor; }));
	EXPECT_TRUE(old->watcherEditors.empty());
}

// The list of the claims of the gods has one byte for each god slot. A list of a different length does not read.
TEST(ServerSnapshots, TheRoomWithClaimsForTheWrongGodSlotsDoesNotRead)
{
	auto room = MakeRoom();
	room.watcherEditors.clear();
	auto bytes = EncodeRoom(room);
	// Put a list of claims for two god slots in place of the list for the three god slots of the room
	bytes.resize(bytes.size() - k_ClockBytes - ClaimBytes(room));
	const std::array<uint8_t, 4> twoSlots {2, 0, 0, 0};
	bytes.insert(bytes.end(), twoSlots.begin(), twoSlots.end());
	bytes.push_back(1);
	bytes.push_back(0);
	const std::array<uint8_t, 4> noObservers {0, 0, 0, 0};
	bytes.insert(bytes.end(), noObservers.begin(), noObservers.end());
	EXPECT_FALSE(DecodeRoom(bytes).has_value());
}

TEST(ServerSnapshots, ASnapshotReadsBackAndATornOrChangedSnapshotDoesNot)
{
	const Snapshot snapshot {.turn = 300, .room = MakeRoom(), .world = World(300), .blocks = {{0, 65'535}, {3, 7}}};
	const auto bytes = EncodeSnapshot(snapshot);
	const auto read = DecodeSnapshot(bytes);
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(read->turn, 300u);
	EXPECT_EQ(read->room, snapshot.room);
	EXPECT_EQ(read->world, snapshot.world);
	EXPECT_EQ(read->blocks, snapshot.blocks);

	// A server that stopped during the write leaves a part of the file
	const std::vector<uint8_t> torn(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(bytes.size() / 2));
	EXPECT_FALSE(DecodeSnapshot(torn).has_value());
	auto changed = bytes;
	changed[changed.size() / 2] ^= 0x40;
	EXPECT_FALSE(DecodeSnapshot(changed).has_value());
	EXPECT_FALSE(DecodeSnapshot({}).has_value());
}

TEST(ServerSnapshots, TheJournalReadStopsAtATornRecord)
{
	auto bytes = JournalHeader();
	for (uint32_t turn = 5; turn < 8; ++turn)
	{
		const auto record = EncodeRecord(RecordKind::Turn, TurnRecordBytes(turn, TurnBytes(turn)));
		bytes.insert(bytes.end(), record.begin(), record.end());
	}
	const auto whole = ReadJournal(bytes);
	ASSERT_EQ(whole.first.size(), 3u);
	EXPECT_FALSE(whole.second);
	const auto turn = ReadTurnRecord(whole.first[1].bytes);
	ASSERT_TRUE(turn.has_value());
	EXPECT_EQ(turn->turn, 6u);
	EXPECT_EQ(turn->bytes, TurnBytes(6));
	EXPECT_TRUE(turn->blocks.empty());

	bytes.resize(bytes.size() - 3);
	const auto torn = ReadJournal(bytes);
	EXPECT_EQ(torn.first.size(), 2u);
	EXPECT_TRUE(torn.second);
}

TEST(ServerSnapshots, TheFileNamesGiveTheirTurns)
{
	EXPECT_EQ(SnapshotFileName(1200), "snapshot-0000001200.obsnap");
	EXPECT_EQ(SnapshotTurnOf(SnapshotFileName(1200)), 1200u);
	EXPECT_EQ(JournalTurnOf(JournalFileName(7)), 7u);
	EXPECT_FALSE(SnapshotTurnOf("snapshot-0000001200.obsnap.tmp").has_value());
	EXPECT_FALSE(SnapshotTurnOf(JournalFileName(7)).has_value());
	EXPECT_FALSE(JournalTurnOf("journal-x.objournal").has_value());
}

TEST(ServerSnapshots, OnlyTheSameLandSeedAndModesAreTheSameGame)
{
	const openblack::game_socket::Start start {.land = "Online Maps/mpm_2p_1.map", .seed = 1, .turn = 0, .opening = "cow"};
	auto later = start;
	later.turn = 40;
	later.names = {"alice"};
	EXPECT_TRUE(SameGame(start, later));
	auto otherSeed = start;
	otherSeed.seed = 2;
	EXPECT_FALSE(SameGame(start, otherSeed));
	auto otherLand = start;
	otherLand.land = "Online Maps/mpm_2p_2.map";
	EXPECT_FALSE(SameGame(start, otherLand));
}

TEST_F(SnapshotStoreTest, TheFolderKeepsTheNewestSnapshotsAndTheirJournals)
{
	Store store(folder, 2);
	Play(store, 1, 101);
	Play(store, 101, 201);
	Play(store, 201, 250);
	EXPECT_FALSE(std::filesystem::exists(folder / SnapshotFileName(1)));
	EXPECT_FALSE(std::filesystem::exists(folder / JournalFileName(1)));
	EXPECT_TRUE(std::filesystem::exists(folder / SnapshotFileName(101)));
	EXPECT_TRUE(std::filesystem::exists(folder / JournalFileName(101)));
	EXPECT_TRUE(std::filesystem::exists(folder / SnapshotFileName(201)));
	EXPECT_TRUE(std::filesystem::exists(folder / JournalFileName(201)));

	const auto restored = store.LoadNewest();
	ASSERT_TRUE(restored.has_value());
	EXPECT_EQ(restored->snapshot.turn, 201u);
	EXPECT_EQ(restored->snapshot.world, World(201));
	EXPECT_TRUE(restored->skipped.empty());
	ASSERT_EQ(restored->turns.size(), 49u);
	EXPECT_EQ(restored->turns.begin()->first, 201u);
	EXPECT_EQ(restored->turns.rbegin()->first, 249u);
	EXPECT_EQ(restored->turns.at(230), TurnBytes(230));
	EXPECT_FALSE(restored->tornJournal);
}

TEST_F(SnapshotStoreTest, ATornNewestSnapshotIsSkippedAndTheOlderOneUsesBothJournals)
{
	Store store(folder, 3);
	Play(store, 1, 101);
	Play(store, 101, 150);
	// The server stopped during the write of the newest snapshot: only a part of the file is there
	const auto newest = folder / SnapshotFileName(101);
	auto bytes = ReadAll(newest);
	bytes.resize(bytes.size() / 3);
	WriteAll(newest, bytes);

	const auto restored = store.LoadNewest();
	ASSERT_TRUE(restored.has_value());
	EXPECT_EQ(restored->snapshot.turn, 1u);
	ASSERT_EQ(restored->skipped.size(), 1u);
	EXPECT_EQ(restored->skipped.front(), SnapshotFileName(101));
	// The turns go on through the journal of the torn snapshot with no gap
	ASSERT_EQ(restored->turns.size(), 149u);
	EXPECT_EQ(restored->turns.rbegin()->first, 149u);
}

TEST_F(SnapshotStoreTest, ATornLastTurnIsLeftOutAndTheLastRoomChangeIsKept)
{
	Store store(folder, 3);
	Play(store, 10, 20);
	auto room = MakeRoom();
	room.seats[1].state = 3;
	room.leaving.clear();
	store.AddRoom(room);
	store.AddTurn(20, TurnBytes(20));
	// The server stopped during the write of the last record
	const auto journal = folder / JournalFileName(10);
	auto bytes = ReadAll(journal);
	bytes.resize(bytes.size() - 2);
	WriteAll(journal, bytes);

	const auto restored = Store(folder, 3).LoadNewest();
	ASSERT_TRUE(restored.has_value());
	EXPECT_TRUE(restored->tornJournal);
	EXPECT_EQ(restored->turns.size(), 10u);
	EXPECT_EQ(restored->turns.rbegin()->first, 19u);
	EXPECT_EQ(restored->room, room);
	EXPECT_EQ(restored->snapshot.room, MakeRoom());
}

TEST_F(SnapshotStoreTest, AFolderWithNoGoodSnapshotGivesNothingAndTheEndOfTheGameClearsIt)
{
	Store store(folder, 3);
	EXPECT_FALSE(store.LoadNewest().has_value());
	Play(store, 1, 5);
	WriteAll(folder / (SnapshotFileName(50) + ".tmp"), {1, 2, 3});
	WriteAll(folder / SnapshotFileName(60), {1, 2, 3});
	const auto restored = store.LoadNewest();
	ASSERT_TRUE(restored.has_value());
	EXPECT_EQ(restored->snapshot.turn, 1u);

	store.Clear();
	EXPECT_FALSE(store.LoadNewest().has_value());
	EXPECT_TRUE(std::filesystem::is_empty(folder));
}

TEST(SessionHashes, TheDigestIsTheSha256OfTheSaltThenTheSecret)
{
	// The SHA-256 test value of "abc"
	constexpr std::array<uint8_t, 32> k_Abc {0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40,
	                                         0xde, 0x5d, 0xae, 0x22, 0x23, 0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17,
	                                         0x7a, 0x9c, 0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad};
	const std::array<uint8_t, 2> salt {'a', 'b'};
	EXPECT_EQ(openblack::server::SaltedSha256(salt, "c"), k_Abc);
	EXPECT_EQ(openblack::server::SaltedSha256({}, "abc"), k_Abc);
}

TEST(SessionHashes, OnlyTheSameSecretMatchesAndEachHashHasItsOwnSalt)
{
	const Sha256SessionHasher hasher;
	const auto first = hasher.Hash("secret-1");
	const auto second = hasher.Hash("secret-1");
	ASSERT_TRUE(first.has_value());
	ASSERT_TRUE(second.has_value());
	EXPECT_NE(first->salt, second->salt);
	EXPECT_NE(first->digest, second->digest);
	EXPECT_TRUE(hasher.Matches(*first, "secret-1"));
	EXPECT_TRUE(hasher.Matches(*second, "secret-1"));
	EXPECT_FALSE(hasher.Matches(*first, "secret-2"));
	EXPECT_FALSE(hasher.Matches(*first, "secret-"));
	EXPECT_FALSE(hasher.Matches(*first, ""));
}

TEST_F(SnapshotStoreTest, TheFilesKeepNoSecretOfASessionButTheHashesFindTheRightSecrets)
{
	const Sha256SessionHasher hasher;
	auto room = MakeRoom();
	room.seats[0].session = hasher.Hash("god-secret-alice");
	room.seats[1].session = hasher.Hash("god-secret-bob");
	room.watchers = {*hasher.Hash("observer-secret-carol")};
	Store store(folder, 3);
	ASSERT_TRUE(store.OpenJournal(1, room));
	ASSERT_TRUE(store.Write(1, EncodeSnapshot({.turn = 1, .room = room, .world = World(1), .blocks = {}})));
	store.AddRoom(room);
	store.AddTurn(1, TurnBytes(1));

	size_t files = 0;
	for (const auto& entry : std::filesystem::directory_iterator(folder))
	{
		const auto bytes = ReadAll(entry.path());
		++files;
		EXPECT_FALSE(HasText(bytes, "secret")) << entry.path().filename().string();
		EXPECT_TRUE(HasText(bytes, "alice")) << entry.path().filename().string();
	}
	EXPECT_EQ(files, 2u);

	const auto restored = Store(folder, 3).LoadNewest();
	ASSERT_TRUE(restored.has_value());
	ASSERT_TRUE(restored->room.seats[0].session.has_value());
	EXPECT_TRUE(hasher.Matches(*restored->room.seats[0].session, "god-secret-alice"));
	EXPECT_FALSE(hasher.Matches(*restored->room.seats[0].session, "god-secret-bob"));
	ASSERT_EQ(restored->room.watchers.size(), 1u);
	EXPECT_TRUE(hasher.Matches(restored->room.watchers[0], "observer-secret-carol"));
}

// A room of format 4 has no numbers of the observers. It still reads.
TEST(ServerSnapshots, TheRoomOfTheFourthFormatReadsWithNoNumbersOfObservers)
{
	auto room = MakeRoom();
	room.watcherNumbers.clear();
	room.arrivingNumbers.clear();
	room.arrivingNames.clear();
	room.departingNumbers.clear();
	auto bytes = EncodeRoom(room);
	bytes.resize(bytes.size() - k_ClockBytes - ClaimBytes(room) - 16);
	const auto old = DecodeRoom(bytes);
	ASSERT_TRUE(old.has_value());
	EXPECT_EQ(old->watcherNames, room.watcherNames);
	EXPECT_TRUE(old->observersIn);
	EXPECT_TRUE(old->watcherNumbers.empty());
	EXPECT_TRUE(old->arrivingNumbers.empty());
}

TEST(ServerSnapshots, ASnapshotOfAnEarlierFormatDoesNotLoad)
{
	const auto now = EncodeSnapshot({.turn = 300, .room = MakeRoom(), .world = World(300), .blocks = {}});
	ASSERT_TRUE(DecodeSnapshot(now).has_value());
	// The first format kept the secrets of the sessions, and the second format kept no blocks of commands. The version is
	// after the 8 bytes of the magic text.
	// The formats 3 to 6 still load.
	for (const uint8_t version : {1, 2, 3, 4, 5, 6})
	{
		auto bytes = now;
		bytes.resize(bytes.size() - sizeof(uint64_t));
		ASSERT_EQ(bytes[8], 7);
		bytes[8] = version;
		const auto checksum = Checksum(bytes);
		for (int shift = 0; shift < 64; shift += 8)
		{
			bytes.push_back(static_cast<uint8_t>(checksum >> shift));
		}
		EXPECT_EQ(DecodeSnapshot(bytes).has_value(), version >= 3) << static_cast<int>(version);
	}
}

TEST(ServerSnapshots, ATurnRecordKeepsTheLastBlocksOfTheGods)
{
	const BlockSequences blocks {{0, 12}, {1, 0}, {31, 65'535}};
	const auto turn = ReadTurnRecord(TurnRecordBytes(77, TurnBytes(77), blocks));
	ASSERT_TRUE(turn.has_value());
	EXPECT_EQ(turn->turn, 77u);
	EXPECT_EQ(turn->blocks, blocks);
	EXPECT_EQ(turn->bytes, TurnBytes(77));
	// A record that ends in the list of the blocks does not read
	auto torn = TurnRecordBytes(77, {}, blocks);
	torn.pop_back();
	EXPECT_FALSE(ReadTurnRecord(torn).has_value());
}

TEST_F(SnapshotStoreTest, TheLastBlocksComeFromTheLastTurnOfTheJournals)
{
	Store store(folder, 3);
	const auto room = MakeRoom();
	ASSERT_TRUE(store.OpenJournal(10, room));
	ASSERT_TRUE(store.Write(10, EncodeSnapshot({.turn = 10, .room = room, .world = World(10), .blocks = {{0, 4}}})));
	store.AddTurn(10, TurnBytes(10), {{0, 4}});
	store.AddTurn(11, TurnBytes(11), {{0, 6}, {1, 2}});
	const auto restored = Store(folder, 3).LoadNewest();
	ASSERT_TRUE(restored.has_value());
	EXPECT_EQ(restored->blocks, (BlockSequences {{0, 6}, {1, 2}}));

	// With no turn after the snapshot, the blocks of the snapshot count
	Store other(folder / "other", 3);
	ASSERT_TRUE(other.OpenJournal(10, room));
	ASSERT_TRUE(other.Write(10, EncodeSnapshot({.turn = 10, .room = room, .world = World(10), .blocks = {{1, 9}}})));
	const auto onlySnapshot = Store(folder / "other", 3).LoadNewest();
	ASSERT_TRUE(onlySnapshot.has_value());
	EXPECT_EQ(onlySnapshot->blocks, (BlockSequences {{1, 9}}));
}

TEST_F(SnapshotStoreTest, ASlotWithANewSessionAfterItsLastBlockHasNoLastBlock)
{
	Store store(folder, 3);
	auto room = MakeRoom();
	ASSERT_TRUE(store.OpenJournal(10, room));
	ASSERT_TRUE(store.Write(10, EncodeSnapshot({.turn = 10, .room = room, .world = World(10), .blocks = {}})));
	store.AddTurn(10, TurnBytes(10), {{0, 40}, {1, 2}});
	// The player of slot 0 came back in a new process, with a new session that counts its blocks from 0
	ASSERT_FALSE(room.seats.empty());
	room.seats[0].session = FixedHash(9);
	store.AddRoom(room);
	const auto restored = Store(folder, 3).LoadNewest();
	ASSERT_TRUE(restored.has_value());
	EXPECT_EQ(restored->blocks, (BlockSequences {{1, 2}}));
}

// The room keeps the clock of the room, so that a restart does not end an editor pause or change the speed
TEST(ServerSnapshots, TheRoomKeepsThePauseTheStepsAndTheSpeedOfTheClock)
{
	const auto room = MakeRoom();
	const auto read = DecodeRoom(EncodeRoom(room));
	ASSERT_TRUE(read.has_value());
	EXPECT_TRUE(read->clock.paused);
	EXPECT_FLOAT_EQ(read->clock.speed, 2.0f);
	EXPECT_EQ(read->clock.steps, 1u);
}

// A room of format 6 has no clock. It still reads, with the clock that turns at the normal speed.
TEST(ServerSnapshots, TheRoomOfTheSixthFormatReadsWithTheNormalClock)
{
	const auto room = MakeRoom();
	auto bytes = EncodeRoom(room);
	bytes.resize(bytes.size() - k_ClockBytes);
	const auto old = DecodeRoom(bytes);
	ASSERT_TRUE(old.has_value());
	EXPECT_EQ(old->watcherEditors, room.watcherEditors);
	EXPECT_EQ(old->clock, openblack::online::room_clock::State {});
	// A room that ends in the clock does not read
	bytes = EncodeRoom(room);
	bytes.pop_back();
	EXPECT_FALSE(DecodeRoom(bytes).has_value());
}

TEST(ServerSnapshots, ASnapshotKeepsTheFilesOfTheEditorInTheOrderThatTheyCame)
{
	const Snapshot snapshot {.turn = 300,
	                         .room = MakeRoom(),
	                         .world = World(300),
	                         .blocks = {},
	                         .editorFiles = {{.kind = EditorFileKind::Mind, .bytes = {1, 2, 3}},
	                                         {.kind = EditorFileKind::ScriptProgram, .bytes = {4}},
	                                         {.kind = EditorFileKind::ScriptSource, .bytes = {5, 6}},
	                                         {.kind = EditorFileKind::LandScript, .bytes = {7}}}};
	const auto read = DecodeSnapshot(EncodeSnapshot(snapshot));
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(read->editorFiles, snapshot.editorFiles);
}

// A snapshot of format 6 has no files of the editor. It still reads, with no files.
TEST(ServerSnapshots, ASnapshotOfTheSixthFormatReadsWithNoFilesOfTheEditor)
{
	auto bytes = EncodeSnapshot({.turn = 300, .room = MakeRoom(), .world = World(300), .blocks = {{0, 2}}});
	// Remove the checksum and the count of the files (4 bytes), then add the checksum of the bytes of format 6
	bytes.resize(bytes.size() - sizeof(uint64_t) - 4);
	bytes[8] = 6;
	const auto checksum = Checksum(bytes);
	for (int shift = 0; shift < 64; shift += 8)
	{
		bytes.push_back(static_cast<uint8_t>(checksum >> shift));
	}
	const auto old = DecodeSnapshot(bytes);
	ASSERT_TRUE(old.has_value());
	EXPECT_EQ(old->blocks, (BlockSequences {{0, 2}}));
	EXPECT_TRUE(old->editorFiles.empty());
}

TEST(ServerSnapshots, AnEditorFileRecordOfAnUnknownKindOrWithMissingBytesDoesNotRead)
{
	const EditorFile file {.kind = EditorFileKind::LandScript, .bytes = {9, 8, 7}};
	EXPECT_EQ(ReadEditorFileRecord(EditorFileRecordBytes(file)), file);
	auto unknown = EditorFileRecordBytes(file);
	unknown[0] = 4;
	EXPECT_FALSE(ReadEditorFileRecord(unknown).has_value());
	auto torn = EditorFileRecordBytes(file);
	torn.pop_back();
	EXPECT_FALSE(ReadEditorFileRecord(torn).has_value());
}

// The files of the snapshot come first, then the files of the journal, so that a replayed turn finds its file
TEST_F(SnapshotStoreTest, TheFilesOfTheEditorComeFromTheSnapshotAndThenFromTheJournals)
{
	const EditorFile first {.kind = EditorFileKind::Mind, .bytes = {1}};
	const EditorFile second {.kind = EditorFileKind::ScriptProgram, .bytes = {2, 2}};
	const EditorFile third {.kind = EditorFileKind::LandScript, .bytes = {3, 3, 3}};
	Store store(folder, 3);
	auto room = MakeRoom();
	ASSERT_TRUE(store.OpenJournal(10, room));
	ASSERT_TRUE(
	    store.Write(10, EncodeSnapshot({.turn = 10, .room = room, .world = World(10), .blocks = {}, .editorFiles = {first}})));
	store.AddTurn(10, TurnBytes(10));
	store.AddEditorFile(second);
	store.AddTurn(11, TurnBytes(11));
	room.clock = {.paused = false, .speed = 0.5f, .steps = 0};
	store.AddRoom(room);
	store.AddEditorFile(third);
	store.AddTurn(12, TurnBytes(12));

	const auto restored = Store(folder, 3).LoadNewest();
	ASSERT_TRUE(restored.has_value());
	EXPECT_EQ(restored->editorFiles, (std::vector<EditorFile> {first, second, third}));
	EXPECT_TRUE(restored->snapshot.editorFiles.empty());
	EXPECT_EQ(restored->turns.size(), 3u);
	EXPECT_EQ(restored->room.clock, room.clock);
	EXPECT_EQ(restored->snapshot.room.clock, MakeRoom().clock);
}

// When the newest snapshot is torn, the older snapshot and both journals still give every file
TEST_F(SnapshotStoreTest, ATornNewestSnapshotStillGivesEveryFileOfTheEditor)
{
	const EditorFile early {.kind = EditorFileKind::Mind, .bytes = {1}};
	const EditorFile middle {.kind = EditorFileKind::ScriptSource, .bytes = {2}};
	const EditorFile late {.kind = EditorFileKind::Mind, .bytes = {3}};
	const auto room = MakeRoom();
	Store store(folder, 3);
	ASSERT_TRUE(store.OpenJournal(1, room));
	ASSERT_TRUE(
	    store.Write(1, EncodeSnapshot({.turn = 1, .room = room, .world = World(1), .blocks = {}, .editorFiles = {early}})));
	store.AddEditorFile(middle);
	store.AddTurn(1, TurnBytes(1));
	ASSERT_TRUE(store.OpenJournal(2, room));
	ASSERT_TRUE(store.Write(
	    2, EncodeSnapshot({.turn = 2, .room = room, .world = World(2), .blocks = {}, .editorFiles = {early, middle}})));
	store.AddEditorFile(late);
	store.AddTurn(2, TurnBytes(2));
	const auto newest = folder / SnapshotFileName(2);
	auto bytes = ReadAll(newest);
	bytes.resize(bytes.size() / 2);
	WriteAll(newest, bytes);

	const auto restored = Store(folder, 3).LoadNewest();
	ASSERT_TRUE(restored.has_value());
	EXPECT_EQ(restored->snapshot.turn, 1u);
	EXPECT_EQ(restored->editorFiles, (std::vector<EditorFile> {early, middle, late}));
	EXPECT_EQ(restored->turns.size(), 2u);
}
