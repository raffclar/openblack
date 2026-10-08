/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// ForestTreeBuckets: the forests' turn gathers every forest's trees in one pass over the trees. Characterised against
// the walk it replaced (each forest walking every tree and keeping its own, the distances taken during the walk, then
// sorted): for every forest the same count, the same grown trees in the same order, and the same nearest-first list,
// bit for bit, with fake trees in mixed creation order.

#include <cstdint>

#include <algorithm>
#include <bit>
#include <map>
#include <random>
#include <ranges>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Common/GUtilsDistance.h"
#include "ECS/ForestTreeBuckets.h"

using openblack::ecs::ForestTreeBuckets;
using openblack::ecs::GrownByDistance;

namespace
{
/// A tree as the forests' turn reads it, in the order the registry visits it
struct FakeTree
{
	entt::entity entity;
	uint32_t forestId;
	bool growing;
	float scale;
	float maxSize;
	glm::vec3 position;
};

using Forests = std::map<uint32_t, glm::vec3>; // id -> centre

struct ForestTrees
{
	size_t count {0};
	std::vector<std::pair<float, entt::entity>> unsorted;
	std::vector<std::pair<float, entt::entity>> sorted;
};

/// The walk it replaced, copied as it was: each forest walks every tree
std::map<uint32_t, ForestTrees> OldWalk(const Forests& forests, const std::vector<FakeTree>& trees)
{
	std::map<uint32_t, ForestTrees> out;
	for (const auto& [id, centre] : forests)
	{
		std::vector<std::pair<float, entt::entity>> grown;
		size_t count = 0;
		for (const auto& tree : trees)
		{
			if (tree.forestId != id)
			{
				continue;
			}
			++count;
			if (!tree.growing || tree.scale >= tree.maxSize)
			{
				grown.emplace_back(openblack::gutils::GetDistanceInMetres(glm::vec2(tree.position.x, tree.position.z),
				                                                          glm::vec2(centre.x, centre.z)),
				                   tree.entity);
			}
		}
		auto& forest = out[id];
		forest.count = count;
		forest.unsorted = grown;
		std::ranges::sort(grown, [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });
		forest.sorted = grown;
	}
	return out;
}

/// The one pass
std::map<uint32_t, ForestTrees> NewPass(const Forests& forests, const std::vector<FakeTree>& trees)
{
	ForestTreeBuckets buckets;
	buckets.Reset(forests | std::views::keys);
	for (const auto& tree : trees)
	{
		buckets.Add(tree.forestId, tree.entity, !tree.growing || tree.scale >= tree.maxSize,
		            glm::vec2(tree.position.x, tree.position.z));
	}
	std::map<uint32_t, ForestTrees> out;
	for (const auto& [id, centre] : forests)
	{
		const auto& bucket = buckets.Of(id);
		auto& forest = out[id];
		forest.count = bucket.count;
		for (const auto& tree : bucket.grown)
		{
			forest.unsorted.emplace_back(openblack::gutils::GetDistanceInMetres(tree.position, glm::vec2(centre.x, centre.z)),
			                             tree.entity);
		}
		forest.sorted = GrownByDistance(bucket.grown, centre);
	}
	return out;
}

void ExpectSame(const std::vector<std::pair<float, entt::entity>>& expected,
                const std::vector<std::pair<float, entt::entity>>& actual, uint32_t id)
{
	ASSERT_EQ(expected.size(), actual.size()) << "forest " << id;
	for (size_t i = 0; i < expected.size(); ++i)
	{
		EXPECT_EQ(std::bit_cast<uint32_t>(expected[i].first), std::bit_cast<uint32_t>(actual[i].first))
		    << "forest " << id << " tree " << i;
		EXPECT_EQ(expected[i].second, actual[i].second) << "forest " << id << " tree " << i;
	}
}

void ExpectSameAsTheOldWalk(const Forests& forests, const std::vector<FakeTree>& trees)
{
	const auto expected = OldWalk(forests, trees);
	const auto actual = NewPass(forests, trees);
	ASSERT_EQ(expected.size(), actual.size());
	for (const auto& [id, forest] : expected)
	{
		const auto& got = actual.at(id);
		EXPECT_EQ(forest.count, got.count) << "forest " << id;
		ExpectSame(forest.unsorted, got.unsorted, id);
		ExpectSame(forest.sorted, got.sorted, id);
	}
}

entt::entity E(uint32_t value)
{
	return static_cast<entt::entity>(value);
}
} // namespace

