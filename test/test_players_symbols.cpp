/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <optional>
#include <span>
#include <vector>

#include <gtest/gtest.h>

#include "3D/PlayersSymbols.h"
#include "PlayerSlots.h"

using namespace openblack::players_symbols;

namespace
{

constexpr size_t k_Texels = static_cast<size_t>(k_SheetSide) * k_SheetSide;

/// A sheet with each square's texels set to the number of the square
Sheet NumberedSheet(uint8_t offset)
{
	Sheet sheet {.colours = std::vector<uint8_t>(k_Texels * 3), .alpha = std::vector<uint8_t>(k_Texels)};
	for (uint32_t y = 0; y < k_SheetSide; ++y)
	{
		for (uint32_t x = 0; x < k_SheetSide; ++x)
		{
			const auto square = static_cast<uint8_t>((y / k_SquareSide) * k_SquaresInARow + x / k_SquareSide);
			const auto texel = static_cast<size_t>(y) * k_SheetSide + x;
			sheet.colours[texel * 3] = static_cast<uint8_t>(offset + square);
			sheet.colours[texel * 3 + 1] = static_cast<uint8_t>(offset + square);
			sheet.colours[texel * 3 + 2] = static_cast<uint8_t>(offset + square);
			sheet.alpha[texel] = static_cast<uint8_t>(offset + square);
		}
	}
	return sheet;
}

/// The texel of a square, counted from the square's top left
size_t TexelOf(uint32_t square, uint32_t x, uint32_t y)
{
	return static_cast<size_t>((square / k_SquaresInARow) * k_SquareSide + y) * k_SheetSide +
	       (square % k_SquaresInARow) * k_SquareSide + x;
}

} // namespace

TEST(PlayersSymbols, TheProfilesSymbolGoesToThePlayersSquareAndTheOtherSquaresStay)
{
	auto players = NumberedSheet(100);
	const auto designs = NumberedSheet(0);
	// The sixth design, in the second row of the sheet of designs, goes to the first player's square
	PutProfileSymbol(players, designs, 5, 0, std::nullopt);
	for (const auto [x, y] : std::array {std::pair {0u, 0u}, std::pair {63u, 63u}, std::pair {10u, 40u}})
	{
		const auto texel = TexelOf(0, x, y);
		EXPECT_EQ(players.colours[texel * 3], 5);
		EXPECT_EQ(players.colours[texel * 3 + 2], 5);
		EXPECT_EQ(players.alpha[texel], 5);
	}
	// The other players' squares keep what the file has
	EXPECT_EQ(players.alpha[TexelOf(1, 0, 0)], 101);
	EXPECT_EQ(players.alpha[TexelOf(4, 0, 0)], 104);
	EXPECT_EQ(players.alpha[TexelOf(7, 63, 63)], 107);
}

TEST(PlayersSymbols, ThePlayersOwnPictureTakesThePlaceOfTheSymbol)
{
	auto players = NumberedSheet(100);
	const auto designs = NumberedSheet(0);
	std::vector<uint8_t> picture(static_cast<size_t>(k_SquareSide) * k_SquareSide, 0);
	picture[0] = 0xF0;
	picture[static_cast<size_t>(k_SquareSide) * 2 + 3] = 0x37;
	PutProfileSymbol(players, designs, 5, 0, std::span<const uint8_t>(picture));
	// Each grey is the texel's colours and its alpha
	EXPECT_EQ(players.colours[TexelOf(0, 0, 0) * 3 + 1], 0xF0);
	EXPECT_EQ(players.alpha[TexelOf(0, 0, 0)], 0xF0);
	EXPECT_EQ(players.colours[TexelOf(0, 3, 2) * 3], 0x37);
	EXPECT_EQ(players.alpha[TexelOf(0, 3, 2)], 0x37);
	EXPECT_EQ(players.alpha[TexelOf(0, 4, 2)], 0);
}

