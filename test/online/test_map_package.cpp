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

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <LobbyApiJson.h>
#include <MapPackageFile.h>
#include <OnlineMapFile.h>
#include <gtest/gtest.h>

#include "Online/GodsRingMap.h"
#include "Online/GodsRingMapFile.h"
#include "Online/GodsRingMapPackage.h"
#include "Online/Sha256.h"
#include "Server/LobbyStatus.h"

using namespace openblack;

namespace
{
using Failure = gods_ring_map_package::Failure;

constexpr std::string_view k_Hash = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

/// A small land in the form of a network land, with a village for the first god. Its citadel is east of the middle of
/// the map for 32 gods.
constexpr std::string_view k_TemplateScript = "VERSION(2.300000)\r\n"
                                              "SET_LAND_NUMBER(0)\r\n"
                                              "LOAD_LANDSCAPE(\".\\Online Maps\\TEST.lnd\")\r\n"
                                              "SET_GLOBAL_LAND_BALANCE(4, 2.000000)\r\n"
                                              "\r\n"
                                              "rem ---------------- Town: 0 ----------------\r\n"
                                              "CREATE_TOWN(0, \"3100.00,2560.00\", \"PLAYER_ONE\", 0, \"CELTIC\")\r\n"
                                              "SET_TOWN_BELIEF_CAP(0, \"PLAYER_ONE\", 2.000000)\r\n"
                                              "CREATE_ABODE(0, \"3090.00,2600.00\", \"CELTIC_ABODE_A\", 6000, 1000, 0, 0)\r\n"
                                              "CREATE_CITADEL(\"3160.00,2560.00\", 0, \"PLAYER_ONE\", 0, 1000)\r\n"
                                              "\r\n"
                                              "rem ---------------- Town: 1 ----------------\r\n"
                                              "CREATE_TOWN(1, \"2000.00,2560.00\", \"PLAYER_TWO\", 0, \"CELTIC\")\r\n"
                                              "CREATE_CITADEL(\"1960.00,2560.00\", 0, \"PLAYER_TWO\", 3142, 1000)\r\n"
                                              "CREATE_WEATHER_CLIMATE(0, 0, \"2560.00,2560.00\", 5120.000000, 5120.000000)\r\n";

std::vector<uint8_t> TemplateMap(std::string_view script = k_TemplateScript)
{
	const std::vector<uint8_t> landscape {1, 2, 3};
	return onlinemap::Write(script, landscape).value_or(std::vector<uint8_t> {});
}

const std::vector<uint8_t>& Package()
{
	static const auto package = gods_ring_map_package::Make();
	return package;
}

std::optional<Failure> FailureOf(const std::variant<std::vector<uint8_t>, Failure>& result)
{
	if (const auto* failure = std::get_if<Failure>(&result); failure != nullptr)
	{
		return *failure;
	}
	return std::nullopt;
}

lobby_api::OnlineMap MapOfList(std::string_view packageSha256)
{
	lobby_api::OnlineMap map;
	map.file = std::string(gods_ring_map::k_File);
	map.packageSha256 = std::string(packageSha256);
	map.packageSize = static_cast<int64_t>(Package().size());
	map.packageUrl = "/maps/" + std::string(packageSha256) + ".obmap";
	return map;
}
} // namespace

TEST(MapPackageFile, ReadGivesThePartsOfWrite)
{
	const onlinemap::package::MapPackage package {
	    .manifest = {.file = "obm_test",
	                 .name = "Test",
	                 .players = 4,
	                 .needs = {{.file = "Online Maps/mpm_4p_1.map", .god = "PLAYER_ONE"}},
	                 .frameSha256 = std::string(k_Hash),
	                 .landscapeSha256 = std::string(k_Hash)},
	    .frame = "rem frame\n",
	    .landscape = {9, 8, 7, 0},
	};
	const auto bytes = onlinemap::package::Write(package);
	ASSERT_TRUE(bytes.has_value());
	const auto read = onlinemap::package::Read(std::as_bytes(std::span(*bytes)));
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(read->manifest, package.manifest);
	EXPECT_EQ(read->frame, package.frame);
	EXPECT_EQ(read->landscape, package.landscape);
	// The same package gives the same bytes
	EXPECT_EQ(onlinemap::package::Write(*read), bytes);
}

