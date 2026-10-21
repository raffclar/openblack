/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PlayersSymbols.h"

#include <algorithm>

using namespace openblack::players_symbols;

namespace
{

constexpr size_t k_Texels = static_cast<size_t>(k_SheetSide) * k_SheetSide;
constexpr size_t k_SquareTexels = static_cast<size_t>(k_SquareSide) * k_SquareSide;

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
	if (!players.Whole() || greys.size() != k_SquareTexels || !InSheet(player))
	{
		return;
	}
	const auto to = SquareStart(player);
	for (uint32_t row = 0; row < k_SquareSide; ++row)
	{
		for (uint32_t column = 0; column < k_SquareSide; ++column)
		{
			const auto grey = greys[static_cast<size_t>(row) * k_SquareSide + column];
			const auto texel = to + static_cast<size_t>(row) * k_SheetSide + column;
			players.colours.at(texel * 3) = grey;
			players.colours.at(texel * 3 + 1) = grey;
			players.colours.at(texel * 3 + 2) = grey;
			players.alpha.at(texel) = grey;
		}
	}
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
