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
#include <stdexcept>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "Graphics/UploadPacer.h"
#include "Resources/LoadQueue.h"
#include "Resources/ResourceManager.h"

using openblack::graphics::UploadPacer;
using openblack::resources::LoadQueue;
using openblack::resources::ResourceManager;

namespace
{

struct FakeResource
{
	int value;
};

/// Makes a resource of the value it is given
struct FakeLoader
{
	using ResourceType = FakeResource;
	using result_type = std::shared_ptr<FakeResource>;

	[[nodiscard]] result_type operator()(int value) const { return std::make_shared<FakeResource>(value); }
};

using FakeCache = ResourceManager<FakeLoader>;

constexpr entt::id_type k_First = 1;
constexpr entt::id_type k_Second = 2;
constexpr entt::id_type k_Third = 3;

/// A registered load that counts how often it runs
openblack::resources::DeferredLoad<FakeResource> Counted(int value, std::atomic<int>& runs)
{
	return [value, &runs] {
		++runs;
		return std::make_shared<FakeResource>(value);
	};
}

} // namespace

TEST(ResourceCache, ARegisteredResourceIsThereBeforeItIsLoaded)
{
	FakeCache cache;
	std::atomic<int> runs = 0;
	ASSERT_TRUE(cache.Register(k_First, Counted(7, runs), 10));

	EXPECT_TRUE(cache.Contains(k_First));
	EXPECT_FALSE(cache.IsLoaded(k_First));
	EXPECT_EQ(cache.PendingCount(), 1);
	EXPECT_EQ(cache.Size(), 1);
	EXPECT_EQ(runs, 0);
	EXPECT_FALSE(cache.Contains(k_Second));
}

TEST(ResourceCache, ItIsLoadedOnceWhenFirstAskedFor)
{
	FakeCache cache;
	std::atomic<int> runs = 0;
	cache.Register(k_First, Counted(7, runs));

	ASSERT_TRUE(cache.Handle(k_First));
	EXPECT_EQ(cache.Handle(k_First)->value, 7);
	ASSERT_NE(cache.Find(k_First), nullptr);
	EXPECT_EQ(cache.Find(k_First)->value, 7);
	EXPECT_EQ(runs, 1);
	EXPECT_TRUE(cache.IsLoaded(k_First));
	EXPECT_EQ(cache.PendingCount(), 0);
}

TEST(ResourceCache, ConstLookupsLoadToo)
{
	FakeCache cache;
	std::atomic<int> runs = 0;
	cache.Register(k_First, Counted(3, runs));
	const auto& view = cache;

	ASSERT_NE(view.Find(k_First), nullptr);
	EXPECT_EQ(view.Handle(k_First)->value, 3);
	EXPECT_EQ(runs, 1);
}

TEST(ResourceCache, TheFirstRegistrationOrLoadOfAnIdIsKept)
{
	FakeCache cache;
	std::atomic<int> runs = 0;
	ASSERT_TRUE(cache.Register(k_First, Counted(1, runs)));
	EXPECT_FALSE(cache.Register(k_First, Counted(2, runs)));
	// Loading an id that is registered loads it as registered
	cache.Load(k_First, 3);
	EXPECT_EQ(cache.Handle(k_First)->value, 1);

	cache.Load(k_Second, 4);
	EXPECT_FALSE(cache.Register(k_Second, Counted(5, runs)));
	EXPECT_EQ(cache.Handle(k_Second)->value, 4);
	EXPECT_EQ(runs, 1);
}

TEST(ResourceCache, RegisterLoadKeepsTheLoadersArguments)
{
	FakeCache cache;
	ASSERT_TRUE(cache.RegisterLoad(k_First, 4, 42));
	EXPECT_FALSE(cache.IsLoaded(k_First));
	EXPECT_EQ(cache.Handle(k_First)->value, 42);
}

