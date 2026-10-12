/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <map>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/MapCells.h"
#include "Scenery/ObjectDrawList.h"

using namespace openblack;
using namespace openblack::object_draw_list;
using openblack::ecs::map_cells::BlockLists;

namespace
{
constexpr entt::entity E(uint32_t id)
{
	return static_cast<entt::entity>(id);
}

std::vector<entt::entity> ToVector(std::span<const entt::entity> span)
{
	return {span.begin(), span.end()};
}

/// A cell of a block, by the block's column and row
constexpr glm::ivec2 CellOf(glm::ivec2 block, glm::ivec2 within = {0, 0})
{
	return block * ecs::map_cells::k_BlockCells + within;
}

/// Blocks with their objects, as the map keeps them
class FakeBlocks
{
public:
	std::map<std::pair<int, int>, std::vector<entt::entity>> objects;

	[[nodiscard]] std::span<const entt::entity> On(glm::ivec2 block) const
	{
		const auto found = objects.find({block.x, block.y});
		return found == objects.end() ? std::span<const entt::entity> {} : std::span<const entt::entity>(found->second);
	}
};

std::vector<entt::entity> Make(std::span<const NearBlock> blocks, const FakeBlocks& fake, std::span<const entt::entity> off,
                               size_t most = k_MostObjects, std::vector<entt::entity> leftOut = {})
{
	return MakeList(
	    blocks, [&fake](glm::ivec2 block) { return fake.On(block); }, off,
	    [&leftOut](entt::entity entity) { return std::ranges::find(leftOut, entity) != leftOut.end(); }, most);
}
} // namespace

TEST(ObjectDrawList, BlocksGoNearToFarAndOfTwoAsFarTheLaterFirst)
{
	const std::vector<NearBlock> inLandOrder {
	    {.block = {0, 0}, .distance = 300.0f},
	    {.block = {1, 0}, .distance = 100.0f},
	    {.block = {2, 0}, .distance = 300.0f},
	    {.block = {3, 0}, .distance = 200.0f},
	};
	const auto sorted = NearToFar(inLandOrder);
	ASSERT_EQ(sorted.size(), 4u);
	EXPECT_EQ(sorted[0].block, glm::ivec2(1, 0));
	EXPECT_EQ(sorted[1].block, glm::ivec2(3, 0));
	// Each block goes in front of the first already placed that is as far: the later of two as far comes first
	EXPECT_EQ(sorted[2].block, glm::ivec2(2, 0));
	EXPECT_EQ(sorted[3].block, glm::ivec2(0, 0));
}

TEST(ObjectDrawList, TheWalkStopsAtTheFirstBlockOutOfRange)
{
	FakeBlocks fake;
	fake.objects[{0, 0}] = {E(1), E(2)};
	fake.objects[{1, 0}] = {E(3)};
	fake.objects[{2, 0}] = {E(4)};
	const std::vector<NearBlock> blocks {
	    {.block = {1, 0}, .distance = 50.0f},
	    {.block = {0, 0}, .distance = 699.0f},
	    {.block = {2, 0}, .distance = 700.0f},
	};
	EXPECT_EQ(Make(blocks, fake, {}), (std::vector {E(3), E(1), E(2)}));
}

TEST(ObjectDrawList, TheThingsOffTheBlocksComeAfterTheNearestBlock)
{
	FakeBlocks fake;
	fake.objects[{0, 0}] = {E(1)};
	fake.objects[{1, 0}] = {E(2)};
	const std::vector off {E(9), E(8)};
	const std::vector<NearBlock> blocks {{.block = {0, 0}, .distance = 10.0f}, {.block = {1, 0}, .distance = 20.0f}};
	EXPECT_EQ(Make(blocks, fake, off), (std::vector {E(1), E(9), E(8), E(2)}));
	// With no block in range, nothing is listed, not even what is off the blocks
	const std::vector<NearBlock> far {{.block = {0, 0}, .distance = 800.0f}};
	EXPECT_TRUE(Make(far, fake, off).empty());
}

TEST(ObjectDrawList, AThingOnSeveralBlocksIsListedOnceAtTheNearest)
{
	FakeBlocks fake;
	fake.objects[{0, 0}] = {E(1), E(5)};
	fake.objects[{1, 0}] = {E(5), E(2)};
	const std::vector<NearBlock> blocks {{.block = {1, 0}, .distance = 10.0f}, {.block = {0, 0}, .distance = 20.0f}};
	EXPECT_EQ(Make(blocks, fake, {}), (std::vector {E(5), E(2), E(1)}));
}

TEST(ObjectDrawList, TheNearestFillItAndTheRestAreLeftOut)
{
	FakeBlocks fake;
	fake.objects[{0, 0}] = {E(1), E(2), E(3)};
	fake.objects[{1, 0}] = {E(4), E(5)};
	const std::vector<NearBlock> blocks {{.block = {1, 0}, .distance = 10.0f}, {.block = {0, 0}, .distance = 20.0f}};
	// Within a block its own order decides what is left out
	EXPECT_EQ(Make(blocks, fake, {}, 4), (std::vector {E(4), E(5), E(1), E(2)}));
	// Things drawn on their own don't take a place
	EXPECT_EQ(Make(blocks, fake, {}, 4, {E(5)}), (std::vector {E(4), E(1), E(2), E(3)}));
}

