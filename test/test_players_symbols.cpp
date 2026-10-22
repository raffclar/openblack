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
