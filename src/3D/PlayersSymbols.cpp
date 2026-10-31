/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PlayersSymbols.h"

#include <cmath>

#include <algorithm>

#include "PlayerSlots.h"

using namespace openblack::players_symbols;

namespace
{

/// The texels on a side of a sheet of full size
[[nodiscard]] uint32_t SideOf(const Sheet& sheet)
{
	return sheet.squaresInARow * k_SquareSide;
}

/// The texel at the top left of a square of a sheet of full size
[[nodiscard]] size_t SquareStart(const Sheet& sheet, uint32_t square)
{
	const auto column = square % sheet.squaresInARow;
	const auto row = square / sheet.squaresInARow;
	return static_cast<size_t>(row) * k_SquareSide * SideOf(sheet) + static_cast<size_t>(column) * k_SquareSide;
}

/// A square that is in the sheet
[[nodiscard]] bool InSheet(const Sheet& sheet, uint32_t square)
{
	return square < sheet.squaresInARow * sheet.squaresInARow;
}

/// Copies a square of one sheet of full size to a square of another
void CopySquare(Sheet& to, uint32_t toSquare, const Sheet& from, uint32_t fromSquare)
{
	const auto source = SquareStart(from, fromSquare);
	const auto target = SquareStart(to, toSquare);
	for (uint32_t row = 0; row < k_SquareSide; ++row)
	{
		const auto sourceRow = source + static_cast<size_t>(row) * SideOf(from);
		const auto targetRow = target + static_cast<size_t>(row) * SideOf(to);
		std::copy_n(from.colours.begin() + static_cast<std::ptrdiff_t>(sourceRow * 3), k_SquareSide * 3,
		            to.colours.begin() + static_cast<std::ptrdiff_t>(targetRow * 3));
		std::copy_n(from.alpha.begin() + static_cast<std::ptrdiff_t>(sourceRow), k_SquareSide,
		            to.alpha.begin() + static_cast<std::ptrdiff_t>(targetRow));
	}
}

} // namespace

uint32_t openblack::players_symbols::SquaresInARowFor(size_t playerNumbers)
{
	constexpr auto k_FileSquares = static_cast<size_t>(k_SquaresInARow) * k_SquaresInARow;
	return player_slots::PlayerNumbersOf(playerNumbers) <= k_FileSquares ? k_SquaresInARow : k_WideSquaresInARow;
}

bool Sheet::Whole() const
{
	const auto texels = static_cast<size_t>(SideOf(*this)) * SideOf(*this);
	return squaresInARow != 0 && colours.size() == texels * 3 && alpha.size() == texels;
}

Sheet openblack::players_symbols::Widen(Sheet sheet, uint32_t squaresInARow)
{
	if (!sheet.Whole() || squaresInARow == 0 || squaresInARow == sheet.squaresInARow)
	{
		return sheet;
	}
	const auto side = static_cast<size_t>(squaresInARow) * k_SquareSide;
	Sheet wide {.colours = std::vector<uint8_t>(side * side * 3),
	            .alpha = std::vector<uint8_t>(side * side),
	            .squaresInARow = squaresInARow};
	const auto squares = std::min(sheet.squaresInARow * sheet.squaresInARow, squaresInARow * squaresInARow);
	for (uint32_t square = 0; square < squares; ++square)
	{
		CopySquare(wide, square, sheet, square);
	}
	return wide;
}

void openblack::players_symbols::CopyDesign(Sheet& players, const Sheet& designs, uint32_t design, uint32_t player)
{
	if (!players.Whole() || !designs.Whole() || !InSheet(designs, design) || !InSheet(players, player))
	{
		return;
	}
	CopySquare(players, player, designs, design);
}

void openblack::players_symbols::PutPicture(Sheet& players, std::span<const uint8_t> greys, uint32_t player)
{
	// The side of the square picture gives the side of the sheet: a sheet of full size, or a sheet at half size
	const auto side = static_cast<uint32_t>(std::sqrt(static_cast<double>(greys.size())));
	const auto sheetSide = side * players.squaresInARow;
	const auto texels = static_cast<size_t>(sheetSide) * sheetSide;
	if (side == 0 || static_cast<size_t>(side) * side != greys.size() || players.colours.size() != texels * 3 ||
	    players.alpha.size() != texels || !InSheet(players, player))
	{
		return;
	}
	const auto to = static_cast<size_t>(player / players.squaresInARow) * side * sheetSide +
	                static_cast<size_t>(player % players.squaresInARow) * side;
	for (uint32_t row = 0; row < side; ++row)
	{
		for (uint32_t column = 0; column < side; ++column)
		{
			const auto grey = greys[static_cast<size_t>(row) * side + column];
			const auto texel = to + static_cast<size_t>(row) * sheetSide + column;
			players.colours.at(texel * 3) = grey;
			players.colours.at(texel * 3 + 1) = grey;
			players.colours.at(texel * 3 + 2) = grey;
			players.alpha.at(texel) = grey;
		}
	}
}

uint32_t openblack::players_symbols::GodPictureOf(uint32_t player, int32_t landNumber)
{
	constexpr uint32_t k_SecondPlayer = 1;
	constexpr int32_t k_KhazarLand = 3;
	constexpr int32_t k_NemesisLand = 5;
	constexpr int64_t k_Khazar = 1;
	constexpr int64_t k_Nemesis = 2;
	auto picture = static_cast<int64_t>(player) - 1;
	if (landNumber == k_KhazarLand && player == k_SecondPlayer)
	{
		picture = k_Khazar;
	}
	if (landNumber == k_NemesisLand && player == k_SecondPlayer)
	{
		picture = k_Nemesis;
	}
	return static_cast<uint32_t>(std::clamp<int64_t>(picture, 0, static_cast<int64_t>(k_GodPictures.size()) - 1));
}

void openblack::players_symbols::PutProfileSymbol(Sheet& players, const Sheet& designs, uint32_t symbol, uint32_t player,
                                                  std::optional<std::span<const uint8_t>> ownPicture)
{
	if (ownPicture.has_value())
	{
		PutPicture(players, *ownPicture, player);
		return;
	}
	CopyDesign(players, designs, symbol, player);
}

std::optional<uint32_t> openblack::players_symbols::ChosenSymbol(std::optional<int> profileSymbol,
                                                                 std::optional<uint32_t> symbolInPlace)
{
	if (symbolInPlace.has_value())
	{
		return symbolInPlace;
	}
	if (!profileSymbol.has_value())
	{
		return std::nullopt;
	}
	// A number outside the sheet of designs copies nothing
	return static_cast<uint32_t>(*profileSymbol);
}

Sheet openblack::players_symbols::MakeSheet(Sheet file, const Sheet& designs, std::optional<uint32_t> symbol, uint32_t player,
                                            std::optional<std::span<const uint8_t>> ownPicture)
{
	if (symbol.has_value())
	{
		PutProfileSymbol(file, designs, *symbol, player, ownPicture);
	}
	return file;
}
