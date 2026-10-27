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
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <InstantSaves.h>
#include <SavePictureMaker.h>
#include <SavePictures.h>
#include <WorldFile.h>
#include <gtest/gtest.h>

#include "Common/PlayerProfiles.h"

using namespace openblack::gamesave;

namespace
{
constexpr size_t k_RowSize = 256 * k_TexelSize;

/// A screen of one colour in each pixel, given by the pixel's place
std::vector<uint8_t> Screen(uint32_t width, uint32_t height, uint32_t pitch, auto colourAt)
{
	std::vector<uint8_t> pixels(static_cast<size_t>(pitch) * height, 0);
	for (uint32_t y = 0; y < height; ++y)
	{
		for (uint32_t x = 0; x < width; ++x)
		{
			const auto colour = colourAt(x, y);
			auto* pixel = pixels.data() + (static_cast<size_t>(y) * pitch) + (static_cast<size_t>(x) * 4);
			pixel[0] = colour[2];
			pixel[1] = colour[1];
			pixel[2] = colour[0];
			pixel[3] = 0xFF;
		}
	}
	return pixels;
}

std::vector<uint8_t> ReadAll(const std::filesystem::path& path)
{
	std::ifstream stream(path, std::ios::binary);
	return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

/// The players' saved games of the original game are their own files and are not part of the project. The tests that
/// read them run only on a copy of a Profiles folder that OPENBLACK_VANILLA_SAVES gives.
std::vector<std::filesystem::path> VanillaPictureFolders()
{
	std::vector<std::filesystem::path> folders;
	const char* folder = std::getenv("OPENBLACK_VANILLA_SAVES"); // NOLINT(concurrency-mt-unsafe)
	if (folder == nullptr || !std::filesystem::is_directory(folder))
	{
		return folders;
	}
	for (const auto& entry : std::filesystem::recursive_directory_iterator(folder))
	{
		if (entry.is_directory() && entry.path().filename() == k_PicturesFolder &&
		    std::filesystem::exists(entry.path() / std::string(k_TemporaryPictureFileName)))
		{
			folders.push_back(entry.path());
		}
	}
	return folders;
}

std::vector<uint8_t> Solid(uint8_t red, uint8_t green, uint8_t blue)
{
	std::vector<uint8_t> texels(k_PictureTexelsSize);
	for (size_t i = 0; i < texels.size(); i += k_TexelSize)
	{
		texels[i] = red;
		texels[i + 1] = green;
		texels[i + 2] = blue;
	}
	return texels;
}
} // namespace

TEST(SaveScreenPicture, TakesTheRowsAndColumnsAtTheSameFraction)
{
	// A screen of 512 by 256: each texel takes every second row and column. Red tells the column, green the row.
	const auto pixels = Screen(512, 256, 512 * 4, [](uint32_t x, uint32_t y) {
		return std::array<uint8_t, 3> {static_cast<uint8_t>(x / 2), static_cast<uint8_t>(y / 2), 0x40};
	});
	const auto picture = SampleScreen({.width = 512, .height = 256, .pitch = 512 * 4, .bottomUp = false, .bgra = pixels});
	ASSERT_TRUE(picture.has_value());
	ASSERT_EQ(picture->size(), static_cast<size_t>(256) * 128 * 3);
	// The first texel of a row is its own pixel
	EXPECT_EQ((*picture)[0], 0);
	// Each texel is the mean of its pixel and the pixel of the texel to its left, rounded down
	const auto* texel = picture->data() + (static_cast<size_t>(10) * k_RowSize) + (static_cast<size_t>(7) * 3);
	EXPECT_EQ(texel[0], (6 + 7) / 2);
	EXPECT_EQ(texel[1], 10);
	EXPECT_EQ(texel[2], 0x40);
}

TEST(SaveScreenPicture, RoundsThePlaceDownAndReadsBottomUpScreens)
{
	// A screen of 300 by 200: the row of texel 127 is 127 * 200 / 128 = 198, the column of texel 255 is 299
	const auto pixels = Screen(300, 200, 1216, [](uint32_t x, uint32_t y) {
		return std::array<uint8_t, 3> {static_cast<uint8_t>(x & 0xFFu), static_cast<uint8_t>(y), 0};
	});
	const auto top = SampleScreen({.width = 300, .height = 200, .pitch = 1216, .bottomUp = false, .bgra = pixels});
	ASSERT_TRUE(top.has_value());
	const auto* last = top->data() + (static_cast<size_t>(127) * k_RowSize) + (static_cast<size_t>(255) * 3);
	EXPECT_EQ(last[1], 198);
	// Column 254 is 297, column 255 is 298
	EXPECT_EQ(last[0], ((297 & 0xFF) + (298 & 0xFF)) / 2);
	// The same rows from a screen stored with the bottom row first
	const auto bottomUp = SampleScreen({.width = 300, .height = 200, .pitch = 1216, .bottomUp = true, .bgra = pixels});
	ASSERT_TRUE(bottomUp.has_value());
	EXPECT_EQ((*bottomUp)[1], 199);
	EXPECT_FALSE(SampleScreen({.width = 300, .height = 200, .pitch = 1216, .bottomUp = false, .bgra = {}}).has_value());
}

TEST(SaveScreenPicture, SmoothsWithTheNeighboursInThreeSteps)
{
	auto texels = Solid(0, 0, 0);
	// One white texel at (5, 5): the texels to its left, above and above to the left take some of it
	for (size_t colour = 0; colour < 3; ++colour)
	{
		texels[(5 * k_RowSize) + (5 * 3) + colour] = 255;
	}
	SmoothLargePicture(texels);
	const auto at = [&texels](size_t x, size_t y) { return texels[(y * k_RowSize) + (x * 3)]; };
	// Right neighbour: 255 * 127 / 255 = 127, then 127 * 128 / 255 = 63
	EXPECT_EQ(at(4, 5), 63);
	// Below: the second step gives 255 * 128 / 255 = 128, the third 128 * 127 / 255 = 63
	EXPECT_EQ(at(5, 4), 63);
	// Below and to the right: the second step gives 255 * 127 / 255 = 127, the third 127 * 127 / 255 = 63
	EXPECT_EQ(at(4, 4), 63);
	// The white texel itself: 255 * 128 / 255 = 128, then 128 * 128 / 255 = 64
	EXPECT_EQ(at(5, 5), 64);
	EXPECT_EQ(at(6, 6), 0);
}

TEST(SaveScreenPicture, LeavesTheLastRowAndColumn)
{
	auto texels = Solid(10, 10, 10);
	// The last column and the row below the large picture differ from the rest
	for (size_t y = 0; y < 129; ++y)
	{
		texels[(y * k_RowSize) + (255 * 3)] = 200;
	}
	SmoothLargePicture(texels);
	EXPECT_EQ(texels[(3 * k_RowSize) + (255 * 3)], 200);
	EXPECT_EQ(texels[127 * k_RowSize], 10);
}

TEST(SaveScreenPicture, PutsTheSmallPictureFromEvenRows)
{
	auto large = Solid(0, 0, 0);
	// Odd rows are white, even rows have red that tells the column
	for (size_t y = 0; y < 128; ++y)
	{
		for (size_t x = 0; x < 256; ++x)
		{
			large[(y * k_RowSize) + (x * 3)] = (y % 2) == 1 ? 255 : static_cast<uint8_t>(x);
		}
	}
	auto texels = Solid(1, 1, 1);
	PutSmallPicture(texels, std::span<const uint8_t>(large).first(128 * k_RowSize));
	// The small picture is in the cell of 128 by 64 at (0, 128)
	EXPECT_EQ(texels[128 * k_RowSize], (0 + 1) / 2);
	EXPECT_EQ(texels[(130 * k_RowSize) + (10 * 3)], (20 + 21) / 2);
	EXPECT_EQ(texels[(191 * k_RowSize) + (127 * 3)], (254 + 255) / 2);
	EXPECT_EQ(texels[(128 * k_RowSize) + (128 * 3)], 1);
	EXPECT_EQ(texels[192 * k_RowSize], 1);
}

TEST(SaveScreenPicture, CompressesOneColourAsTheGameDoes)
{
	// The game's files keep a block of one colour as that colour two times and every texel at the first colour
	const auto magenta = EncodePicture(Solid(255, 0, 255));
	const std::vector<uint8_t> magentaBlock {0x1F, 0xF8, 0x1F, 0xF8, 0, 0, 0, 0};
	EXPECT_TRUE(std::equal(magentaBlock.begin(), magentaBlock.end(), magenta.begin()));
	const auto black = EncodePicture(Solid(0, 0, 0));
	EXPECT_TRUE(std::ranges::all_of(black, [](uint8_t byte) { return byte == 0; }));
	// Read back, the colours keep five bits each
	const auto texels = DecodePicture(magenta);
	EXPECT_EQ(texels[0], 248);
	EXPECT_EQ(texels[1], 0);
	EXPECT_EQ(texels[2], 248);
}

TEST(SaveScreenPicture, CompressesTwoColoursWithoutLoss)
{
	auto texels = Solid(0, 0, 0);
	// The left half of each block is white
	for (size_t y = 0; y < 256; ++y)
	{
		for (size_t x = 0; x < 256; ++x)
		{
			if ((x % 4) < 2)
			{
				std::fill_n(texels.begin() + static_cast<std::ptrdiff_t>((y * k_RowSize) + (x * 3)), 3, uint8_t {255});
			}
		}
	}
	const auto blocks = EncodePicture(texels);
	EXPECT_EQ(blocks[0], 0xFF);
	EXPECT_EQ(blocks[1], 0xFF);
	EXPECT_EQ(blocks[2], 0);
	EXPECT_EQ(blocks[3], 0);
	const auto back = DecodePicture(blocks);
	EXPECT_EQ(back[0], 248);
	EXPECT_EQ(back[2 * 3], 0);
}

TEST(SaveScreenPicture, WritesTheFileThatTheRoomReads)
{
	const auto file = WritePictureFile(NewPictureBlocks());
	ASSERT_EQ(file.size(), k_PictureFileSize);
	EXPECT_EQ(file[0], 0x80);
	EXPECT_EQ(file[1], 0x80);
	const auto read = ReadPictureFile(file);
	ASSERT_TRUE(read.has_value());
	EXPECT_TRUE(std::ranges::all_of(read->blocks, [](uint8_t byte) { return byte == 0xFF; }));
}

TEST(SaveScreenPicture, MakesTheTemporaryFile)
{
	// With no old file, the rest of the picture is black
	const std::vector<uint8_t> screen(static_cast<size_t>(256) * 128 * 3, 255);
	const auto file = MakeTemporaryPicture(std::nullopt, screen);
	const auto picture = ReadPictureFile(file);
	ASSERT_TRUE(picture.has_value());
	const auto large = CellBlocks(*picture, LargePictureCell(0));
	const auto small = CellBlocks(*picture, SmallPictureCell(4));
	const auto rest = CellBlocks(*picture, SmallPictureCell(5));
	const std::vector<uint8_t> white {0xFF, 0xFF, 0xFF, 0xFF, 0, 0, 0, 0};
	EXPECT_TRUE(std::equal(white.begin(), white.end(), large.begin()));
	EXPECT_TRUE(std::equal(white.begin(), white.end(), small.begin()));
	EXPECT_TRUE(std::ranges::all_of(rest, [](uint8_t byte) { return byte == 0; }));
	// A second picture keeps the rest of the old file
	const auto again = MakeTemporaryPicture(file, std::vector<uint8_t>(static_cast<size_t>(256) * 128 * 3, 0));
	const auto second = ReadPictureFile(again);
	ASSERT_TRUE(second.has_value());
	EXPECT_TRUE(std::ranges::all_of(CellBlocks(*second, LargePictureCell(0)), [](uint8_t byte) { return byte == 0; }));
	EXPECT_EQ(CellBlocks(*second, SmallPictureCell(5)), rest);
}

TEST(SaveScreenPicture, PutsThePicturesIntoTheCellsOfTheSlot)
{
	const auto temporary = MakeTemporaryPicture(std::nullopt, std::vector<uint8_t>(static_cast<size_t>(256) * 128 * 3, 255));
	const auto source = *ReadPictureFile(temporary);
	// Slot 15: the bottom half of the large file 7 and the last cell of the small file 1, both files new
	const auto files = PutSlotPictures(temporary, std::nullopt, std::nullopt, 15);
	const auto large = ReadPictureFile(files.large);
	const auto small = ReadPictureFile(files.small);
	ASSERT_TRUE(large.has_value());
	ASSERT_TRUE(small.has_value());
	EXPECT_EQ(CellBlocks(*large, LargePictureCell(15)), CellBlocks(source, LargePictureCell(0)));
	EXPECT_EQ(CellBlocks(*small, SmallPictureCell(15)), CellBlocks(source, SmallPictureCell(4)));
	EXPECT_TRUE(std::ranges::all_of(CellBlocks(*large, LargePictureCell(14)), [](uint8_t byte) { return byte == 0xFF; }));
	// Slot 14 into the same files keeps slot 15's pictures
	const auto both = PutSlotPictures(temporary, files.large, files.small, 14);
	EXPECT_EQ(CellBlocks(*ReadPictureFile(both.large), LargePictureCell(15)), CellBlocks(source, LargePictureCell(0)));
	// With no temporary file, the slot takes the blocks of a new file
	const auto none = PutSlotPictures(std::nullopt, std::nullopt, std::nullopt, 3);
	EXPECT_TRUE(std::ranges::all_of(CellBlocks(*ReadPictureFile(none.large), LargePictureCell(3)),
	                                [](uint8_t byte) { return byte == 0xFF; }));
}

TEST(SaveScreenPicture, MatchesTheGameFiles)
{
	// The game's own files: the description is the same but for the place in memory, and the pictures of a slot are the
	// blocks of the temporary file
	for (const auto& folder : VanillaPictureFolders())
	{
		const auto temporary = ReadAll(folder / std::string(k_TemporaryPictureFileName));
		const auto ours = WritePictureFile(ReadPictureFile(temporary)->blocks);
		ASSERT_EQ(ours.size(), temporary.size());
		for (size_t i = 0; i < ours.size(); ++i)
		{
			if (i < 0x28 || i >= 0x2C)
			{
				EXPECT_EQ(ours[i], temporary[i]) << folder << " byte " << i;
			}
		}
		// The game's temporary file holds one colour, so a new compression of what openblack reads gives its blocks
		const auto blocks = ReadPictureFile(temporary)->blocks;
		EXPECT_EQ(EncodePicture(DecodePicture(blocks)), blocks) << folder;
	}
}

TEST(InstantSaves, NamesTheSlotWithTheLand)
{
	EXPECT_EQ(InstantSaveName(u"Auto Save Land: %3.0f", 1), u"Auto Save Land:   1");
	EXPECT_EQ(InstantSaveName(u"Auto Save Land: %3.0f", 1234), u"Auto Save Land: 1234");
	EXPECT_EQ(InstantSaveName(u"Quick Save", 2), u"Quick Save");
	EXPECT_EQ(InstantSaveName(u"100%% %d", 2), u"100% %d");
	EXPECT_EQ(k_SlotNameTexts.at(15), 6690u);
	EXPECT_EQ(k_SlotNameTexts.at(19), 6661u);
	EXPECT_EQ(k_SlotNameTexts.at(20), 6691u);
	EXPECT_EQ(k_SlotNameTexts.at(0), 0u);
}

TEST(InstantSaves, AutosavesAfterTenMinutesOfTurns)
{
	AutosaveState state {.autosaveOn = true,
	                     .gameOver = false,
	                     .scriptCutScene = false,
	                     .paused = false,
	                     .land = 1,
	                     .turn = 6000,
	                     .lastAutosaveTurn = 0};
	EXPECT_FALSE(AutosaveDue(state));
	state.turn = 6001;
	EXPECT_TRUE(AutosaveDue(state));
	state.autosaveOn = false;
	EXPECT_FALSE(AutosaveDue(state));
	state.autosaveOn = true;
	state.paused = true;
	EXPECT_FALSE(AutosaveDue(state));
	state.paused = false;
	state.scriptCutScene = true;
	EXPECT_FALSE(AutosaveDue(state));
	state.scriptCutScene = false;
	state.gameOver = true;
	EXPECT_FALSE(AutosaveDue(state));
	state.gameOver = false;
	state.networkGame = true;
	EXPECT_FALSE(AutosaveDue(state));
	state.networkGame = false;
	state.land = 6;
	EXPECT_FALSE(AutosaveDue(state));
	// A last autosave turn after the turn wraps to a large count of turns
	state.land = 1;
	state.turn = 10;
	state.lastAutosaveTurn = 20;
	EXPECT_TRUE(AutosaveDue(state));
	EXPECT_EQ(AutosaveSlot(0), 15u);
	EXPECT_EQ(AutosaveSlot(4), 16u);
	EXPECT_EQ(AutosaveSlot(5), 17u);
}

TEST(InstantSaves, QuickKeysNeedMoreThanThreeTurns)
{
	EXPECT_FALSE(QuickTurnAllowed(3, 0, false, false, false));
	EXPECT_TRUE(QuickTurnAllowed(4, 0, false, false, false));
	EXPECT_FALSE(QuickTurnAllowed(100, 97, false, false, false));
	EXPECT_TRUE(QuickTurnAllowed(100, 104, false, false, false));
	EXPECT_FALSE(QuickTurnAllowed(100, 103, false, false, false));
	EXPECT_FALSE(QuickTurnAllowed(100, 0, true, false, false));
	EXPECT_FALSE(QuickTurnAllowed(100, 0, false, true, false));
	// A network game never saves or loads
	EXPECT_FALSE(QuickTurnAllowed(100, 0, false, false, true));
	// The most negative difference stays negative
	EXPECT_FALSE(QuickTurnAllowed(0x80000000u, 0, false, false, false));
}

TEST(InstantSaves, TheGameBlockKeepsTheTurnsOfTheLastSaves)
{
	const GameChunk game {.land = 1,
	                      .turn = 9000,
	                      .landscape = "./Data/Landscape/Land1.lnd",
	                      .knownLeashes = 0,
	                      .creatureInTemple = 0,
	                      .lastAutosaveTurn = 7000,
	                      .lastQuickSaveTurn = 8000,
	                      .landscapeFile = {}};
	const auto chunk = ToChunk(game);
	EXPECT_EQ(ReadGameChunk(chunk.bytes), game);
	// A block from before the landscape file has the two turns at its end; one from before the two turns has neither
	const size_t emptyText = sizeof(uint32_t);
	const auto beforeLandscape = std::span<const uint8_t>(chunk.bytes).first(chunk.bytes.size() - emptyText);
	EXPECT_EQ(ReadGameChunk(beforeLandscape), game);
	const auto older = std::span<const uint8_t>(chunk.bytes).first(chunk.bytes.size() - emptyText - 8);
	const auto read = ReadGameChunk(older);
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(read->lastAutosaveTurn, 0u);
	EXPECT_EQ(read->turn, 9000u);
}

TEST(InstantSaves, TheGameBlockKeepsTheLandscapeFile)
{
	const GameChunk game {.land = 1,
	                      .turn = 9000,
	                      .landscape = "./Scripts/Land1.txt",
	                      .knownLeashes = 0,
	                      .creatureInTemple = 0,
	                      .lastAutosaveTurn = 7000,
	                      .lastQuickSaveTurn = 8000,
	                      .landscapeFile = "./Data/Landscape/Land1.lnd"};
	EXPECT_EQ(ReadGameChunk(ToChunk(game).bytes), game);
}

TEST(InstantSaves, TheProfileKeepsTheAutosavesOfEachLand)
{
	namespace profiles = openblack::player_profiles;
	profiles::Profiles kept;
	kept.profiles.push_back({.name = "abab", .creatureFile = "C1.erc", .creatureName = "abab"});
	kept.current = "abab";
	kept.profiles.front().landAutosaves = {{1, 4}, {2, 1}};
	const auto read = profiles::Read(profiles::Write(kept));
	ASSERT_EQ(read.profiles.size(), 1u);
	EXPECT_EQ(read.profiles.front().landAutosaves, kept.profiles.front().landAutosaves);
}
