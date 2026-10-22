/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>
#include <span>
#include <vector>

/// The sheet of the players' symbols that the world shows: over a town's artefacts, in the miracles and in the
/// villages' belief. The sheet has a square of 64 by 64 texels for each player, four squares in a row, the first
/// player's square at the top left. At the start of a land, the square of the player of this computer gets the symbol
/// that the player's profile has. The player's own picture for that symbol takes the place of the symbol. The other
/// squares stay as the sheet's file has them.
namespace openblack::players_symbols
{

/// The sheet is a square of this many texels a side
constexpr uint32_t k_SheetSide = 256;
/// Each player's square, and each design's square in the sheet of designs, is this many texels a side
constexpr uint32_t k_SquareSide = 64;
/// The squares in a row of a sheet
constexpr uint32_t k_SquaresInARow = k_SheetSide / k_SquareSide;

/// A sheet of red, green and blue a texel at a time, a row at a time from the top, and its alpha beside it
struct Sheet
{
	std::vector<uint8_t> colours;
	std::vector<uint8_t> alpha;

	/// True when the colours and the alpha are each the size of a sheet
	[[nodiscard]] bool Whole() const;
};

/// Copies the square of a design in the sheet of designs to the square of a player
void CopyDesign(Sheet& players, const Sheet& designs, uint32_t design, uint32_t player);

/// Puts a grey picture of 64 by 64 texels, a row at a time from the top, in the square of a player. Each texel takes
/// the grey for its colours and for its alpha.
void PutPicture(Sheet& players, std::span<const uint8_t> greys, uint32_t player);

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
