/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <algorithm>
#include <random>
#include <vector>

#include <gtest/gtest.h>

#include "Graphics/CasterBlocks.h"
#include "Visual/TreeShadows.h"

using openblack::graphics::CasterBlocks;
namespace tree_shadows = openblack::graphics::tree_shadows;

namespace
{

constexpr uint32_t k_MapCells = 512;

uint32_t CellIndex(int x, int z)
{
	return (static_cast<uint32_t>(x) * k_MapCells) + static_cast<uint32_t>(z);
}

/// Whether the shadow of a cell falls into one of the blocks, by a look at each block
bool CastsInto(const std::vector<uint32_t>& cells, const std::vector<glm::ivec2>& blocks)
{
	return std::ranges::any_of(cells, [&blocks](uint32_t index) {
		const auto alongX = tree_shadows::BlocksTakingShadowFrom(static_cast<int>(index / k_MapCells));
		const auto alongZ = tree_shadows::BlocksTakingShadowFrom(static_cast<int>(index % k_MapCells));
		return std::ranges::any_of(blocks, [&](glm::ivec2 block) {
			return block.x >= alongX.x && block.x <= alongX.y && block.y >= alongZ.x && block.y <= alongZ.y;
		});
	});
}

} // namespace

TEST(CasterBlocks, FindsTheCastersOfACellNearTheEdgeOfABlock)
{
	CasterBlocks casters;
	const auto caster = entt::entity {5};
	// A cell near the start of a block casts also into the block before it
	const std::vector<uint32_t> cells {CellIndex(tree_shadows::k_BlockCells, 40)};
	casters.Add(caster, cells);
	const auto alongX = tree_shadows::BlocksTakingShadowFrom(tree_shadows::k_BlockCells);
	ASSERT_LT(alongX.x, alongX.y);
	for (int x = alongX.x; x <= alongX.y; ++x)
	{
		const std::vector<glm::ivec2> blocks {{x, 40 / tree_shadows::k_BlockCells}};
		EXPECT_EQ(casters.CastingInto(blocks), std::vector<entt::entity> {caster});
	}
	const std::vector<glm::ivec2> far {{alongX.y + 1, 40 / tree_shadows::k_BlockCells}};
	EXPECT_TRUE(casters.CastingInto(far).empty());
}

TEST(CasterBlocks, GivesTheSameCastersAsALookAtEachCaster)
{
	std::mt19937 random(7);
	std::uniform_int_distribution<int> place(0, 95);
	std::uniform_int_distribution<int> size(1, 3);
	CasterBlocks casters;
	std::vector<std::vector<uint32_t>> cellsOf(300);
	for (size_t i = 0; i < cellsOf.size(); ++i)
	{
		const int x = place(random);
		const int z = place(random);
		const int across = size(random);
		for (int dx = 0; dx < across; ++dx)
		{
			cellsOf[i].push_back(CellIndex(x + dx, z));
		}
		casters.Add(static_cast<entt::entity>(i), cellsOf[i]);
	}
	// Some casters go, and some move to other cells
	for (size_t i = 0; i < cellsOf.size(); i += 7)
	{
		casters.Remove(static_cast<entt::entity>(i), cellsOf[i]);
		cellsOf[i].clear();
	}
	for (size_t i = 3; i < cellsOf.size(); i += 11)
	{
		casters.Remove(static_cast<entt::entity>(i), cellsOf[i]);
		cellsOf[i] = {CellIndex(place(random), place(random))};
		casters.Add(static_cast<entt::entity>(i), cellsOf[i]);
	}

	for (int test = 0; test < 40; ++test)
	{
		std::vector<glm::ivec2> blocks;
		for (int n = 0; n < 1 + (test % 4); ++n)
		{
			blocks.emplace_back(place(random) / tree_shadows::k_BlockCells, place(random) / tree_shadows::k_BlockCells);
		}
		std::vector<entt::entity> expected;
		for (size_t i = 0; i < cellsOf.size(); ++i)
		{
			if (CastsInto(cellsOf[i], blocks))
			{
				expected.push_back(static_cast<entt::entity>(i));
			}
		}
		EXPECT_EQ(casters.CastingInto(blocks), expected) << "test " << test;
	}
}

TEST(CasterBlocks, GivesEachCasterOneTime)
{
	CasterBlocks casters;
	// A caster that covers many cells of one block
	const std::vector<uint32_t> cells {CellIndex(20, 20), CellIndex(21, 20), CellIndex(22, 21)};
	casters.Add(entt::entity {2}, cells);
	casters.Add(entt::entity {1}, std::vector<uint32_t> {CellIndex(20, 21)});
	const std::vector<glm::ivec2> blocks {{1, 1}, {0, 1}, {1, 0}, {0, 0}};
	EXPECT_EQ(casters.CastingInto(blocks), (std::vector<entt::entity> {entt::entity {1}, entt::entity {2}}));

	casters.Remove(entt::entity {2}, cells);
	EXPECT_EQ(casters.CastingInto(blocks), std::vector<entt::entity> {entt::entity {1}});
	casters.Clear();
	EXPECT_TRUE(casters.CastingInto(blocks).empty());
}