TEST(PlayersSymbols, ASheetOfTheWrongSizeOrASquareOutsideItChangesNothing)
{
	auto players = NumberedSheet(100);
	const auto designs = NumberedSheet(0);
	Sheet small {.colours = std::vector<uint8_t>(12), .alpha = std::vector<uint8_t>(4)};
	CopyDesign(players, small, 1, 0);
	CopyDesign(players, designs, 16, 0);
	CopyDesign(players, designs, 1, 16);
	EXPECT_EQ(players.alpha[TexelOf(0, 0, 0)], 100);
	EXPECT_FALSE(small.Whole());
	EXPECT_TRUE(players.Whole());
}

TEST(PlayersSymbols, ThePlayerShowsTheSymbolOfTheProfileOrTheSymbolThatATestPutsInItsPlace)
{
	EXPECT_EQ(ChosenSymbol(14, std::nullopt), 14u);
	EXPECT_EQ(ChosenSymbol(0, std::nullopt), 0u);
	EXPECT_EQ(ChosenSymbol(14, 3u), 3u);
	EXPECT_EQ(ChosenSymbol(std::nullopt, 3u), 3u);
	EXPECT_EQ(ChosenSymbol(std::nullopt, std::nullopt), std::nullopt);
}

TEST(PlayersSymbols, ALoadMakesTheSheetAgainWithTheSymbolOfTheProfilePlayedNow)
{
	const auto file = NumberedSheet(100);
	const auto designs = NumberedSheet(0);
	// The game that made the save showed symbol 5 in the square of player 1
	const auto played = MakeSheet(file, designs, ChosenSymbol(5, std::nullopt), 1, std::nullopt);
	EXPECT_EQ(played.alpha[TexelOf(1, 0, 0)], 5);
	// The save keeps no part of the sheet, so the load makes it again from the file, with the profile's symbol 14
	const auto loaded = MakeSheet(file, designs, ChosenSymbol(14, std::nullopt), 1, std::nullopt);
	EXPECT_EQ(loaded.alpha[TexelOf(1, 0, 0)], 14);
	EXPECT_EQ(loaded.colours[TexelOf(1, 63, 63) * 3], 14);
	// The other squares are the file's squares
	EXPECT_EQ(loaded.alpha[TexelOf(0, 0, 0)], 100);
	EXPECT_EQ(loaded.alpha[TexelOf(2, 0, 0)], 102);
	// A load of the same profile makes the same sheet as the game that made the save
	EXPECT_EQ(MakeSheet(file, designs, ChosenSymbol(5, std::nullopt), 1, std::nullopt).colours, played.colours);
}

TEST(PlayersSymbols, WithNoProfileTheSheetIsTheFilesSheet)
{
	const auto file = NumberedSheet(100);
	const auto designs = NumberedSheet(0);
	const auto sheet = MakeSheet(file, designs, std::nullopt, 0, std::nullopt);
	EXPECT_EQ(sheet.colours, file.colours);
	EXPECT_EQ(sheet.alpha, file.alpha);
}

TEST(PlayersSymbols, EachComputerPlayerTakesTheGodBeforeItsNumber)
{
	// The second player takes Lethys and the third player takes Khazar, as on the second land
	EXPECT_EQ(k_GodPictures.at(GodPictureOf(1, 2)), "Lethis");
	EXPECT_EQ(k_GodPictures.at(GodPictureOf(2, 2)), "Kazarr");
	EXPECT_EQ(k_GodPictures.at(GodPictureOf(3, 4)), "nemesis");
	// The first player and the players after Nemesis take the nearest picture
	EXPECT_EQ(GodPictureOf(0, 2), 0u);
	EXPECT_EQ(GodPictureOf(6, 2), 2u);
}