TEST(ResourceCache, PrefetchedResourcesAreLoadedOnTheThreadsAndCollected)
{
	FakeCache cache;
	LoadQueue queue(2);
	std::atomic<int> runs = 0;
	cache.Register(k_First, Counted(1, runs), 10);
	cache.Register(k_Second, Counted(2, runs), 10);
	cache.Register(k_Third, Counted(3, runs), 10);

	cache.Prefetch(k_First);
	cache.Prefetch(k_Third);
	EXPECT_EQ(cache.LoadingCount(), 2);
	cache.StartLoads(queue, 1000);
	queue.WaitIdle();
	EXPECT_EQ(runs, 2);
	// Not in the cache until collected, on the main thread
	EXPECT_FALSE(cache.IsLoaded(k_First));
	EXPECT_EQ(cache.Collect(), 2);
	EXPECT_TRUE(cache.IsLoaded(k_First));
	EXPECT_TRUE(cache.IsLoaded(k_Third));
	EXPECT_FALSE(cache.IsLoaded(k_Second));
	EXPECT_EQ(cache.LoadingCount(), 0);
	EXPECT_EQ(cache.Handle(k_Third)->value, 3);
	EXPECT_EQ(runs, 2);
}

TEST(ResourceCache, LoadsAreHandedOverInOrderWithinTheBudget)
{
	FakeCache cache;
	// With no threads nothing runs, so what was handed over can be counted
	LoadQueue queue(0);
	std::atomic<int> runs = 0;
	cache.Register(k_First, Counted(1, runs), 10);
	cache.Register(k_Second, Counted(2, runs), 10);
	cache.Register(k_Third, Counted(3, runs), 10);
	cache.Prefetch(k_Second);
	cache.Prefetch(k_First);
	cache.Prefetch(k_Third);

	// The second and the first fit in 15 bytes once the second has gone; the third waits for the next frame
	EXPECT_EQ(cache.StartLoads(queue, 15), 20);
	EXPECT_EQ(cache.StartLoads(queue, 15), 10);
	EXPECT_EQ(cache.StartLoads(queue, 15), 0);
}

TEST(ResourceCache, OneResourceGoesEvenWhenItIsOverTheBudget)
{
	FakeCache cache;
	LoadQueue queue(0);
	std::atomic<int> runs = 0;
	cache.Register(k_First, Counted(1, runs), 100);
	cache.PrefetchAll();
	EXPECT_EQ(cache.StartLoads(queue, 10), 100);
}

TEST(ResourceCache, AResourceNoThreadHasStartedIsLoadedWhereItIsAskedFor)
{
	FakeCache cache;
	// Handed over, but no thread will ever run it
	LoadQueue queue(0);
	std::atomic<int> runs = 0;
	cache.Register(k_First, Counted(9, runs), 10);
	cache.Prefetch(k_First);
	cache.StartLoads(queue, 100);

	EXPECT_EQ(cache.Handle(k_First)->value, 9);
	EXPECT_EQ(runs, 1);
	EXPECT_EQ(cache.Collect(), 0);
	EXPECT_EQ(cache.LoadingCount(), 0);
}

TEST(ResourceCache, AskingForAResourceWaitingToUploadLetsItThrough)
{
	FakeCache cache;
	LoadQueue queue(1);
	// A frame with room for one upload, and a load that makes two: it waits for the next frame, which won't come
	queue.GetUploadPacer().BeginFrame(1);
	std::atomic<bool> firstMade = false;
	cache.Register(k_First, [&firstMade] {
		UploadPacer::Pace(1);
		firstMade = true;
		UploadPacer::Pace(1);
		return std::make_shared<FakeResource>(5);
	});
	cache.Prefetch(k_First);
	cache.StartLoads(queue, 100);
	while (!firstMade)
	{
		std::this_thread::yield();
	}

	EXPECT_EQ(cache.Handle(k_First)->value, 5);
}

TEST(ResourceCache, WhatIsStillLoadingIsCounted)
{
	FakeCache cache;
	LoadQueue queue(0);
	std::atomic<int> runs = 0;
	cache.Register(k_First, Counted(1, runs), 30);
	cache.Register(k_Second, Counted(2, runs), 40);
	cache.PrefetchAll();
	EXPECT_EQ(cache.StartLoads(queue, 1000), 70);
	EXPECT_EQ(cache.LoadingBytes(), 70);
	EXPECT_EQ(cache.Handle(k_First)->value, 1);
	cache.Collect();
	EXPECT_EQ(cache.LoadingBytes(), 40);
	cache.Clear();
	EXPECT_EQ(cache.LoadingBytes(), 0);
}

