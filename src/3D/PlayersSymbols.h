/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

/// The sheet of the players' symbols that the world shows: over a town's artefacts, in the miracles and in the
/// villages' belief. The sheet has a square of 64 by 64 texels for each player, four squares in a row, the first
/// player's square at the top left. At the start of a land, the square of the player of this computer gets the symbol
/// that the player's profile has. The player's own picture for that symbol takes the place of the symbol. The square
/// of each computer player gets the picture of its god. The other squares stay as the sheet's file has them. A network game
/// with more player numbers than the squares of the file has a wider sheet: eight squares in a row, with the square of each
/// player number.
namespace openblack::players_symbols
{

/// The sheet is a square of this many texels a side
constexpr uint32_t k_SheetSide = 256;
/// Each player's square, and each design's square in the sheet of designs, is this many texels a side
constexpr uint32_t k_SquareSide = 64;
/// The squares in a row of a sheet
constexpr uint32_t k_SquaresInARow = k_SheetSide / k_SquareSide;
/// The squares in a row of the wider sheet of a network game with more player numbers than the squares of the file
constexpr uint32_t k_WideSquaresInARow = 2 * k_SquaresInARow;

/// The squares in a row of the sheet for a game with this count of player numbers. A land and each game with up to 16
/// player numbers keep the sheet of the file. A game with more player numbers has the wider sheet.
[[nodiscard]] uint32_t SquaresInARowFor(size_t playerNumbers);

/// A sheet of red, green and blue a texel at a time, a row at a time from the top, and its alpha beside it
struct Sheet
{
	std::vector<uint8_t> colours;
	std::vector<uint8_t> alpha;
	/// The squares in a row, and also the rows of squares
	uint32_t squaresInARow {k_SquaresInARow};

	/// True when the colours and the alpha are each the size of a sheet of full size
	[[nodiscard]] bool Whole() const;
};

/// A sheet with the given squares in a row, and with each square of the sheet in the place of the same number. The new
/// squares are clear. A sheet that is not whole, or that has the same squares in a row, comes back as it is.
[[nodiscard]] Sheet Widen(Sheet sheet, uint32_t squaresInARow);

/// Copies the square of a design in the sheet of designs to the square of a player
void CopyDesign(Sheet& players, const Sheet& designs, uint32_t design, uint32_t player);

/// Puts a square grey picture, a row at a time from the top, in the square of a player. Each texel takes the grey for
/// its colours and for its alpha. The side of the picture is a quarter of the side of the sheet: 64 texels for a sheet
/// of full size, and 32 texels for a sheet at half size. A picture of a different size puts nothing.
void PutPicture(Sheet& players, std::span<const uint8_t> greys, uint32_t player);

/// The pictures of the computer gods, by their number: Lethys, Khazar and Nemesis. Each is a file in the textures
/// folder with the name and the ending ".cps", of 64 by 64 greys.
constexpr std::array<std::string_view, 3> k_GodPictures {"Lethis", "Kazarr", "nemesis"};
/// At the low detail levels each god's picture comes from a file of 32 by 32 greys, with this letter after the name
constexpr std::string_view k_LowDetailLetter = "L";

/// The picture of the god that the computer plays as a player, at the start of a land. The picture is the player's
/// number less one. On the third land, and on the fifth land, the second player takes Khazar and Nemesis. A number
/// below the first picture gives the first picture, and a number after the last picture gives the last picture.
[[nodiscard]] uint32_t GodPictureOf(uint32_t player, int32_t landNumber);

/// Gives the player of this computer the symbol of the profile, or the player's own picture for it when there is one
void PutProfileSymbol(Sheet& players, const Sheet& designs, uint32_t symbol, uint32_t player,
                      std::optional<std::span<const uint8_t>> ownPicture);

/// The symbol of the player of this computer: the symbol that a test puts in place of the profile's, else the profile's
/// symbol. There is no symbol without a profile.
[[nodiscard]] std::optional<uint32_t> ChosenSymbol(std::optional<int> profileSymbol, std::optional<uint32_t> symbolInPlace);

/// The sheet that the world shows from the start of a game, a new game or a load: the sheet of the file, with the chosen
/// symbol in the square of the player of this computer. A save keeps no part of the sheet, so a load makes it again.
[[nodiscard]] Sheet MakeSheet(Sheet file, const Sheet& designs, std::optional<uint32_t> symbol, uint32_t player,
                              std::optional<std::span<const uint8_t>> ownPicture);

} // namespace openblack::players_symbols