TEST(MapPackageFile, BytesThatAreNotAPackageDoNotRead)
{
	const onlinemap::package::MapPackage package {.manifest = {.file = "f", .name = "n", .players = 2}, .frame = "x"};
	const auto bytes = onlinemap::package::Write(package).value_or(std::vector<uint8_t> {});
	ASSERT_FALSE(bytes.empty());
	EXPECT_TRUE(onlinemap::package::Read(std::as_bytes(std::span(bytes))).has_value());

	auto otherMagic = bytes;
	otherMagic[0] = 'X';
	EXPECT_FALSE(onlinemap::package::Read(std::as_bytes(std::span(otherMagic))).has_value());
	auto otherFormat = bytes;
	otherFormat[8] = 2;
	EXPECT_FALSE(onlinemap::package::Read(std::as_bytes(std::span(otherFormat))).has_value());
	auto shorter = bytes;
	shorter.pop_back();
	EXPECT_FALSE(onlinemap::package::Read(std::as_bytes(std::span(shorter))).has_value());
	auto longer = bytes;
	longer.push_back(0);
	EXPECT_FALSE(onlinemap::package::Read(std::as_bytes(std::span(longer))).has_value());
	// A manifest that is not JSON
	auto badManifest = bytes;
	badManifest[24] = '[';
	EXPECT_FALSE(onlinemap::package::Read(std::as_bytes(std::span(badManifest))).has_value());
	EXPECT_FALSE(onlinemap::package::Read({}).has_value());
}

TEST(GodsRingMapPackage, ThePackageHasNoLineOfTheGameData)
{
	const auto package = onlinemap::package::Read(std::as_bytes(std::span(Package())));
	ASSERT_TRUE(package.has_value());
	EXPECT_EQ(package->manifest.file, "obm_32p_1");
	EXPECT_EQ(package->manifest.players, 32);
	ASSERT_EQ(package->manifest.needs.size(), 1u);
	EXPECT_EQ(package->manifest.needs.front().file, "Online Maps/mpm_4p_1.map");
	EXPECT_EQ(package->manifest.needs.front().god, "PLAYER_ONE");
	// The frame has only lines that openblack writes: the mark lines and the slot lines
	size_t start = 0;
	size_t lines = 0;
	while (start < package->frame.size())
	{
		const auto end = package->frame.find('\n', start);
		const auto line = std::string_view(package->frame).substr(start, end - start);
		start = end + 1;
		++lines;
		EXPECT_TRUE(line.empty() || line.starts_with("rem ")) << line;
	}
	EXPECT_EQ(lines, 1 + (32 * 3) + 2);
	// The same code gives the same package, and the name of the file is its hash
	EXPECT_EQ(gods_ring_map_package::Make(), Package());
	EXPECT_EQ(gods_ring_map_package::FileName(Package()), online::sha256::HexOf(Package()) + ".obmap");
}

TEST(GodsRingMapPackage, ThePackageAndTheGameDataGiveTheFileOfTheServer)
{
	const auto templateMap = TemplateMap();
	const auto server = gods_ring_map_file::Make(templateMap);
	ASSERT_TRUE(server.has_value());
	const auto assembled = gods_ring_map_package::Assemble(Package(), templateMap);
	ASSERT_FALSE(FailureOf(assembled).has_value());
	EXPECT_EQ(std::get<std::vector<uint8_t>>(assembled), *server);
}

TEST(GodsRingMapPackage, ADamagedPackageOrOtherGameDataGivesNoFile)
{
	const auto templateMap = TemplateMap();
	auto damaged = Package();
	damaged[damaged.size() - 10] ^= 0x01U;
	EXPECT_EQ(FailureOf(gods_ring_map_package::Assemble(damaged, templateMap)), Failure::NotAPackage);
	EXPECT_EQ(FailureOf(gods_ring_map_package::Assemble(TemplateMap(), templateMap)), Failure::NotAPackage);

	// A land with no village for the first god, and bytes that are not a land
	const auto noVillage = TemplateMap("VERSION(2.300000)\r\nCREATE_WEATHER_CLIMATE(0, 0, \"0.00,0.00\", 1.0, 1.0)\r\n");
	EXPECT_EQ(FailureOf(gods_ring_map_package::Assemble(Package(), noVillage)), Failure::GameDataDoesNotMatch);
	const std::vector<uint8_t> notALand {1, 2, 3};
	EXPECT_EQ(FailureOf(gods_ring_map_package::Assemble(Package(), notALand)), Failure::GameDataDoesNotMatch);

	// A package of another map
	auto other = onlinemap::package::Read(std::as_bytes(std::span(Package())));
	ASSERT_TRUE(other.has_value());
	other->manifest.file = "obm_other";
	const auto otherBytes = onlinemap::package::Write(*other).value_or(std::vector<uint8_t> {});
	EXPECT_EQ(FailureOf(gods_ring_map_package::Assemble(otherBytes, templateMap)), Failure::OtherMap);
}