TEST(ResourceCache, EveryResourceIsLoadedOnceWhetherByAThreadOrWhereItIsAskedFor)
{
	FakeCache cache;
	LoadQueue queue(4);
	constexpr int k_Count = 200;
	std::vector<std::atomic<int>> runs(k_Count);
	for (int i = 0; i < k_Count; ++i)
	{
		cache.Register(static_cast<entt::id_type>(i + 1), Counted(i, runs[static_cast<size_t>(i)]), 1);
	}
	cache.PrefetchAll();
	cache.StartLoads(queue, k_Count);
	// Asked for at once, racing the threads
	for (int i = 0; i < k_Count; ++i)
	{
		ASSERT_EQ(cache.Handle(static_cast<entt::id_type>(i + 1))->value, i);
	}
	queue.WaitIdle();
	cache.Collect();
	for (const auto& count : runs)
	{
		EXPECT_EQ(count, 1);
	}
	EXPECT_EQ(cache.PendingCount(), 0);
	EXPECT_EQ(cache.LoadingCount(), 0);
}

TEST(ResourceCache, WhatIsAskedForIsTheSameHoweverFarLoadingHasGot)
{
	std::atomic<int> runs = 0;
	FakeCache onDemand;
	FakeCache prefetched;
	LoadQueue queue(2);
	for (entt::id_type id = 1; id <= 20; ++id)
	{
		onDemand.Register(id, Counted(static_cast<int>(id) * 3, runs));
		prefetched.Register(id, Counted(static_cast<int>(id) * 3, runs));
	}
	prefetched.PrefetchAll();
	prefetched.StartLoads(queue, 5);
	for (entt::id_type id = 1; id <= 20; ++id)
	{
		EXPECT_EQ(onDemand.Contains(id), prefetched.Contains(id));
		EXPECT_EQ(onDemand.Handle(id)->value, prefetched.Handle(id)->value);
	}
}

TEST(ResourceCache, AResourceThatFailsToLoadIsLeftOut)
{
	FakeCache cache;
	cache.Register(k_First, []() -> std::shared_ptr<FakeResource> { throw std::runtime_error("broken"); });
	EXPECT_TRUE(cache.Contains(k_First));
	EXPECT_FALSE(cache.Handle(k_First));
	EXPECT_EQ(cache.Find(k_First), nullptr);
	EXPECT_FALSE(cache.Contains(k_First));
}

TEST(ResourceCache, ErasingAndClearingDropRegistrations)
{
	FakeCache cache;
	LoadQueue queue(1);
	std::atomic<int> runs = 0;
	cache.Register(k_First, Counted(1, runs));
	cache.Register(k_Second, Counted(2, runs));
	cache.PrefetchAll();
	cache.StartLoads(queue, 100);
	cache.Erase(k_First);
	EXPECT_FALSE(cache.Contains(k_First));
	// Registered again while the old load may still run: the new one is what is loaded
	cache.Register(k_First, Counted(5, runs));
	queue.WaitIdle();
	cache.Collect();
	EXPECT_EQ(cache.Handle(k_First)->value, 5);

	cache.Clear();
	EXPECT_FALSE(cache.Contains(k_Second));
	EXPECT_EQ(cache.Size(), 0);
}

TEST(LoadQueue, RunsEveryJobAndCanDropThoseNotStarted)
{
	std::atomic<int> runs = 0;
	{
		LoadQueue queue(3);
		EXPECT_EQ(queue.GetThreadCount(), 3);
		for (int i = 0; i < 50; ++i)
		{
			queue.Submit([&runs] { ++runs; });
		}
		queue.WaitIdle();
		EXPECT_EQ(runs, 50);
		queue.Cancel();
	}
	// Without threads, jobs are dropped: what asked for them loads them itself
	LoadQueue none(0);
	none.Submit([&runs] { ++runs; });
	none.WaitIdle();
	EXPECT_EQ(runs, 50);
	EXPECT_GE(LoadQueue::DefaultThreadCount(), 1);
}