TEST(PlayersSymbols, OnTheThirdAndFifthLandsTheSecondPlayerTakesKhazarAndNemesis)
{
	EXPECT_EQ(k_GodPictures.at(GodPictureOf(1, 3)), "Kazarr");
	EXPECT_EQ(k_GodPictures.at(GodPictureOf(1, 5)), "nemesis");
	// Only the second player changes on these lands
	EXPECT_EQ(GodPictureOf(2, 3), 1u);
	EXPECT_EQ(GodPictureOf(2, 5), 1u);
	EXPECT_EQ(GodPictureOf(1, 4), 0u);
}

TEST(PlayersSymbols, AGodsPictureOfHalfSizeGoesToTheSquareOfASheetAtHalfSize)
{
	constexpr uint32_t k_HalfSheet = k_SheetSide / 2;
	constexpr uint32_t k_HalfSquare = k_SquareSide / 2;
	Sheet half {.colours = std::vector<uint8_t>(static_cast<size_t>(k_HalfSheet) * k_HalfSheet * 3, 9),
	            .alpha = std::vector<uint8_t>(static_cast<size_t>(k_HalfSheet) * k_HalfSheet, 9)};
	std::vector<uint8_t> picture(static_cast<size_t>(k_HalfSquare) * k_HalfSquare, 0x80);
	picture.back() = 0x11;
	// The sixth square is the second square of the second row
	PutPicture(half, picture, 5);
	const auto texel = [](uint32_t square, uint32_t x, uint32_t y) {
		return static_cast<size_t>((square / k_SquaresInARow) * k_HalfSquare + y) * k_HalfSheet +
		       (square % k_SquaresInARow) * k_HalfSquare + x;
	};
	EXPECT_EQ(half.alpha[texel(5, 0, 0)], 0x80);
	EXPECT_EQ(half.colours[texel(5, 31, 31) * 3 + 2], 0x11);
	EXPECT_EQ(half.alpha[texel(5, 31, 31)], 0x11);
	// The squares about it keep what they had
	EXPECT_EQ(half.alpha[texel(4, 31, 0)], 9);
	EXPECT_EQ(half.alpha[texel(6, 0, 0)], 9);
	EXPECT_EQ(half.alpha[texel(1, 0, 31)], 9);
	// A picture of full size does not go into a sheet at half size
	const std::vector<uint8_t> full(static_cast<size_t>(k_SquareSide) * k_SquareSide, 0x22);
	PutPicture(half, full, 0);
	EXPECT_EQ(half.alpha[texel(0, 0, 0)], 9);
}

// A land and each game with up to 16 player numbers keep the sheet of the file; a game of 32 gods has the wider sheet
TEST(PlayersSymbols, AGameWithMorePlayerNumbersThanSquaresHasTheWiderSheet)
{
	EXPECT_EQ(SquaresInARowFor(0), k_SquaresInARow);
	EXPECT_EQ(SquaresInARowFor(8), k_SquaresInARow);
	EXPECT_EQ(SquaresInARowFor(16), k_SquaresInARow);
	EXPECT_EQ(SquaresInARowFor(17), k_WideSquaresInARow);
	EXPECT_EQ(SquaresInARowFor(33), k_WideSquaresInARow);
	// The wider sheet has a square for each player number of a game with the most players
	EXPECT_GE(static_cast<size_t>(k_WideSquaresInARow) * k_WideSquaresInARow, openblack::player_slots::k_PlayerNumbers);
}

