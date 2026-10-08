/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The map grid's sets (ECS/MapGridCells): clearing only the cells that were filled leaves every set as clearing all of
// them does: empty, with the same buckets, and the next inserts iterate in the same order.

#include <cstdint>

#include <array>
#include <memory>
#include <stdexcept>
#include <unordered_set>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/MapGridCells.h"

using openblack::ecs::MapGridCells;

namespace
{
constexpr size_t k_Cells = 64;

/// The grid as it was: one set per cell, every one cleared
using FullGrid = std::array<std::unordered_set<entt::entity>, k_Cells>;

void ClearAll(FullGrid& grid)
{
	for (auto& set : grid)
	{
		set.clear();
	}
}

/// A turn's inserts: a few cells crowded, some with one entity, most empty, entities repeated across cells
std::vector<std::pair<size_t, entt::entity>> Turn(uint32_t seed)
{
	std::vector<std::pair<size_t, entt::entity>> inserts;
	uint32_t state = seed;
	const auto draw = [&state](uint32_t below) {
		state = state * 1664525u + 1013904223u;
		return (state >> 8) % below;
	};
	const auto count = 20 + draw(200);
	for (uint32_t i = 0; i < count; ++i)
	{
		const size_t cell = draw(4) == 0 ? draw(k_Cells) : draw(6); // most in the first few cells
		inserts.emplace_back(cell, static_cast<entt::entity>(draw(500)));
	}
	return inserts;
}

std::vector<entt::entity> Order(const std::unordered_set<entt::entity>& set)
{
	return {set.begin(), set.end()};
}
} // namespace

TEST(MapGridCells, ClearingTheFilledCellsIsAFullClear)
{
	auto grid = std::make_unique<MapGridCells<k_Cells>>();
	auto full = std::make_unique<FullGrid>();
	for (uint32_t turn = 1; turn <= 30; ++turn)
	{
		grid->Clear();
		ClearAll(*full);
		for (size_t cell = 0; cell < k_Cells; ++cell)
		{
			ASSERT_TRUE(grid->At(cell).empty()) << turn << " " << cell;
			ASSERT_EQ(grid->At(cell).bucket_count(), (*full)[cell].bucket_count()) << turn << " " << cell;
		}
		ASSERT_TRUE(grid->Occupied().empty());
		for (const auto& [cell, entity] : Turn(turn))
		{
			grid->Insert(cell, entity);
			(*full)[cell].insert(entity);
		}
		for (size_t cell = 0; cell < k_Cells; ++cell)
		{
			ASSERT_EQ(Order(grid->At(cell)), Order((*full)[cell])) << turn << " " << cell;
			ASSERT_EQ(grid->At(cell).bucket_count(), (*full)[cell].bucket_count()) << turn << " " << cell;
		}
	}
}

TEST(MapGridCells, EachFilledCellIsListedOnce)
{
	MapGridCells<8> grid;
	grid.Insert(5, static_cast<entt::entity>(1));
	grid.Insert(2, static_cast<entt::entity>(1));
	grid.Insert(5, static_cast<entt::entity>(2));
	grid.Insert(5, static_cast<entt::entity>(1));
	EXPECT_EQ(grid.Occupied(), (std::vector<uint32_t> {5, 2}));
	EXPECT_EQ(grid.At(5).size(), 2u);
	grid.Clear();
	EXPECT_TRUE(grid.Occupied().empty());
	EXPECT_TRUE(grid.At(5).empty());
	EXPECT_TRUE(grid.At(2).empty());
}

TEST(MapGridCells, ACellOffTheGridThrows)
{
	MapGridCells<8> grid;
	EXPECT_THROW(grid.Insert(8, static_cast<entt::entity>(1)), std::out_of_range);
	EXPECT_THROW(static_cast<void>(grid.At(8)), std::out_of_range);
	EXPECT_TRUE(grid.Occupied().empty());
}
