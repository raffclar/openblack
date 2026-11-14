/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <Online/DesyncFacts.h>
#include <Online/GameDataPrint.h>
#include <gtest/gtest.h>

using namespace openblack::online;
using namespace openblack::online::game_data;

namespace
{

FilePrint File(std::string path, uint64_t size, uint64_t hash)
{
	return {.path = std::move(path), .present = true, .size = size, .hash = hash};
}

openblack::game_socket::SyncFacts Facts()
{
	return {.turn = 1,
	        .build = "0.1.0+abc/codec35",
	        .land = "Online Maps/mpm_2p_1.txt",
	        .landSha256 = std::string(64, 'a'),
	        .map = {},
	        .seed = 7,
	        .data = 0x1234,
	        .patch = "1.20",
	        .language = "0809"};
}

} // namespace

TEST(GameDataPrintTest, TheHashOfNoBytesIsTheOffsetAndOneByteChangesIt)
{
	const std::vector<uint8_t> a {1, 2, 3};
	const std::vector<uint8_t> b {1, 2, 4};
	EXPECT_EQ(HashOf({}), 14695981039346656037ull);
	EXPECT_NE(HashOf(a), HashOf(b));
	EXPECT_EQ(HashOf(a), HashOf(std::vector<uint8_t> {1, 2, 3}));
}

TEST(GameDataPrintTest, APrintPathIsRelativeWithNoClimbOrDrive)
{
	EXPECT_TRUE(IsPrintPath("Data/info.dat"));
	EXPECT_TRUE(IsPrintPath("Scripts/Online Maps/mpm_2p_1.txt"));
	EXPECT_FALSE(IsPrintPath(""));
	EXPECT_FALSE(IsPrintPath("/etc/passwd"));
	EXPECT_FALSE(IsPrintPath("../secret"));
	EXPECT_FALSE(IsPrintPath("Data/../../x"));
	EXPECT_FALSE(IsPrintPath("C:/Windows/x"));
	EXPECT_FALSE(IsPrintPath("Data\\info.dat"));
	EXPECT_FALSE(IsPrintPath("Data//info.dat"));
	EXPECT_FALSE(IsPrintPath(std::string(k_MostPrintPathBytes + 1, 'a')));
}

TEST(GameDataPrintTest, ThePathsOfAPrintAreInOrderOnceAndBounded)
{
	std::vector<std::filesystem::path> read {"Scripts/b.txt", "Data/info.dat", "Scripts/b.txt", "../out.txt"};
	const auto paths = PrintPaths(read);
	ASSERT_EQ(paths.size(), 2U);
	EXPECT_EQ(paths[0], "Data/info.dat");
	EXPECT_EQ(paths[1], "Scripts/b.txt");

	std::vector<std::filesystem::path> many;
	for (size_t i = 0; i < k_MostPrintFiles + 10; ++i)
	{
		many.emplace_back("Data/f" + std::to_string(1000 + i));
	}
	EXPECT_EQ(PrintPaths(many).size(), k_MostPrintFiles);
}

TEST(GameDataPrintTest, AFileThatIsNotThereHasNoSizeOrHash)
{
	const auto missing = PrintOf("Data/info.dat", std::nullopt);
	EXPECT_FALSE(missing.present);
	EXPECT_EQ(missing.size, 0U);
	EXPECT_EQ(missing.hash, 0U);
	const auto present = PrintOf("Data/info.dat", std::vector<uint8_t> {9, 9});
	EXPECT_TRUE(present.present);
	EXPECT_EQ(present.size, 2U);
	EXPECT_EQ(present.hash, HashOf(std::vector<uint8_t> {9, 9}));
}

TEST(GameDataPrintTest, TheHintsComeFromThePatchNotesAndTheSoundBanks)
{
	const std::vector<std::string> names {"readme-patch-v110.txt", "README-PATCH-V120.TXT", "00000809.16", "Data",
	                                      "runblack.exe"};
	const auto hints = HintsOf(names);
	EXPECT_EQ(hints.patch, "1.20");
	EXPECT_EQ(hints.language, "0809");
	EXPECT_EQ(HintsOf(std::vector<std::string> {"Data"}), InstallHints {});
}

TEST(GameDataPrintTest, TheFirstDifferenceFollowsTheOrderOfTheServer)
{
	const std::vector<FilePrint> server {File("Data/info.dat", 10, 1), File("Landscape/x.lnd", 20, 2)};
	EXPECT_FALSE(FirstDifference(server, server).has_value());

	// A file that only the player has does not count
	auto more = server;
	more.push_back(File("Data/extra.txt", 1, 1));
	EXPECT_FALSE(FirstDifference(server, more).has_value());

	auto other = server;
	other[1].hash = 3;
	const auto different = FirstDifference(server, other);
	ASSERT_TRUE(different.has_value());
	EXPECT_EQ(different->kind, DifferenceKind::Different);
	EXPECT_EQ(different->server.path, "Landscape/x.lnd");
	EXPECT_NE(Describe(*different).find("Landscape/x.lnd is not the server's"), std::string::npos);

	auto missing = server;
	missing[0] = PrintOf("Data/info.dat", std::nullopt);
	const auto gone = FirstDifference(server, missing);
	ASSERT_TRUE(gone.has_value());
	EXPECT_EQ(gone->kind, DifferenceKind::Missing);
	EXPECT_EQ(gone->server.path, "Data/info.dat");

	const std::vector<FilePrint> partial {server[0]};
	const auto notGiven = FirstDifference(server, partial);
	ASSERT_TRUE(notGiven.has_value());
	EXPECT_EQ(notGiven->kind, DifferenceKind::NotGiven);
}

