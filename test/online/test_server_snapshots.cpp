/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include "Server/ServerSnapshots.h"

using namespace openblack::server::snapshots;

namespace
{

Room MakeRoom()
{
	return Room {.start = R"({"type":"room.start","land":"Land1.txt"})",
	             .seats = {{.state = 1, .session = "secret-1", .account = "11", .name = "alice"},
	                       {.state = 2, .session = "secret-2", .account = "12", .name = "bob"},
	                       {.state = 0, .session = {}, .account = {}, .name = {}}},
	             .watchers = {"secret-3"},
	             .stoppedAfter = std::nullopt,
	             .leaving = {1},
	             .joining = {}};
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
		ASSERT_TRUE(
		    store.Write(snapshotTurn, EncodeSnapshot({.turn = snapshotTurn, .room = room, .world = World(snapshotTurn)})));
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

TEST(ServerSnapshots, ASnapshotReadsBackAndATornOrChangedSnapshotDoesNot)
{
	const Snapshot snapshot {.turn = 300, .room = MakeRoom(), .world = World(300)};
	const auto bytes = EncodeSnapshot(snapshot);
	const auto read = DecodeSnapshot(bytes);
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(read->turn, 300u);
	EXPECT_EQ(read->room, snapshot.room);
	EXPECT_EQ(read->world, snapshot.world);

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
	EXPECT_EQ(turn->first, 6u);
	EXPECT_EQ(turn->second, TurnBytes(6));

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
