/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <tuple>
#include <vector>

#include <ChallengePictureFiles.h>
#include <SavePictureMaker.h>
#include <SavePictures.h>
#include <gtest/gtest.h>

using namespace openblack::gamesave;

namespace
{
/// The first 128 bytes of the game's challenge picture files, as words. Word 10 is the place in the game's memory of
/// the picture, which changes from file to file.
constexpr std::array<uint32_t, 32> k_GameHeader {
    0x00008080, 0x0000007C, 0x00081007, 0x00000100, 0x00000100, 0x00008000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x2D5D29B0, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000020, 0x00000004, 0x31545844, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00001800, 0x00000000, 0x00000000, 0x00000000, 0x00000000};
constexpr size_t k_MemoryWordAt = 0x28;
constexpr size_t k_HeaderSize = 128;
/// A block of one colour: magenta, as the game takes it on a screen that it cannot read
constexpr std::array<uint8_t, 8> k_MagentaBlock {0x1F, 0xF8, 0x1F, 0xF8, 0x00, 0x00, 0x00, 0x00};
constexpr std::array<uint8_t, 8> k_BlackBlock {};
constexpr uint32_t k_BlocksAcross = 64;

std::vector<uint8_t> Picture(uint8_t red, uint8_t green, uint8_t blue)
{
	std::vector<uint8_t> picture(static_cast<size_t>(256) * 128 * 3);
	for (size_t i = 0; i < picture.size(); i += 3)
	{
		picture[i] = red;
		picture[i + 1] = green;
		picture[i + 2] = blue;
	}
	return picture;
}

std::vector<uint8_t> ReadAll(const std::filesystem::path& path)
{
	std::ifstream stream(path, std::ios::binary);
	return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

void WriteAll(const std::filesystem::path& path, std::span<const uint8_t> bytes)
{
	std::ofstream stream(path, std::ios::binary);
	stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

/// Compares a file with the game's file of the same layout. The word of the place in memory is not compared.
void ExpectGameFile(std::span<const uint8_t> ours, std::span<const uint8_t> game, const std::string& what)
{
	ASSERT_EQ(ours.size(), game.size()) << what;
	for (size_t i = 0; i < ours.size(); ++i)
	{
		if (i < k_MemoryWordAt || i >= k_MemoryWordAt + 4)
		{
			ASSERT_EQ(ours[i], game[i]) << what << " byte " << i;
		}
	}
}

/// The game's file after two challenges took a magenta picture: the blocks of the rows of blocks before firstBlack are
/// magenta and the rest are black
std::vector<uint8_t> GameFile(uint32_t firstBlackRow)
{
	std::vector<uint8_t> file(k_HeaderSize);
	for (size_t word = 0; word < k_GameHeader.size(); ++word)
	{
		for (size_t byte = 0; byte < 4; ++byte)
		{
			file[word * 4 + byte] = static_cast<uint8_t>(k_GameHeader.at(word) >> (8 * byte));
		}
	}
	for (uint32_t block = 0; block < k_BlocksAcross * k_BlocksAcross; ++block)
	{
		const auto& bytes = block / k_BlocksAcross < firstBlackRow ? k_MagentaBlock : k_BlackBlock;
		file.insert(file.end(), bytes.begin(), bytes.end());
	}
	return file;
}

/// The vanilla profiles are the players' own files and not part of the project. The test that reads them runs only on
/// a copy of a Profiles folder that OPENBLACK_VANILLA_SAVES gives.
std::vector<std::filesystem::path> VanillaCurrentFolders()
{
	std::vector<std::filesystem::path> folders;
	const char* folder = std::getenv("OPENBLACK_VANILLA_SAVES"); // NOLINT(concurrency-mt-unsafe)
	if (folder == nullptr || !std::filesystem::is_directory(folder))
	{
		return folders;
	}
	for (const auto& entry : std::filesystem::recursive_directory_iterator(folder))
	{
		// The folder of the game that the player played last holds the pictures of its first two challenges
		if (entry.is_directory() && entry.path().filename() == k_CurrentGameFolder &&
		    std::filesystem::exists(entry.path() / ChallengeLargePictureFileName(0)))
		{
			folders.push_back(entry.path());
		}
	}
	return folders;
}
} // namespace

TEST(ChallengePictureFiles, TheNamesAreTheGameNames)
{
	EXPECT_EQ(ChallengeLargePictureFileName(0), "screenshots_hires_0_map_0.raw");
	EXPECT_EQ(ChallengeLargePictureFileName(1), "screenshots_hires_0_map_0.raw");
	EXPECT_EQ(ChallengeLargePictureFileName(21), "screenshots_hires_10_map_0.raw");
	EXPECT_EQ(ChallengeSmallPictureFileName(7), "screenshots_lores_0_map_0.raw");
	EXPECT_EQ(ChallengeSmallPictureFileName(8), "screenshots_lores_1_map_0.raw");
	const auto names = ChallengePictureFileNames();
	ASSERT_EQ(names.size(), 14u);
	EXPECT_EQ(names.front(), "screenshots_hires_0_map_0.raw");
	EXPECT_EQ(names[10], "screenshots_hires_10_map_0.raw");
	EXPECT_EQ(names[11], "screenshots_lores_0_map_0.raw");
	EXPECT_EQ(names.back(), "screenshots_lores_2_map_0.raw");
}

TEST(ChallengePictureFiles, TwoPicturesGiveTheGameBytes)
{
	// The game's profile after its first two challenges: each picture is magenta, the rest of the files are black
	ChallengePictureStore store;
	const auto magenta = Picture(255, 0, 255);
	const auto first = store.Put(0, magenta);
	EXPECT_EQ(first.first, "screenshots_hires_0_map_0.raw");
	EXPECT_EQ(first.second, "screenshots_lores_0_map_0.raw");
	std::ignore = store.Put(1, magenta);
	ExpectGameFile(*store.File(first.first), GameFile(64), "large");
	// The two small pictures fill the first 64 rows of the small picture file
	ExpectGameFile(*store.File(first.second), GameFile(16), "small");
}

TEST(ChallengePictureFiles, TheProfileFilesOfTheGameAreTheSame)
{
	for (const auto& folder : VanillaCurrentFolders())
	{
		const auto large = ReadAll(folder / ChallengeLargePictureFileName(0));
		const auto small = ReadAll(folder / ChallengeSmallPictureFileName(0));
		// Each vanilla file is the file of two magenta pictures
		ExpectGameFile(GameFile(64), large, folder.generic_string() + " large");
		ExpectGameFile(GameFile(16), small, folder.generic_string() + " small");
		ChallengePictureStore store;
		store.Set(ChallengeLargePictureFileName(0), large);
		store.Set(ChallengeSmallPictureFileName(0), small);
		for (uint32_t place = 0; place < 2; ++place)
		{
			const auto cells = store.Cells(place);
			EXPECT_EQ(cells.large.size(), static_cast<size_t>(64) * 32 * 8);
			EXPECT_EQ(cells.small.size(), static_cast<size_t>(32) * 16 * 8);
			EXPECT_TRUE(std::ranges::equal(std::span<const uint8_t>(cells.large).first(8), k_MagentaBlock));
		}
		EXPECT_TRUE(std::ranges::equal(std::span<const uint8_t>(store.Cells(2).small).first(8), k_BlackBlock));
	}
}

TEST(ChallengePictureFiles, APictureGoesIntoItsHalfAndItsCell)
{
	const auto red = Picture(255, 0, 0);
	const auto written = PutChallengePicture(std::nullopt, std::nullopt, red, 5);
	const auto large = ReadPictureFile(written.large);
	const auto small = ReadPictureFile(written.small);
	ASSERT_TRUE(large.has_value());
	ASSERT_TRUE(small.has_value());
	// Place 5: the bottom half of its large file, and the right of the third row of its small file
	const std::array<uint8_t, 8> redBlock {0x00, 0xF8, 0x00, 0xF8, 0, 0, 0, 0};
	const auto bottom = CellBlocks(*large, LargePictureCell(1));
	const auto top = CellBlocks(*large, LargePictureCell(0));
	EXPECT_TRUE(std::ranges::equal(std::span<const uint8_t>(bottom).first(8), redBlock));
	EXPECT_TRUE(std::ranges::all_of(top, [](uint8_t byte) { return byte == 0; }));
	const auto cell = CellBlocks(*small, SmallPictureCell(5));
	EXPECT_TRUE(std::ranges::equal(std::span<const uint8_t>(cell).last(8), redBlock));
	EXPECT_TRUE(std::ranges::all_of(CellBlocks(*small, SmallPictureCell(4)), [](uint8_t byte) { return byte == 0; }));

	// A new picture at the other place of the file keeps this picture
	const auto again = PutChallengePicture(written.large, written.small, Picture(0, 0, 255), 4);
	EXPECT_EQ(CellBlocks(*ReadPictureFile(again.large), LargePictureCell(1)), bottom);
	EXPECT_EQ(CellBlocks(*ReadPictureFile(again.small), SmallPictureCell(5)), cell);
}

TEST(ChallengePictureFiles, AFileTheGameCannotReadIsBlack)
{
	const std::vector<uint8_t> broken(100, 0x55);
	const auto written = PutChallengePicture(broken, broken, Picture(255, 0, 255), 0);
	const auto large = ReadPictureFile(written.large);
	ASSERT_TRUE(large.has_value());
	EXPECT_TRUE(std::ranges::all_of(CellBlocks(*large, LargePictureCell(1)), [](uint8_t byte) { return byte == 0; }));
	ChallengePictureStore store;
	store.Set(ChallengeLargePictureFileName(0), broken);
	EXPECT_TRUE(store.Cells(0).large.empty());
	EXPECT_TRUE(store.Cells(0).small.empty());
}

TEST(ChallengePictureFiles, TheFilesWriteAndReadInAProfileFolder)
{
	// The store's files go into a folder of a test profile and come back the same
	const auto folder = std::filesystem::temp_directory_path() / "openblack_test_challenge_picture_files" / "Current";
	std::filesystem::remove_all(folder.parent_path());
	std::filesystem::create_directories(folder);
	ChallengePictureStore store;
	std::ignore = store.Put(0, Picture(255, 0, 255));
	std::ignore = store.Put(3, Picture(0, 255, 0));
	for (const auto& name : ChallengePictureFileNames())
	{
		if (const auto file = store.File(name); file.has_value())
		{
			WriteAll(folder / name, *file);
		}
	}
	EXPECT_TRUE(std::filesystem::exists(folder / "screenshots_hires_0_map_0.raw"));
	EXPECT_TRUE(std::filesystem::exists(folder / "screenshots_hires_1_map_0.raw"));
	EXPECT_TRUE(std::filesystem::exists(folder / "screenshots_lores_0_map_0.raw"));
	EXPECT_FALSE(std::filesystem::exists(folder / "screenshots_hires_2_map_0.raw"));

	ChallengePictureStore read;
	for (const auto& name : ChallengePictureFileNames())
	{
		if (std::filesystem::exists(folder / name))
		{
			read.Set(name, ReadAll(folder / name));
		}
	}
	for (const uint32_t place : {0u, 3u})
	{
		EXPECT_EQ(read.Cells(place).large, store.Cells(place).large) << place;
		EXPECT_EQ(read.Cells(place).small, store.Cells(place).small) << place;
		EXPECT_FALSE(read.Cells(place).large.empty());
	}
	EXPECT_TRUE(read.Cells(4).large.empty());
	std::filesystem::remove_all(folder.parent_path());
}

TEST(ChallengePictureFiles, DecodedBlocksKeepAllTheBits)
{
	// A block of white and black: the display card mixes the two in thirds, with all the bits of each colour
	const std::array<uint8_t, 8> block {0xFF, 0xFF, 0x00, 0x00, 0b00'00'10'11, 0, 0, 0};
	const auto texels = DecodeBlocks(block, 4, 4);
	ASSERT_EQ(texels.size(), 48u);
	EXPECT_EQ(texels[0], 85);
	EXPECT_EQ(texels[3], 170);
	EXPECT_EQ(texels[6], 255);
	EXPECT_EQ(texels[9], 255);
	EXPECT_EQ(texels[12], 255);
	EXPECT_TRUE(DecodeBlocks(block, 8, 4).empty());
}
