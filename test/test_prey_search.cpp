/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <algorithm>
#include <vector>

#include <gtest/gtest.h>

#include "Animals/PreySearch.h"

using namespace openblack;
using openblack::animals::prey_search::SpiralOrder;

namespace
{
/// The cells that the spiral search looks at, in its order, made step by step as the search does
std::vector<glm::ivec2> SpiralCells(glm::ivec2 centre, uint32_t cells)
{
	std::vector<glm::ivec2> order;
	auto cell = centre;
	map_coords::Spiral spiral;
	for (uint32_t i = 0; i < cells; ++i)
	{
		order.push_back(cell);
		const auto& step = spiral.Next();
		cell += glm::ivec2(step.x, step.z);
	}
	return order;
}
} // namespace

TEST(PreySearch, EachCellOfTheSpiralHasItsPlaceInTheOrder)
{
	for (const uint32_t cells : {1u, 2u, 3u, 7u, 9u, 25u, 26u, 100u, 401u})
	{
		const glm::ivec2 centre {500, 700};
		const SpiralOrder order(centre, cells);
		const auto expected = SpiralCells(centre, cells);
		for (uint32_t i = 0; i < expected.size(); ++i)
		{
			ASSERT_EQ(order.RankOf(expected[i]), i) << cells << " cells, cell " << i;
		}
	}
}

TEST(PreySearch, TheSearchDoesNotLookAtOtherCells)
{
	const glm::ivec2 centre {10, 20};
	constexpr uint32_t k_Cells = 26;
	const SpiralOrder order(centre, k_Cells);
	const auto expected = SpiralCells(centre, k_Cells);
	uint32_t found = 0;
	for (int32_t x = centre.x - 8; x <= centre.x + 8; ++x)
	{
		for (int32_t y = centre.y - 8; y <= centre.y + 8; ++y)
		{
			const bool inSpiral = std::ranges::find(expected, glm::ivec2(x, y)) != expected.end();
			EXPECT_EQ(order.RankOf({x, y}).has_value(), inSpiral) << x << ", " << y;
			found += inSpiral ? 1 : 0;
		}
	}
	EXPECT_EQ(found, k_Cells);
}

TEST(PreySearch, ASearchOfNoCellsFindsNoCell)
{
	const SpiralOrder order({3, 4}, 0);
	EXPECT_FALSE(order.RankOf({3, 4}).has_value());
}

TEST(PreySearch, CellsOffTheMapEdgeKeepTheirPlace)
{
	// The spiral of a hunter at the corner of the map steps to cells below 0, where no living can be
	const SpiralOrder order({0, 0}, 9);
	EXPECT_EQ(order.RankOf({0, 0}), 0u);
	EXPECT_EQ(order.RankOf({-1, 0}), 1u);
	EXPECT_EQ(order.RankOf({-1, -1}), 2u);
	EXPECT_FALSE(order.RankOf({2, 0}).has_value());
}