TEST(ObjectDrawList, TheListHoldsAtMost2999)
{
	FakeBlocks fake;
	auto& many = fake.objects[{0, 0}];
	for (uint32_t i = 0; i < 4000; ++i)
	{
		many.push_back(E(i));
	}
	const std::vector<NearBlock> blocks {{.block = {0, 0}, .distance = 10.0f}};
	const auto list = Make(blocks, fake, {});
	ASSERT_EQ(list.size(), 2999u);
	EXPECT_EQ(list.back(), E(2998));
}

TEST(BlockLists, ThingsGoToTheBackOfTheirBlock)
{
	BlockLists lists;
	lists.Insert(CellOf({2, 3}, {1, 1}), E(1));
	lists.Insert(CellOf({2, 3}, {15, 15}), E(2));
	lists.Insert(CellOf({4, 3}), E(3));
	EXPECT_EQ(ToVector(lists.OnBlock({2, 3})), (std::vector {E(1), E(2)}));
	EXPECT_EQ(ToVector(lists.OnBlock({4, 3})), (std::vector {E(3)}));
	EXPECT_TRUE(lists.OnBlock({0, 0}).empty());
}

TEST(BlockLists, ABuildingOverSeveralCellsOfABlockIsListedOnce)
{
	BlockLists lists;
	// Its cells, x by x and z by z, crossing into the next block and back
	lists.Insert(CellOf({0, 0}, {14, 15}), E(7));
	lists.Insert(CellOf({0, 1}, {14, 0}), E(7));
	lists.Insert(CellOf({0, 0}, {15, 15}), E(7));
	lists.Insert(CellOf({0, 1}, {15, 0}), E(7));
	EXPECT_EQ(ToVector(lists.OnBlock({0, 0})), (std::vector {E(7)}));
	EXPECT_EQ(ToVector(lists.OnBlock({0, 1})), (std::vector {E(7)}));
	// Taken out of every cell, it leaves both
	lists.Remove(CellOf({0, 0}, {14, 15}), E(7));
	lists.Remove(CellOf({0, 1}, {14, 0}), E(7));
	lists.Remove(CellOf({0, 0}, {15, 15}), E(7));
	lists.Remove(CellOf({0, 1}, {15, 0}), E(7));
	EXPECT_TRUE(lists.OnBlock({0, 0}).empty());
	EXPECT_TRUE(lists.OnBlock({0, 1}).empty());
}

TEST(BlockLists, TheLastTakesThePlaceOfOneTakenOut)
{
	BlockLists lists;
	for (uint32_t i = 1; i <= 4; ++i)
	{
		lists.Insert(CellOf({1, 1}, {static_cast<int>(i), 0}), E(i));
	}
	lists.Remove(CellOf({1, 1}, {2, 0}), E(2));
	EXPECT_EQ(ToVector(lists.OnBlock({1, 1})), (std::vector {E(1), E(4), E(3)}));
	// Moving to another cell of the block takes it out and puts it at the back
	lists.Remove(CellOf({1, 1}, {1, 0}), E(1));
	lists.Insert(CellOf({1, 1}, {1, 1}), E(1));
	EXPECT_EQ(ToVector(lists.OnBlock({1, 1})), (std::vector {E(3), E(4), E(1)}));
}

TEST(BlockLists, OneTakenOutOfABlockItIsNotInDropsTheLast)
{
	BlockLists lists;
	lists.Insert(CellOf({1, 1}), E(1));
	lists.Insert(CellOf({1, 1}), E(2));
	lists.Remove(CellOf({1, 1}), E(9));
	EXPECT_EQ(ToVector(lists.OnBlock({1, 1})), (std::vector {E(1)}));
	// Nothing comes out of an empty block
	lists.Remove(CellOf({1, 1}), E(1));
	lists.Remove(CellOf({1, 1}), E(3));
	EXPECT_TRUE(lists.OnBlock({1, 1}).empty());
}

TEST(BlockLists, CellsOfNoLandBlockShareOneList)
{
	BlockLists lists;
	const std::vector<glm::ivec2> land {{1, 1}, {2, 1}};
	lists.SetLandBlocks(land);
	EXPECT_TRUE(lists.LandBlocksKnown());
	lists.Insert(CellOf({1, 1}), E(1));
	lists.Insert(CellOf({5, 5}), E(2));
	lists.Insert(CellOf({0, 9}), E(3));
	EXPECT_EQ(ToVector(lists.OnBlock({1, 1})), (std::vector {E(1)}));
	EXPECT_TRUE(lists.OnBlock({5, 5}).empty());
	EXPECT_EQ(ToVector(lists.OffBlocks()), (std::vector {E(2), E(3)}));
	lists.Remove(CellOf({5, 5}), E(2));
	EXPECT_EQ(ToVector(lists.OffBlocks()), (std::vector {E(3)}));
}

TEST(BlockLists, TheCellListsKeepTheBlocks)
{
	ecs::map_cells::CellLists cells;
	const auto cell = *cells.IndexOf(CellOf({3, 4}, {2, 2}));
	cells.Insert(cell, E(1), ecs::map_cells::Placement::MobileFront);
	cells.Insert(cell, E(2), ecs::map_cells::Placement::FixedFront);
	EXPECT_EQ(ToVector(cells.Blocks().OnBlock({3, 4})), (std::vector {E(1), E(2)}));
	cells.Remove(cell, E(1));
	EXPECT_EQ(ToVector(cells.Blocks().OnBlock({3, 4})), (std::vector {E(2)}));
}