TEST(ForestTreeBuckets, EachForestSeesItsTreesInThePassOrder)
{
	// Forests 2, 5, 9 and an empty 7; trees of 3 and 11 belong to no forest in the list
	const Forests forests {
	    {2, {10.0f, 0.0f, 10.0f}}, {5, {40.0f, 3.0f, 0.0f}}, {7, {0.0f, 0.0f, 0.0f}}, {9, {-5.0f, 0.0f, 25.0f}}};
	const std::vector<FakeTree> trees {
	    {E(14), 5, false, 1.0f, 1.0f, {41.0f, 0.0f, 2.0f}},
	    {E(3), 2, true, 0.5f, 1.0f, {11.0f, 0.0f, 10.0f}}, // still growing: counted, not grown
	    {E(8), 9, false, 0.7f, 1.0f, {-5.0f, 0.0f, 30.0f}},
	    {E(21), 3, false, 1.0f, 1.0f, {0.0f, 0.0f, 0.0f}},
	    {E(1), 2, true, 1.2f, 1.2f, {14.0f, 0.0f, 10.0f}}, // growing but at its size: grown
	    {E(30), 5, false, 1.0f, 1.0f, {38.0f, 0.0f, 0.0f}},
	    {E(2), 2, false, 1.0f, 1.0f, {10.0f, 0.0f, 12.0f}},
	    {E(9), 11, false, 1.0f, 1.0f, {1.0f, 0.0f, 1.0f}},
	    {E(5), 9, false, 1.0f, 1.0f, {-5.0f, 0.0f, 20.0f}}, // same distance as 8
	    {E(6), 2, false, 1.0f, 1.0f, {6.0f, 0.0f, 10.0f}},
	};
	ExpectSameAsTheOldWalk(forests, trees);

	const auto actual = NewPass(forests, trees);
	EXPECT_EQ(actual.at(2).count, 4U);
	ASSERT_EQ(actual.at(2).unsorted.size(), 3U);
	EXPECT_EQ(actual.at(2).unsorted[0].second, E(1));
	EXPECT_EQ(actual.at(2).unsorted[1].second, E(2));
	EXPECT_EQ(actual.at(2).unsorted[2].second, E(6));
	EXPECT_EQ(actual.at(7).count, 0U);
	EXPECT_TRUE(actual.at(7).unsorted.empty());
	ASSERT_EQ(actual.at(9).sorted.size(), 2U);
	EXPECT_EQ(actual.at(9).sorted[0].first, actual.at(9).sorted[1].first);
}

TEST(ForestTreeBuckets, AnIdNotGivenToResetIsEmpty)
{
	ForestTreeBuckets buckets;
	buckets.Reset(std::vector<uint32_t> {1, 4});
	buckets.Add(4, E(1), true, {0.0f, 0.0f});
	buckets.Add(3, E(2), true, {0.0f, 0.0f});
	EXPECT_EQ(buckets.Of(4).count, 1U);
	EXPECT_EQ(buckets.Of(3).count, 0U);
	EXPECT_TRUE(buckets.Of(3).grown.empty());
	buckets.Reset(std::vector<uint32_t> {4});
	EXPECT_EQ(buckets.Of(4).count, 0U);
}

TEST(ForestTreeBuckets, ManyForestsAndTiesMatchTheOldWalk)
{
	// Many forests, trees in a shuffled order, positions on a coarse grid so that many distances tie (the sort is not
	// stable, so ties only come out the same when every forest gets its trees in the same order)
	std::mt19937 random(20261008U);
	Forests forests;
	for (uint32_t id = 1; id <= 40; ++id)
	{
		if (id % 7 == 0)
		{
			continue;
		}
		forests[id] = {static_cast<float>(random() % 50), 0.0f, static_cast<float>(random() % 50)};
	}
	std::vector<FakeTree> trees;
	for (uint32_t i = 0; i < 3000; ++i)
	{
		trees.push_back({E(i),
		                 static_cast<uint32_t>(random() % 45),
		                 random() % 3 == 0,
		                 static_cast<float>(random() % 4) * 0.5f,
		                 1.0f,
		                 {static_cast<float>(random() % 60), 0.0f, static_cast<float>(random() % 60)}});
	}
	std::ranges::shuffle(trees, random);
	ExpectSameAsTheOldWalk(forests, trees);
}
