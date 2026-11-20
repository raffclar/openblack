/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>

#include <atomic>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/PoseThreads.h"
#include "Resources/KeptHandles.h"
#include "Resources/ResourceManager.h"

using openblack::resources::KeptHandles;
using openblack::resources::ResourceManager;

namespace
{

struct FakeResource
{
	int value;
};

struct FakeLoader
{
	using ResourceType = FakeResource;
	using result_type = std::shared_ptr<FakeResource>;

	[[nodiscard]] result_type operator()(int value) const { return std::make_shared<FakeResource>(value); }
};

using FakeCache = ResourceManager<FakeLoader>;

constexpr entt::id_type k_First = 1;
constexpr entt::id_type k_Second = 2;
constexpr entt::id_type k_Missing = 9;

/// A cache that counts the questions it gets
struct CountingCache
{
	FakeResource first {.value = 5};
	mutable int contains {0};
	mutable int finds {0};
	mutable int loadedFinds {0};

	[[nodiscard]] bool Contains(entt::id_type id) const
	{
		++contains;
		return id == k_First;
	}
	[[nodiscard]] const FakeResource* Find(entt::id_type id) const
	{
		++finds;
		return id == k_First ? &first : nullptr;
	}
	[[nodiscard]] const FakeResource* FindLoaded(entt::id_type id) const
	{
		++loadedFinds;
		return id == k_First ? &first : nullptr;
	}
};

/// A registered load that counts how often it runs
openblack::resources::DeferredLoad<FakeResource> Counted(int value, std::atomic<int>& runs)
{
	return [value, &runs] {
		++runs;
		return std::make_shared<FakeResource>(value);
	};
}

} // namespace

TEST(KeptHandles, EachIdAsksTheCacheOneTime)
{
	const CountingCache cache;
	KeptHandles<FakeResource> kept;
	for (int i = 0; i < 4; ++i)
	{
		EXPECT_TRUE(kept.Contains(cache, k_First));
		EXPECT_EQ(kept.Find(cache, k_First), &cache.first);
		EXPECT_FALSE(kept.Contains(cache, k_Missing));
		EXPECT_EQ(kept.Find(cache, k_Missing), nullptr);
	}
	EXPECT_EQ(cache.contains, 2);
	EXPECT_EQ(cache.finds, 2);
}

TEST(KeptHandles, ClearAsksTheCacheAgain)
{
	const CountingCache cache;
	KeptHandles<FakeResource> kept;
	EXPECT_TRUE(kept.Contains(cache, k_First));
	EXPECT_NE(kept.Find(cache, k_First), nullptr);
	kept.Clear();
	EXPECT_TRUE(kept.Contains(cache, k_First));
	EXPECT_NE(kept.Find(cache, k_First), nullptr);
	EXPECT_EQ(cache.contains, 2);
	EXPECT_EQ(cache.finds, 2);
}

TEST(KeptHandles, AFoundResourceIsAlsoLoaded)
{
	const CountingCache cache;
	KeptHandles<FakeResource> kept;
	EXPECT_NE(kept.Find(cache, k_First), nullptr);
	EXPECT_EQ(kept.FindLoaded(cache, k_First), &cache.first);
	EXPECT_EQ(cache.loadedFinds, 0);
}

TEST(KeptHandles, OnlyALoadedResourceIsKeptByFindLoaded)
{
	const CountingCache cache;
	KeptHandles<FakeResource> kept;
	EXPECT_EQ(kept.FindLoaded(cache, k_Missing), nullptr);
	EXPECT_EQ(kept.FindLoaded(cache, k_Missing), nullptr);
	EXPECT_EQ(kept.FindLoaded(cache, k_First), &cache.first);
	EXPECT_EQ(kept.FindLoaded(cache, k_First), &cache.first);
	EXPECT_EQ(cache.loadedFinds, 3);
}

