/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include <ECS/EntityHashes.h>
#include <gtest/gtest.h>

using namespace openblack::synced_state;

namespace
{

std::vector<EntityHash> Entities(size_t count)
{
	std::vector<EntityHash> entities;
	for (size_t i = 0; i < count; ++i)
	{
		entities.push_back({.entity = static_cast<uint32_t>(i * 2), .hash = i * 7 + 1, .kind = "Villager", .position = {}});
	}
	return entities;
}

} // namespace

TEST(EntityHashesTest, EachBlockHasItsEntitiesAndOneChangeChangesOneBlock)
{
	const auto entities = Entities(k_EntitiesInBlock * 2 + 3);
	const auto blocks = BlocksOf(entities);
	ASSERT_EQ(blocks.size(), 3U);
	auto changed = entities;
	changed[k_EntitiesInBlock + 5].hash ^= 1;
	const auto other = BlocksOf(changed);
	EXPECT_EQ(blocks[0], other[0]);
	EXPECT_NE(blocks[1], other[1]);
	EXPECT_EQ(blocks[2], other[2]);
	EXPECT_EQ(DifferentBlocks(blocks, other), std::vector<uint32_t> {1});
	EXPECT_TRUE(BlocksOf({}).empty());
}

TEST(EntityHashesTest, TheBlocksThatDifferAreBoundedAndABlockOfOneSideDiffers)
{
	const std::vector<uint64_t> own {1, 2, 3, 4, 5, 6};
	const std::vector<uint64_t> other {9, 9, 9, 9, 9, 9, 9};
	EXPECT_EQ(DifferentBlocks(own, other).size(), k_MostAskedBlocks);
	const std::vector<uint64_t> shorter {1, 2};
	EXPECT_EQ(DifferentBlocks(own, shorter), (std::vector<uint32_t> {2, 3, 4, 5}));
	EXPECT_TRUE(DifferentBlocks(own, own).empty());
}

TEST(EntityHashesTest, TheEntitiesOfBlocksAreInOrderAndABlockPastTheEndGivesNothing)
{
	const auto entities = Entities(k_EntitiesInBlock + 10);
	const std::vector<uint32_t> blocks {1, 0, 7};
	const auto found = EntitiesOfBlocks(entities, blocks);
	ASSERT_EQ(found.size(), entities.size());
	EXPECT_EQ(found.front().entity, 0U);
	EXPECT_EQ(found.back().entity, entities.back().entity);
}

TEST(EntityHashesTest, TheEntitiesThatDifferNameEachSide)
{
	std::vector<EntityHash> server {{.entity = 1, .hash = 10, .kind = "A", .position = std::array<float, 3> {1, 2, 3}},
	                                {.entity = 2, .hash = 20, .kind = "B", .position = {}},
	                                {.entity = 4, .hash = 40, .kind = "D", .position = {}}};
	std::vector<EntityHash> player {{.entity = 1, .hash = 10, .kind = "A", .position = std::array<float, 3> {1, 2, 3}},
	                                {.entity = 2, .hash = 21, .kind = "B", .position = {}},
	                                {.entity = 3, .hash = 30, .kind = "C", .position = {}}};
	const auto different = DifferentEntities(server, player);
	ASSERT_EQ(different.size(), 3U);
	EXPECT_EQ(different[0].entity, 2U);
	EXPECT_TRUE(different[0].server.has_value() && different[0].player.has_value());
	EXPECT_EQ(different[1].entity, 3U);
	EXPECT_FALSE(different[1].server.has_value());
	EXPECT_EQ(different[2].entity, 4U);
	EXPECT_FALSE(different[2].player.has_value());
	const auto words = Describe("Transform", different);
	EXPECT_NE(words.find("entity 2 (server B hash 0000000000000014; player B hash 0000000000000015)"), std::string::npos);
	EXPECT_NE(words.find("server has no entity"), std::string::npos);
	EXPECT_EQ(Describe("Transform", {}), "Transform: no entity differs");
}

TEST(EntityHashesTest, TheEntitiesThatDifferAreBounded)
{
	const auto server = Entities(100);
	auto player = server;
	for (auto& entity : player)
	{
		entity.hash += 1;
	}
	EXPECT_EQ(DifferentEntities(server, player).size(), k_MostDifferentEntities);
}

TEST(EntityHashesTest, TheKindLeavesOutTheTransformAndKeepsTheFirstNames)
{
	const std::vector<std::string_view> names {"Transform", "Villager", "Mesh", "Footprint", "Hunger", "Thirst"};
	EXPECT_EQ(KindOf(names), "Villager+Mesh+Footprint+Hunger");
	EXPECT_EQ(KindOf(std::vector<std::string_view> {"Transform"}), "");
}