TEST(GodsRingMapPackage, ADownloadedPackageMustHaveTheHashOfTheList)
{
	const auto templateMap = TemplateMap();
	const auto hash = online::sha256::HexOf(Package());
	const auto good =
	    gods_ring_map_package::AssembleDownloaded(MapOfList(hash), Package(), std::span<const uint8_t>(templateMap));
	EXPECT_FALSE(FailureOf(good).has_value());
	EXPECT_EQ(FailureOf(gods_ring_map_package::AssembleDownloaded(MapOfList(k_Hash), Package(),
	                                                              std::span<const uint8_t>(templateMap))),
	          Failure::WrongPackage);
	EXPECT_EQ(FailureOf(gods_ring_map_package::AssembleDownloaded(MapOfList(hash), Package(), std::nullopt)),
	          Failure::NoGameData);
	auto otherFile = MapOfList(hash);
	otherFile.file = "mpm_2p_1";
	EXPECT_EQ(FailureOf(gods_ring_map_package::AssembleDownloaded(otherFile, Package(), std::span<const uint8_t>(templateMap))),
	          Failure::OtherMap);
}

TEST(GodsRingMapPackage, AFrameWithASlotThatIsNotOnTheMapGivesNoScript)
{
	const auto slots = gods_ring_map::LayOutSlots({2560.0f, 2560.0f});
	const auto village =
	    gods_ring_map::ReadTemplate(k_TemplateScript, gods_ring_map::k_TemplateGod, {2560.0f, 2560.0f}).value();
	EXPECT_EQ(gods_ring_map::FillFrame(gods_ring_map::ScriptFrame(slots), village, slots),
	          gods_ring_map::MapScript(village, slots));
	EXPECT_FALSE(gods_ring_map::FillFrame("rem openblack: village 99\n", village, slots).has_value());
	EXPECT_FALSE(gods_ring_map::FillFrame("rem openblack: village x\n", village, slots).has_value());
}

TEST(GodsRingMapPackage, TheListGivesThePackageOfAMap)
{
	const std::string common = R"({"maps":[{"number":1,"name":"Gods","file":"obm_32p_1","players":32,"sha256":")" +
	                           std::string(k_Hash) + R"(","checksum":"00000000deadbeef","size":1234,"url":"/api/v1/maps/x")";
	const auto list =
	    lobby_api::ParseMapList(common + R"(,"package_sha256":")" + std::string(k_Hash) +
	                            R"(","package_size":99,"package_url":"/maps/)" + std::string(k_Hash) + R"(.obmap"}]})");
	ASSERT_TRUE(list.has_value());
	ASSERT_EQ(list->maps.size(), 1u);
	EXPECT_EQ(list->maps[0].packageSha256, k_Hash);
	EXPECT_EQ(list->maps[0].packageSize, 99);
	EXPECT_EQ(list->maps[0].packageUrl, "/maps/" + std::string(k_Hash) + ".obmap");
	// A map with no package, or with a null package
	const auto none = lobby_api::ParseMapList(common + "}]}");
	ASSERT_TRUE(none.has_value());
	EXPECT_TRUE(none->maps[0].packageSha256.empty());
	EXPECT_TRUE(
	    lobby_api::ParseMapList(common + R"(,"package_sha256":null,"package_size":null,"package_url":null}]})").has_value());
	// A package at a place outside the folder of maps, or with a bad hash or size, makes the list unreadable
	EXPECT_FALSE(lobby_api::ParseMapList(common + R"(,"package_sha256":")" + std::string(k_Hash) +
	                                     R"(","package_size":99,"package_url":"https://other.test/x.obmap"}]})")
	                 .has_value());
	EXPECT_FALSE(
	    lobby_api::ParseMapList(common + R"(,"package_sha256":"x","package_size":99,"package_url":"/maps/x"}]})").has_value());
	EXPECT_FALSE(lobby_api::ParseMapList(common + R"(,"package_sha256":")" + std::string(k_Hash) +
	                                     R"(","package_size":0,"package_url":"/maps/x"}]})")
	                 .has_value());
}

TEST(GodsRingMapPackage, TheServerTellsTheLobbyThePackageOfItsMap)
{
	const server::lobby_status::CustomMap map {.file = "obm_32p_1",
	                                           .name = "Ring",
	                                           .players = 32,
	                                           .bytes = {},
	                                           .packageSha256 = std::string(k_Hash),
	                                           .packageSize = 1234};
	EXPECT_EQ(server::lobby_status::MapAddress("https://lobby.test/api/v1", "s1", map),
	          "https://lobby.test/api/v1/servers/s1/map?file=obm_32p_1&name=Ring&players=32&package=" + std::string(k_Hash) +
	              "&package_size=1234");
}