TEST(KeptHandles, FindLoadedDoesNotLoadARegisteredResource)
{
	FakeCache cache;
	std::atomic<int> runs = 0;
	cache.Register(k_First, Counted(7, runs));
	cache.Load(k_Second, 3);
	const auto& view = cache;
	KeptHandles<FakeResource> kept;

	EXPECT_TRUE(kept.Contains(view, k_First));
	EXPECT_EQ(kept.FindLoaded(view, k_First), nullptr);
	EXPECT_EQ(runs, 0);
	ASSERT_NE(kept.FindLoaded(view, k_Second), nullptr);
	EXPECT_EQ(kept.FindLoaded(view, k_Second)->value, 3);

	// The walk loads it later, and then it is there
	ASSERT_NE(kept.Find(view, k_First), nullptr);
	EXPECT_EQ(runs, 1);
	ASSERT_NE(kept.FindLoaded(view, k_First), nullptr);
	EXPECT_EQ(kept.FindLoaded(view, k_First)->value, 7);
}

TEST(KeptHandles, TheCacheFindsOnlyLoadedResources)
{
	FakeCache cache;
	std::atomic<int> runs = 0;
	cache.Register(k_First, Counted(7, runs));
	const auto& view = cache;

	EXPECT_EQ(view.FindLoaded(k_First), nullptr);
	EXPECT_EQ(view.FindLoaded(k_Missing), nullptr);
	EXPECT_EQ(runs, 0);
	ASSERT_NE(view.Find(k_First), nullptr);
	ASSERT_NE(view.FindLoaded(k_First), nullptr);
	EXPECT_EQ(view.FindLoaded(k_First)->value, 7);
}

TEST(PoseThreads, EachJobRunsOneTimeAndThePoolIsKept)
{
	std::unique_ptr<openblack::WorkerPool> pool;
	std::vector<std::atomic<int>> runs(500);
	openblack::ecs::pose_threads::ForEach(pool, runs.size(), 0, [&](size_t index, size_t /*lane*/) { ++runs[index]; });
	ASSERT_NE(pool, nullptr);
	const auto* first = pool.get();
	openblack::ecs::pose_threads::ForEach(pool, runs.size(), 0, [&](size_t index, size_t /*lane*/) { ++runs[index]; });
	EXPECT_EQ(pool.get(), first);
	for (const auto& run : runs)
	{
		EXPECT_EQ(run, 2);
	}
}

TEST(PoseThreads, EachIndexRunsOneTimeForAnyCount)
{
	std::unique_ptr<openblack::WorkerPool> pool;
	for (const size_t count : {1, 2, 7, 27, 28, 29, 1001})
	{
		std::vector<std::atomic<int>> runs(count);
		openblack::ecs::pose_threads::ForEach(pool, count, 0, [&](size_t index, size_t /*lane*/) { ++runs.at(index); });
		for (const auto& run : runs)
		{
			EXPECT_EQ(run, 1) << "count " << count;
		}
	}
}

TEST(PoseThreads, NoJobsIsFine)
{
	std::unique_ptr<openblack::WorkerPool> pool;
	openblack::ecs::pose_threads::ForEach(pool, 0, 0, [](size_t /*index*/, size_t /*lane*/) { FAIL(); });
}

TEST(PoseThreads, AFewJobsRunInOrderOnTheCallerWithNoPool)
{
	std::unique_ptr<openblack::WorkerPool> pool;
	std::vector<size_t> order;
	openblack::ecs::pose_threads::ForEach(pool, 5, 5, [&](size_t index, size_t lane) {
		EXPECT_EQ(lane, 0);
		order.push_back(index);
	});
	EXPECT_EQ(pool, nullptr);
	EXPECT_EQ(order, (std::vector<size_t> {0, 1, 2, 3, 4}));
}

TEST(PoseThreads, MoreJobsThanTheCallerLimitUseThePool)
{
	std::unique_ptr<openblack::WorkerPool> pool;
	std::vector<std::atomic<int>> runs(6);
	openblack::ecs::pose_threads::ForEach(pool, runs.size(), 5, [&](size_t index, size_t /*lane*/) { ++runs.at(index); });
	EXPECT_NE(pool, nullptr);
	for (const auto& run : runs)
	{
		EXPECT_EQ(run, 1);
	}
}