TEST(GameDataPrintTest, OnlyTheLandsScriptsAndTheTableOfObjectsRefuseAJoin)
{
	EXPECT_TRUE(RefusesJoin("Data/info.dat"));
	EXPECT_TRUE(RefusesJoin("data/INFO.DAT"));
	EXPECT_TRUE(RefusesJoin("Data/Landscape/Land1.lnd"));
	EXPECT_TRUE(RefusesJoin("Scripts/challenge.chl"));
	EXPECT_TRUE(RefusesJoin("Scripts/Online Maps/mpm_2p_1.txt"));
	EXPECT_FALSE(RefusesJoin("Data/Text/english.txt"));
	EXPECT_FALSE(RefusesJoin("Data/AllMeshes.g3d"));
}

TEST(GameDataPrintTest, TheHashOfAPrintChangesWithEachFile)
{
	const std::vector<FilePrint> a {File("Data/info.dat", 10, 1)};
	std::vector<FilePrint> b {File("Data/info.dat", 10, 2)};
	EXPECT_NE(HashOfPrint(a), HashOfPrint(b));
	b[0].hash = 1;
	EXPECT_EQ(HashOfPrint(a), HashOfPrint(b));
}

TEST(DesyncFactsTest, TheFirstFactThatDiffersFollowsTheOrder)
{
	const auto server = Facts();
	EXPECT_FALSE(desync_facts::FirstDifferent(server, server).has_value());

	auto player = server;
	player.seed = 8;
	player.data = 0x9999;
	const auto data = desync_facts::FirstDifferent(server, player);
	ASSERT_TRUE(data.has_value());
	EXPECT_EQ(data->fact, "data");
	EXPECT_EQ(data->words, "the player has other game data");

	player.build = "0.1.0+def/codec35";
	EXPECT_EQ(desync_facts::FirstDifferent(server, player)->fact, "build");

	// A hash that one side does not know does not count
	auto unknown = server;
	unknown.data = 0;
	unknown.landSha256.clear();
	unknown.seed = 9;
	EXPECT_EQ(desync_facts::FirstDifferent(server, unknown)->fact, "seed");

	// The turn, the patch and the language do not count
	auto hints = server;
	hints.turn = 99;
	hints.patch = "1.10";
	hints.language = "0407";
	EXPECT_FALSE(desync_facts::FirstDifferent(server, hints).has_value());
}

TEST(DesyncFactsTest, TheListedFactsHaveEachName)
{
	const auto listed = desync_facts::Listed(Facts());
	ASSERT_EQ(listed.size(), 9U);
	EXPECT_EQ(listed[0].first, "turn");
	EXPECT_EQ(listed[2].second, "0000000000001234");
}

TEST(GameDataPrintTest, EachPathHasAKind)
{
	EXPECT_EQ(KindOfPath("Scripts/info.dat"), FileKind::Core);
	EXPECT_EQ(KindOfPath("Online Maps/mpm_2p_1.map"), FileKind::Core);
	EXPECT_EQ(KindOfPath("Data/Landscape/Land1.LND"), FileKind::Core);
	EXPECT_EQ(KindOfPath("Data/AllAnims.anm"), FileKind::Core);
	EXPECT_EQ(KindOfPath("Data/CTR/Creature.CBN"), FileKind::Core);
	EXPECT_EQ(KindOfPath("Scripts/Dance/dance1"), FileKind::Core);
	EXPECT_EQ(KindOfPath("Data/CreatureMesh/C_Cow.l3d"), FileKind::Sim);
	EXPECT_EQ(KindOfPath("Data/AllMeshes.g3d"), FileKind::Sim);
	EXPECT_EQ(KindOfPath("Audio/Music/align/a.sad"), FileKind::Presentation);
	EXPECT_EQ(KindOfPath("Data/Textures/t.RAW"), FileKind::Presentation);
	EXPECT_EQ(KindOfPath("Data/Spells/fall/x.bik"), FileKind::Presentation);
}

TEST(GameDataPrintTest, ThePrintHasTheCoreFilesFirstAndNoPresentationFiles)
{
	std::vector<std::filesystem::path> read {"Audio/SFX/a.sad", "Data/CreatureMesh/a.l3d", "Scripts/info.dat",
	                                         "Data/Textures/t.raw", "Online Maps/m.map"};
	for (size_t i = 0; i < k_MostPrintFiles; ++i)
	{
		read.emplace_back("Data/CreatureMesh/m" + std::to_string(1000 + i) + ".l3d");
	}
	const auto paths = PrintPaths(read);
	ASSERT_EQ(paths.size(), k_MostPrintFiles);
	EXPECT_EQ(paths[0], "Online Maps/m.map");
	EXPECT_EQ(paths[1], "Scripts/info.dat");
	EXPECT_EQ(paths[2], "Data/CreatureMesh/a.l3d");
	EXPECT_TRUE(
	    std::ranges::none_of(paths, [](const std::string& path) { return KindOfPath(path) == FileKind::Presentation; }));
}
