/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <cstddef>
#include <cstdint>

#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <LNDFile.h>
#include <LobbyApiJson.h>
#include <OnlineMapFile.h>
#include <gtest/gtest.h>

#include "3D/GodsRingLand.h"
#include "3D/LandData.h"
#include "FileSystem/DefaultFileSystem.h"
#include "Online/GodsRingMapFile.h"
#include "Online/MapDownload.h"
#include "Online/OnlineMapStoreInterface.h"
#include "Online/StartRules.h"
#include "Server/LobbyStatus.h"

#include "../TestTempFolder.h"

using namespace openblack;

namespace
{

constexpr std::string_view k_Hash = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

std::vector<uint8_t> Bytes(std::string_view text)
{
	return {text.begin(), text.end()};
}

lobby_api::OnlineMap Map(uint64_t checksum = 0)
{
	return {.number = 1,
	        .name = "Gods",
	        .file = "obm_32p_1",
	        .players = 32,
	        .sha256 = std::string(k_Hash),
	        .checksum = checksum,
	        .size = 3,
	        .url = "/api/v1/maps/" + std::string(k_Hash)};
}

/// A store in memory, which answers when the test lets it
class FakeStore final: public online::OnlineMapStoreInterface
{
public:
	void Find(std::string file, uint64_t checksum, FoundCallback done) override
	{
		const auto found = files.find(file);
		done(found != files.end() && start_rules::LandChecksum(found->second) == checksum);
	}
	[[nodiscard]] std::optional<std::vector<uint8_t>> Read(std::string_view file) const override
	{
		const auto found = files.find(std::string(file));
		return found != files.end() ? std::optional(found->second) : std::nullopt;
	}
	void Write(std::string file, std::vector<uint8_t> bytes, WrittenCallback done) override
	{
		if (refuses)
		{
			done(false);
			return;
		}
		files[std::move(file)] = std::move(bytes);
		done(true);
	}

	std::map<std::string, std::vector<uint8_t>> files;
	bool refuses {false};
};

/// The lobby of the download: it keeps each call, and the test answers it
struct FakeFetch
{
	std::vector<std::string> asked;
	std::function<void(std::optional<std::vector<uint8_t>>)> done;

	online::MapDownload::Fetch Function()
	{
		return [this](const lobby_api::OnlineMap& map, std::function<void(std::optional<std::vector<uint8_t>>)> callback) {
			asked.push_back(map.sha256);
			done = std::move(callback);
		};
	}
	void Answer(std::optional<std::vector<uint8_t>> bytes)
	{
		auto callback = std::move(done);
		done = {};
		callback(std::move(bytes));
	}
};

} // namespace

TEST(OnlineMapFile, WriteGivesTheBytesThatParseReads)
{
	const std::vector<uint8_t> landscape {1, 2, 3, 4, 5};
	const auto file = onlinemap::Write("SCRIPT", landscape);
	ASSERT_TRUE(file.has_value());
	// The two lengths, then the script, then the landscape
	ASSERT_EQ(file->size(), 8u + 6u + 5u);
	EXPECT_EQ((*file)[0], 6u);
	EXPECT_EQ((*file)[4], 5u);
	const auto parsed = onlinemap::Parse(std::as_bytes(std::span(*file)));
	ASSERT_TRUE(parsed.has_value());
	EXPECT_EQ(parsed->script, "SCRIPT");
	ASSERT_EQ(parsed->landscape.size(), landscape.size());
	EXPECT_EQ(static_cast<uint8_t>(parsed->landscape[4]), 5u);
}

TEST(LandDataBytes, ALandOfMoreThan255BlocksReadsBackTheSame)
{
	const auto land = gods_ring_land::Build();
	ASSERT_GT(land.blocks.size(), LandData::k_MostBlocksInTable);
	const auto bytes = land.LndBytes();
	lnd::LNDFile file;
	ASSERT_EQ(file.Open(bytes), lnd::LNDResult::Success);
	const auto read = LandData::FromLnd(file);
	EXPECT_EQ(read.blockIndexLookup, land.blockIndexLookup);
	ASSERT_EQ(read.blocks.size(), land.blocks.size());
	EXPECT_EQ(read.countries.size(), land.countries.size());
	EXPECT_EQ(read.materials.size(), land.materials.size());
	EXPECT_EQ(read.coarsePages.size(), land.coarsePages.size());
	// A second write gives the same bytes, so each computer has the same file and the same checksum
	EXPECT_EQ(read.LndBytes(), bytes);
}

