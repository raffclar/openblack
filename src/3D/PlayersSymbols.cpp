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

using namespace openblack::players_symbols;

namespace
{

constexpr size_t k_Texels = static_cast<size_t>(k_SheetSide) * k_SheetSide;

/// The texel at the top left of a square
[[nodiscard]] size_t SquareStart(uint32_t square)
{
	const auto column = square % k_SquaresInARow;
	const auto row = square / k_SquaresInARow;
	return static_cast<size_t>(row) * k_SquareSide * k_SheetSide + static_cast<size_t>(column) * k_SquareSide;
}

/// A square that is in the sheet
[[nodiscard]] bool InSheet(uint32_t square)
{
	return square < k_SquaresInARow * k_SquaresInARow;
}

} // namespace

bool Sheet::Whole() const
{
	return colours.size() == k_Texels * 3 && alpha.size() == k_Texels;
}

void openblack::players_symbols::CopyDesign(Sheet& players, const Sheet& designs, uint32_t design, uint32_t player)
{
	if (!players.Whole() || !designs.Whole() || !InSheet(design) || !InSheet(player))
	{
		return;
	}
	const auto from = SquareStart(design);
	const auto to = SquareStart(player);
	for (uint32_t row = 0; row < k_SquareSide; ++row)
	{
		const auto source = from + static_cast<size_t>(row) * k_SheetSide;
		const auto target = to + static_cast<size_t>(row) * k_SheetSide;
		std::copy_n(designs.colours.begin() + static_cast<std::ptrdiff_t>(source * 3), k_SquareSide * 3,
		            players.colours.begin() + static_cast<std::ptrdiff_t>(target * 3));
		std::copy_n(designs.alpha.begin() + static_cast<std::ptrdiff_t>(source), k_SquareSide,
		            players.alpha.begin() + static_cast<std::ptrdiff_t>(target));
	}
}

void openblack::players_symbols::PutPicture(Sheet& players, std::span<const uint8_t> greys, uint32_t player)
{
	// The side of the square picture gives the side of the sheet: a sheet of full size, or a sheet at half size
	const auto side = static_cast<uint32_t>(std::sqrt(static_cast<double>(greys.size())));
	const auto sheetSide = side * k_SquaresInARow;
	const auto texels = static_cast<size_t>(sheetSide) * sheetSide;
	if (side == 0 || static_cast<size_t>(side) * side != greys.size() || players.colours.size() != texels * 3 ||
	    players.alpha.size() != texels || !InSheet(player))
	{
		return;
	}
	const auto to =
	    static_cast<size_t>(player / k_SquaresInARow) * side * sheetSide + static_cast<size_t>(player % k_SquaresInARow) * side;
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