// The wider sheet keeps each square of the file at the number of its player, and the new squares are clear
TEST(PlayersSymbols, TheWiderSheetKeepsTheSquareOfEachPlayer)
{
	const auto file = NumberedSheet(100);
	// The same squares in a row give the same sheet
	const auto same = Widen(file, k_SquaresInARow);
	EXPECT_EQ(same.colours, file.colours);
	EXPECT_EQ(same.alpha, file.alpha);
	EXPECT_EQ(same.squaresInARow, k_SquaresInARow);

	const auto wide = Widen(file, k_WideSquaresInARow);
	ASSERT_TRUE(wide.Whole());
	constexpr uint32_t k_WideSide = k_WideSquaresInARow * k_SquareSide;
	EXPECT_EQ(wide.alpha.size(), static_cast<size_t>(k_WideSide) * k_WideSide);
	const auto texel = [](uint32_t square, uint32_t x, uint32_t y) {
		return static_cast<size_t>((square / k_WideSquaresInARow) * k_SquareSide + y) * k_WideSide +
		       (square % k_WideSquaresInARow) * k_SquareSide + x;
	};
	for (uint32_t square = 0; square < k_SquaresInARow * k_SquaresInARow; ++square)
	{
		EXPECT_EQ(wide.alpha[texel(square, 0, 0)], static_cast<uint8_t>(100 + square));
		EXPECT_EQ(wide.alpha[texel(square, 63, 63)], static_cast<uint8_t>(100 + square));
		EXPECT_EQ(wide.colours[texel(square, 10, 20) * 3 + 1], static_cast<uint8_t>(100 + square));
	}
	EXPECT_EQ(wide.alpha[texel(16, 0, 0)], 0);
	EXPECT_EQ(wide.alpha[texel(63, 63, 63)], 0);
	// A sheet that is not whole comes back as it is
	const Sheet small {.colours = std::vector<uint8_t>(12), .alpha = std::vector<uint8_t>(4)};
	EXPECT_EQ(Widen(small, k_WideSquaresInARow).alpha.size(), 4u);
}

// A god after the seventh at this computer gets the symbol of its profile in its own square of the wider sheet
TEST(PlayersSymbols, ALaterGodGetsItsProfileSymbolInTheWiderSheet)
{
	const auto designs = NumberedSheet(0);
	constexpr uint32_t k_TwentiethGod = 20;
	constexpr uint32_t k_WideSide = k_WideSquaresInARow * k_SquareSide;
	const auto texel = [](uint32_t square, uint32_t x, uint32_t y) {
		return static_cast<size_t>((square / k_WideSquaresInARow) * k_SquareSide + y) * k_WideSide +
		       (square % k_WideSquaresInARow) * k_SquareSide + x;
	};
	const auto sheet = MakeSheet(Widen(NumberedSheet(100), k_WideSquaresInARow), designs, ChosenSymbol(5, std::nullopt),
	                             k_TwentiethGod, std::nullopt);
	EXPECT_EQ(sheet.alpha[texel(k_TwentiethGod, 0, 0)], 5);
	EXPECT_EQ(sheet.alpha[texel(k_TwentiethGod, 63, 63)], 5);
	EXPECT_EQ(sheet.alpha[texel(1, 0, 0)], 101);
	// The own picture of the profile also goes to the square, and to the square of a sheet at half size
	std::vector<uint8_t> picture(static_cast<size_t>(k_SquareSide) * k_SquareSide, 0x44);
	auto withPicture = Widen(NumberedSheet(100), k_WideSquaresInARow);
	PutPicture(withPicture, picture, k_TwentiethGod);
	EXPECT_EQ(withPicture.alpha[texel(k_TwentiethGod, 31, 31)], 0x44);
	constexpr uint32_t k_HalfSide = k_WideSide / 2;
	Sheet half {.colours = std::vector<uint8_t>(static_cast<size_t>(k_HalfSide) * k_HalfSide * 3, 9),
	            .alpha = std::vector<uint8_t>(static_cast<size_t>(k_HalfSide) * k_HalfSide, 9),
	            .squaresInARow = k_WideSquaresInARow};
	std::vector<uint8_t> halfPicture(static_cast<size_t>(k_SquareSide / 2) * (k_SquareSide / 2), 0x55);
	PutPicture(half, halfPicture, k_TwentiethGod);
	const auto halfTexel = static_cast<size_t>((k_TwentiethGod / k_WideSquaresInARow) * (k_SquareSide / 2)) * k_HalfSide +
	                       (k_TwentiethGod % k_WideSquaresInARow) * (k_SquareSide / 2);
	EXPECT_EQ(half.alpha[halfTexel], 0x55);
	EXPECT_EQ(half.alpha[halfTexel - 1], 9);
}