TEST(GodsRingMapFile, ReadGivesTheScriptAndTheLandOfWrite)
{
	const auto land = gods_ring_land::Build();
	const auto file = gods_ring_map_file::Write("LOAD_LAND", land);
	ASSERT_TRUE(file.has_value());
	const auto parts = gods_ring_map_file::Read(*file);
	ASSERT_TRUE(parts.has_value());
	EXPECT_EQ(parts->script, "LOAD_LAND");
	EXPECT_EQ(parts->land.blockIndexLookup, land.blockIndexLookup);
	EXPECT_EQ(gods_ring_map_file::Write(parts->script, parts->land), file);
	EXPECT_EQ(gods_ring_map_file::WireName(), "Online Maps/obm_32p_1.map");
	EXPECT_FALSE(gods_ring_map_file::Read(Bytes("not a map")).has_value());
}

TEST(OnlineMapStore, OnlyPlainMapNamesGoInTheStore)
{
	EXPECT_TRUE(online::IsStoreFileName("obm_32p_1.map"));
	EXPECT_TRUE(online::IsStoreFileName("My Land-2.map"));
	EXPECT_FALSE(online::IsStoreFileName(""));
	EXPECT_FALSE(online::IsStoreFileName("obm_32p_1.txt"));
	EXPECT_FALSE(online::IsStoreFileName("../obm_32p_1.map"));
	EXPECT_FALSE(online::IsStoreFileName("a/b.map"));
	EXPECT_FALSE(online::IsStoreFileName(".map"));
	EXPECT_FALSE(online::IsStoreFileName(std::string(70, 'a') + ".map"));
}

TEST(OnlineMapStore, TheFolderStoreKeepsMapsInTheProfileFolder)
{
	const test::TestTempFolder folder;
	filesystem::DefaultFileSystem files;
	files.SetGamePath(folder.Path() / "game");
	online::FolderMapStore store(files, folder.Path() / "profile");
	const auto bytes = Bytes("map bytes");
	const auto checksum = start_rules::LandChecksum(bytes);

	std::optional<bool> found;
	store.Find("obm_32p_1.map", checksum, [&found](bool answer) { found = answer; });
	EXPECT_EQ(found, false);

	std::optional<bool> written;
	store.Write("obm_32p_1.map", bytes, [&written](bool answer) { written = answer; });
	EXPECT_EQ(written, true);
	EXPECT_TRUE(std::filesystem::exists(folder.Path() / "profile" / "Online Maps" / "obm_32p_1.map"));
	EXPECT_EQ(store.Read("obm_32p_1.map"), bytes);

	store.Find("obm_32p_1.map", checksum, [&found](bool answer) { found = answer; });
	EXPECT_EQ(found, true);
	// A file with a different checksum is not the map of the room
	store.Find("obm_32p_1.map", checksum + 1, [&found](bool answer) { found = answer; });
	EXPECT_EQ(found, false);

	// A name that goes out of the folder is refused
	store.Write("../x.map", bytes, [&written](bool answer) { written = answer; });
	EXPECT_EQ(written, false);
}

TEST(OnlineMapStore, AStoreWithNoProfileFolderKeepsNothing)
{
	filesystem::DefaultFileSystem files;
	online::FolderMapStore store(files, std::nullopt);
	std::optional<bool> written;
	store.Write("obm_32p_1.map", Bytes("x"), [&written](bool answer) { written = answer; });
	EXPECT_EQ(written, false);
	EXPECT_FALSE(store.Read("obm_32p_1.map").has_value());
}

TEST(OnlineMapStore, TheLandOfTheStartComesFromTheStoreFirst)
{
	const test::TestTempFolder folder;
	filesystem::DefaultFileSystem files;
	files.SetGamePath(folder.Path() / "game");
	std::filesystem::create_directories(folder.Path() / "game" / "Online Maps");
	std::filesystem::create_directories(folder.Path() / "game" / "Scripts");
	ASSERT_TRUE(files.WriteAll(folder.Path() / "game" / "Online Maps" / "mpm_2p_1.map", Bytes("game")));
	FakeStore store;
	store.files["obm_32p_1.map"] = Bytes("sent");

	EXPECT_EQ(online::ReadStartLand("Online Maps/obm_32p_1.map", &store, files), Bytes("sent"));
	// A land that the store does not have comes from the game folder
	EXPECT_EQ(online::ReadStartLand("Online Maps/mpm_2p_1.map", &store, files), Bytes("game"));
	EXPECT_EQ(online::ReadStartLand("Online Maps/mpm_2p_1.map", nullptr, files), Bytes("game"));
	EXPECT_FALSE(online::ReadStartLand("Online Maps/obm_32p_1.map", nullptr, files).has_value());
	EXPECT_FALSE(online::ReadStartLand("../obm_32p_1.map", &store, files).has_value());
}

TEST(MapDownload, AMapInTheStoreIsReadyWithNoDownload)
{
	FakeStore store;
	store.files["obm_32p_1.map"] = Bytes("abc");
	FakeFetch fetch;
	online::MapDownload download(store, fetch.Function());
	download.Start(Map(start_rules::LandChecksum(Bytes("abc"))));
	EXPECT_EQ(download.GetState(), online::MapDownload::State::Ready);
	EXPECT_TRUE(fetch.asked.empty());
}

