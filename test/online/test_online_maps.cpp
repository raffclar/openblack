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

#include <algorithm>
#include <fstream>
#include <functional>
#include <map>
#include <optional>
#include <random>
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
#include "Online/DownloadTestMap.h"
#include "Online/GodsRingMapFile.h"
#include "Online/HttpClientInterface.h"
#include "Online/MapDownload.h"
#include "Online/OnlineMapLand.h"
#include "Online/OnlineMapStoreInterface.h"
#include "Online/Sha256.h"
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

lobby_api::OnlineMap Map(uint64_t checksum = 0, std::string_view hash = k_Hash)
{
	return {.number = 1,
	        .name = "Gods",
	        .file = "obm_32p_1",
	        .players = 32,
	        .sha256 = std::string(hash),
	        .checksum = checksum,
	        .size = 3,
	        .url = "/api/v1/maps/" + std::string(hash)};
}

/// The map of the list for a file of this text: its start checksum and its SHA-256
lobby_api::OnlineMap MapOf(std::string_view text)
{
	return Map(online::start_rules::LandChecksum(Bytes(text)), online::sha256::HexOf(Bytes(text)));
}

/// A store in memory, which answers when the test lets it
class FakeStore final: public online::OnlineMapStoreInterface
{
public:
	void Find(std::string file, uint64_t checksum, FoundCallback done) override
	{
		const auto found = files.find(file);
		done(found != files.end() && online::start_rules::LandChecksum(found->second) == checksum);
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
	std::filesystem::create_directories(folder.Path() / "game");
	files.SetGamePath(folder.Path() / "game");
	online::FolderMapStore store(files, folder.Path() / "profile");
	const auto bytes = Bytes("map bytes");
	const auto checksum = online::start_rules::LandChecksum(bytes);

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
	std::filesystem::create_directories(folder.Path() / "game" / "Online Maps");
	std::filesystem::create_directories(folder.Path() / "game" / "Scripts");
	files.SetGamePath(folder.Path() / "game");
	// The game folder is read only for the game, so the test writes the land of the game itself
	std::ofstream(folder.Path() / "game" / "Online Maps" / "mpm_2p_1.map", std::ios::binary) << "game";
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
	download.Start(Map(online::start_rules::LandChecksum(Bytes("abc"))));
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
	download.Start(MapOf("new"));
	EXPECT_EQ(download.GetState(), online::MapDownload::State::Downloading);
	ASSERT_EQ(fetch.asked, (std::vector<std::string> {online::sha256::HexOf(Bytes("new"))}));
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
	download.Start(MapOf("x"));
	fetch.Answer(Bytes("x"));
	EXPECT_EQ(download.GetState(), online::MapDownload::State::Failed);
}

TEST(MapDownload, AFileWithADifferentChecksumIsNotKept)
{
	FakeStore store;
	FakeFetch fetch;
	online::MapDownload download(store, fetch.Function());
	download.Start(MapOf("right"));
	fetch.Answer(Bytes("wrong"));
	EXPECT_EQ(download.GetState(), online::MapDownload::State::Downloading);
	fetch.Answer(Bytes("wrong"));
	EXPECT_EQ(download.GetState(), online::MapDownload::State::Failed);
	EXPECT_TRUE(store.files.empty());
}

// The start checksum is a sum of the bytes, so the same bytes in another order have the same checksum. The SHA-256 of
// the map list finds such a file.
TEST(MapDownload, AFileWithTheChecksumOfTheMapButAnotherHashIsNotKept)
{
	ASSERT_EQ(online::start_rules::LandChecksum(Bytes("ab")), online::start_rules::LandChecksum(Bytes("ba")));
	FakeStore store;
	FakeFetch fetch;
	online::MapDownload download(store, fetch.Function());
	download.Start(MapOf("ab"));
	fetch.Answer(Bytes("ba"));
	fetch.Answer(Bytes("ba"));
	EXPECT_EQ(download.GetState(), online::MapDownload::State::Failed);
	EXPECT_TRUE(store.files.empty());
}

TEST(Sha256, TheHashOfTheStandardTextsIsTheHashOfTheStandard)
{
	using online::sha256::HexOf;
	EXPECT_EQ(HexOf(Bytes("")), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
	EXPECT_EQ(HexOf(Bytes("abc")), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
	EXPECT_EQ(HexOf(Bytes("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")),
	          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
	// The lengths at the ends of a block: the length of the text goes in the same block or in the next block
	EXPECT_EQ(HexOf(Bytes(std::string(55, 'a'))), "9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318");
	EXPECT_EQ(HexOf(Bytes(std::string(56, 'a'))), "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a");
	EXPECT_EQ(HexOf(Bytes(std::string(64, 'a'))), "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb");
	EXPECT_EQ(HexOf(Bytes(std::string(1000, 'a'))), "41edece42d63e8d9bf515a9ba6932e1c20cbc9f5a5d134645adb5db1b9737ea3");
	EXPECT_EQ(HexOf(std::vector<uint8_t> {1, 2, 3}), "039058c6f2c0cb492c533b0a4d14ef77cc0f78abccced5287d84a1a2011cfb81");
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

namespace
{

/// A small land for the tests: a few blocks, a country, a material and a coarse page
LandData SmallLand()
{
	LandData land;
	for (uint32_t i = 0; i < 3; ++i)
	{
		lnd::LNDBlock block {};
		block.blockX = 10 + i;
		block.blockZ = 12;
		block.index = i + 1;
		land.blocks.push_back(block);
		land.blockIndexLookup.at(((10 + i) * LandData::k_BlocksPerSide) + 12) = static_cast<uint16_t>(i + 1);
	}
	land.countries.resize(1);
	land.materials.resize(1);
	lnd::LNDLowResolutionTexture page {};
	page.texels.assign(64, 7);
	land.coarsePages.push_back(page);
	return land;
}

constexpr std::string_view k_TemplateScript = "VERSION(2.300000)\nLOAD_LANDSCAPE(\".\\Online Maps\\MPM_2P_1.lnd\")\n";

/// The bytes of a land file for the tests, with the small land
std::vector<uint8_t> SmallMapFile(std::string_view script = k_TemplateScript)
{
	return *onlinemap::Write(script, SmallLand().LndBytes());
}

void PutLength(std::vector<uint8_t>& bytes, size_t at, uint32_t value)
{
	for (size_t i = 0; i < 4; ++i)
	{
		bytes.at(at + i) = static_cast<uint8_t>((value >> (8 * i)) & 0xFFu);
	}
}

} // namespace

TEST(OnlineMapFile, TwoLengthsWhoseSumWrapsRoundAreNotAMap)
{
	// On a 32-bit build the sum of these lengths wraps round to the 8 bytes after the lengths
	std::vector<uint8_t> bytes(16, 0);
	PutLength(bytes, 0, 0xFFFF'FFFFu);
	PutLength(bytes, 4, 9);
	EXPECT_FALSE(onlinemap::Parse(std::as_bytes(std::span(bytes))).has_value());
}

TEST(LndFile, CountsLargerThanTheFileAreRefusedBeforeTheReaderMakesRoom)
{
	const auto good = SmallLand().LndBytes();
	{
		lnd::LNDFile file;
		ASSERT_EQ(file.Open(good), lnd::LNDResult::Success);
	}
	// The header fields after the table of 1024 bytes: material count, country count, block size, material size,
	// country size and coarse page count
	constexpr size_t k_Materials = 4 + 1024;
	constexpr size_t k_Countries = k_Materials + 4;
	constexpr size_t k_Pages = k_Materials + 20;
	constexpr size_t k_FirstPageSize = sizeof(lnd::LNDHeader) + 16;
	const auto refused = [&good](size_t at, uint32_t value) {
		auto bytes = good;
		PutLength(bytes, at, value);
		lnd::LNDFile file;
		return file.Open(bytes) != lnd::LNDResult::Success;
	};
	// No blocks, not even the first block that the file does not keep
	EXPECT_TRUE(refused(0, 0));
	EXPECT_TRUE(refused(0, 0xFFFF'FFFFu));
	EXPECT_TRUE(refused(k_Materials, 0xFFFF'FFFFu));
	EXPECT_TRUE(refused(k_Countries, 0xFFFF'FFFFu));
	EXPECT_TRUE(refused(k_Pages, 0xFFFF'FFFFu));
	// A page size that is smaller than its own 4 bytes, and a page size larger than the file
	EXPECT_TRUE(refused(k_FirstPageSize, 0));
	EXPECT_TRUE(refused(k_FirstPageSize, 0xFFFF'FFFFu));
	// A file that stops early does not read
	for (const size_t size : {size_t {0}, sizeof(lnd::LNDHeader) - 1, sizeof(lnd::LNDHeader) + 10, good.size() / 2})
	{
		lnd::LNDFile file;
		EXPECT_NE(file.Open(std::vector<uint8_t>(good.begin(), good.begin() + static_cast<std::ptrdiff_t>(size))),
		          lnd::LNDResult::Success);
	}
}

TEST(LandDataBytes, ATablePlaceThatNamesAMissingBlockHoldsNoBlock)
{
	auto land = SmallLand();
	land.blockIndexLookup.at(5) = 200;
	const auto bytes = land.LndBytes();
	lnd::LNDFile file;
	ASSERT_EQ(file.Open(bytes), lnd::LNDResult::Success);
	const auto read = LandData::FromLnd(file);
	EXPECT_EQ(read.blockIndexLookup.at(5), 0u);
	EXPECT_EQ(read.blockIndexLookup.at((11 * LandData::k_BlocksPerSide) + 12), 2u);
}

// A short run of a fuzz test: changed bytes of a land file, in its lengths, its landscape header and in other places,
// do not stop the reader of the file. A table place that the reader keeps names only a block that the land has.
TEST(OnlineMapFile, ChangedBytesOfALandFileDoNotStopTheReader)
{
	const auto seed = SmallMapFile();
	const size_t landscape = 8 + k_TemplateScript.size();
	std::mt19937 random(20261105);
	const auto pick = [&random](size_t from, size_t to) { return std::uniform_int_distribution<size_t>(from, to - 1)(random); };
	for (int run = 0; run < 400; ++run)
	{
		auto bytes = seed;
		const auto changes = 1 + pick(0, 8);
		for (size_t i = 0; i < changes; ++i)
		{
			// Most changes go into the lengths and the landscape header, which hold the counts
			size_t at = landscape + pick(0, 1100);
			if (pick(0, 4) == 0)
			{
				at = pick(0, bytes.size());
			}
			else if (pick(0, 8) == 0)
			{
				at = pick(0, 8);
			}
			bytes.at(at) = static_cast<uint8_t>(pick(0, 256));
		}
		if (pick(0, 10) == 0)
		{
			bytes.resize(pick(0, bytes.size()));
		}
		const auto parts = gods_ring_map_file::Read(bytes);
		if (parts.has_value())
		{
			for (const auto index : parts->land.blockIndexLookup)
			{
				ASSERT_LE(index, parts->land.blocks.size());
			}
		}
	}
}

TEST(OnlineMapStore, ANameOfADeviceOfWindowsIsNotAMapFile)
{
	for (const auto* name :
	     {"CON.map", "con.map", "nul.map", "AUX.map", "prn.map", "COM1.map", "lpt9.map", "CON .map", " obm.map", "obm\x01.map"})
	{
		EXPECT_FALSE(online::IsStoreFileName(name)) << name;
	}
	EXPECT_TRUE(online::IsStoreFileName("console.map"));
	EXPECT_TRUE(online::IsStoreFileName("com10.map"));
	EXPECT_TRUE(online::IsStoreFileName("obt_dl_2p_1.map"));
}

TEST(MapDownload, AMapWithANameThatTheStoreCannotKeepFailsAndLeavesNoFile)
{
	const test::TestTempFolder folder;
	filesystem::DefaultFileSystem files;
	std::filesystem::create_directories(folder.Path() / "game");
	files.SetGamePath(folder.Path() / "game");
	online::FolderMapStore store(files, folder.Path() / "profile");
	FakeFetch fetch;
	online::MapDownload download(store, fetch.Function());
	auto map = MapOf("land");
	for (const auto* name : {"CON", "../x", "a/b", "C:x", ".hidden"})
	{
		map.file = name;
		download.Start(map);
		fetch.Answer(Bytes("land"));
		EXPECT_EQ(download.GetState(), online::MapDownload::State::Failed) << name;
	}
	const auto folderOfMaps = folder.Path() / "profile" / "Online Maps";
	EXPECT_TRUE(!std::filesystem::exists(folderOfMaps) || std::filesystem::is_empty(folderOfMaps));
}

TEST(MapDownload, ABadAnswerKeepsNoFileAndAGoodSecondAnswerIsKept)
{
	const auto file = SmallMapFile();
	const auto map = Map(online::start_rules::LandChecksum(file), online::sha256::HexOf(file));
	// A changed byte, a part of the file and no answer are each a bad answer
	auto changed = file;
	changed.at(changed.size() / 2) ^= 0x01u;
	const std::vector<uint8_t> part(file.begin(), file.begin() + static_cast<std::ptrdiff_t>(file.size() / 2));
	for (const auto& bad : {std::optional(changed), std::optional(part), std::optional<std::vector<uint8_t>>()})
	{
		FakeStore store;
		FakeFetch fetch;
		online::MapDownload download(store, fetch.Function());
		download.Start(map);
		fetch.Answer(bad);
		EXPECT_EQ(download.GetState(), online::MapDownload::State::Downloading);
		EXPECT_TRUE(store.files.empty());
		fetch.Answer(file);
		EXPECT_EQ(download.GetState(), online::MapDownload::State::Ready);
		EXPECT_EQ(store.files.at("obm_32p_1.map"), file);
	}
}

TEST(DownloadTestMap, TheLandHasItsOwnNameAndTheLandscapeOfTheTemplate)
{
	const auto templateMap = SmallMapFile(
	    "VERSION(2.300000)\r\nSET_LAND_NUMBER(0)\r\n  load_landscape(\".\\Online Maps\\MPM_2P_1.lnd\")\r\nEND\r\n");
	const auto made = download_test_map::Make(templateMap);
	ASSERT_TRUE(made.has_value());
	EXPECT_NE(online::sha256::HexOf(*made), online::sha256::HexOf(templateMap));
	const auto parsed = onlinemap::Parse(std::as_bytes(std::span(*made)));
	const auto source = onlinemap::Parse(std::as_bytes(std::span(templateMap)));
	ASSERT_TRUE(parsed.has_value() && source.has_value());
	EXPECT_EQ(parsed->script, "rem openblack: the land of the map download test\r\nVERSION(2.300000)\r\nSET_LAND_NUMBER(0)\r\n"
	                          "LOAD_LANDSCAPE(\".\\Online Maps\\OBT_DL_2P_1.lnd\")\r\nEND\r\n");
	EXPECT_TRUE(std::ranges::equal(parsed->landscape, source->landscape));
	EXPECT_TRUE(
	    online_map_land::IsOwnLandscape("Online Maps/" + download_test_map::FileName(), ".\\Online Maps\\OBT_DL_2P_1.lnd"));
	EXPECT_TRUE(online::IsStoreFileName(download_test_map::FileName()));
	// The same template gives the same bytes, so every tool run gives the same land
	EXPECT_EQ(download_test_map::Make(templateMap), made);
}

TEST(DownloadTestMap, ATemplateWithNoLandscapeOrNoMapGivesNothing)
{
	EXPECT_FALSE(download_test_map::Make(SmallMapFile("VERSION(2.300000)\n")).has_value());
	EXPECT_FALSE(download_test_map::Make(Bytes("not a map")).has_value());
}

TEST(HttpLimits, TheFullTimeOfALargeTransferFollowsItsLimitOfBytes)
{
	// The time to connect, one stall, and the bytes at the stall speed
	EXPECT_EQ(online::LargeTransferTimeoutMs(0), 40'000);
	EXPECT_EQ(online::LargeTransferTimeoutMs(1024 * 1024), 40'000 + (1024 * 1000));
	EXPECT_EQ(online::LargeTransferTimeoutMs(online::k_MostLargeAnswerBytes), 40'000 + (64 * 1024 * 1000));
	EXPECT_EQ(online::LargeTransferTimeoutMs(UINT64_MAX), 0x7FFF'FFFF);
}