TEST(MapDownload, AMissingMapComesFromTheLobbyAndGoesInTheStore)
{
	FakeStore store;
	// The store has an older file with the same name: its checksum is not the room's
	store.files["obm_32p_1.map"] = Bytes("old");
	FakeFetch fetch;
	online::MapDownload download(store, fetch.Function());
	download.Start(Map(start_rules::LandChecksum(Bytes("new"))));
	EXPECT_EQ(download.GetState(), online::MapDownload::State::Downloading);
	ASSERT_EQ(fetch.asked, (std::vector<std::string> {std::string(k_Hash)}));
	fetch.Answer(Bytes("new"));
	EXPECT_EQ(download.GetState(), online::MapDownload::State::Ready);
	EXPECT_EQ(store.files.at("obm_32p_1.map"), Bytes("new"));
}

TEST(MapDownload, TheLobbyIsAskedTwoTimesBeforeTheDownloadFails)
{
	FakeStore store;
	FakeFetch fetch;
	online::MapDownload download(store, fetch.Function());
	download.Start(Map());
	fetch.Answer(std::nullopt);
	EXPECT_EQ(download.GetState(), online::MapDownload::State::Downloading);
	EXPECT_EQ(fetch.asked.size(), 2u);
	fetch.Answer(std::nullopt);
	EXPECT_EQ(download.GetState(), online::MapDownload::State::Failed);
	EXPECT_EQ(fetch.asked.size(), 2u);
}

TEST(MapDownload, AStoreThatCannotKeepTheFileFails)
{
	FakeStore store;
	store.refuses = true;
	FakeFetch fetch;
	online::MapDownload download(store, fetch.Function());
	download.Start(Map());
	fetch.Answer(Bytes("x"));
	EXPECT_EQ(download.GetState(), online::MapDownload::State::Failed);
}

TEST(MapDownload, AnAnswerAfterTheStopDoesNothing)
{
	FakeStore store;
	FakeFetch fetch;
	online::MapDownload download(store, fetch.Function());
	download.Start(Map());
	download.Stop();
	EXPECT_EQ(download.GetState(), online::MapDownload::State::Idle);
	fetch.Answer(Bytes("x"));
	EXPECT_EQ(download.GetState(), online::MapDownload::State::Idle);
	EXPECT_TRUE(store.files.empty());
}

TEST(MapDownload, TheRoomsMapIsFoundByItsHash)
{
	const std::vector maps {Map()};
	EXPECT_EQ(online::MapDownload::FileOf(maps[0]), "obm_32p_1.map");
	ASSERT_TRUE(online::MapDownload::MapOfRoom(maps, k_Hash).has_value());
	EXPECT_FALSE(online::MapDownload::MapOfRoom(maps, "").has_value());
	EXPECT_FALSE(online::MapDownload::MapOfRoom(maps, std::string(64, 'f')).has_value());
}

TEST(LobbyMaps, TheListOfMapsAndTheRoomsMap)
{
	const std::string text = R"({"maps":[{"number":1,"name":"Gods","file":"obm_32p_1","players":32,"sha256":")" +
	                         std::string(k_Hash) + R"(","checksum":"00000000deadbeef","size":1234,"url":"/api/v1/maps/x"}]})";
	const auto list = lobby_api::ParseMapList(text);
	ASSERT_TRUE(list.has_value());
	ASSERT_EQ(list->maps.size(), 1u);
	const auto& map = list->maps[0];
	EXPECT_EQ(map.file, "obm_32p_1");
	EXPECT_EQ(map.players, 32);
	EXPECT_EQ(map.sha256, k_Hash);
	EXPECT_EQ(map.checksum, 0xdeadbeefu);
	EXPECT_EQ(map.size, 1234);
	// A hash that is not 64 hex digits, or a checksum that is not hex, makes the list unreadable
	EXPECT_FALSE(lobby_api::ParseMapList(R"({"maps":[{"number":1,"name":"G","file":"f","players":2,"sha256":"x",)"
	                                     R"("checksum":"1","size":1,"url":"/u"}]})")
	                 .has_value());
	EXPECT_FALSE(lobby_api::ParseMapList(R"({"maps":[{"number":1,"name":"G","file":"f","players":2,"sha256":")" +
	                                     std::string(k_Hash) + R"(","checksum":"zz","size":1,"url":"/u"}]})")
	                 .has_value());
	EXPECT_TRUE(lobby_api::ParseMapList(R"({"maps":[]})").has_value());
}

TEST(LobbyMaps, TheAddressOfTheServersMap)
{
	const server::lobby_status::CustomMap map {.file = "obm_32p_1", .name = "Gods' ring", .players = 32, .bytes = {}};
	EXPECT_EQ(server::lobby_status::MapAddress("https://lobby.test/api/v1/", "s 1", map),
	          "https://lobby.test/api/v1/servers/s%201/map?file=obm_32p_1&name=Gods%27%20ring&players=32");
}
