/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <array>
#include <atomic>
#include <memory>
#include <numeric>
#include <optional>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "Common/WorkerPool.h"
#include "Common/WorkerPools.h"

using openblack::WorkerPool;
using openblack::WorkerPools;
using openblack::WorkerThreads;

namespace
{

/// Each index's square, made by the pool into the index's own place, and the lanes that did the jobs
struct Squares
{
	std::vector<uint64_t> values;
	std::vector<size_t> lanes;
};

Squares MakeSquares(WorkerPool& pool, size_t count)
{
	Squares squares {.values = std::vector<uint64_t>(count, 0), .lanes = std::vector<size_t>(count, 0)};
	pool.ForEach(count, [&squares](size_t index, size_t lane) {
		squares.values[index] = static_cast<uint64_t>(index) * index;
		squares.lanes[index] = lane;
	});
	return squares;
}

} // namespace

TEST(WorkerPool, EveryJobRunsOnceIntoItsOwnPlace)
{
	WorkerPool pool(3);
	ASSERT_EQ(pool.GetLaneCount(), 4U);
	for (const size_t count : {0U, 1U, 2U, 7U, 500U})
	{
		const auto squares = MakeSquares(pool, count);
		for (size_t index = 0; index < count; ++index)
		{
			EXPECT_EQ(squares.values[index], static_cast<uint64_t>(index) * index);
			EXPECT_LT(squares.lanes[index], pool.GetLaneCount());
		}
	}
}

TEST(WorkerPool, ManyCallsInTurnAllFinish)
{
	WorkerPool pool(4);
	uint64_t total = 0;
	for (int call = 0; call < 2'000; ++call)
	{
		const auto squares = MakeSquares(pool, 37);
		total += std::accumulate(squares.values.begin(), squares.values.end(), uint64_t {0});
	}
	// The sum of the squares from 0 to 36, once for each call
	EXPECT_EQ(total, uint64_t {2'000} * 16'206);
}

TEST(WorkerPool, CallsOfFewJobsAfterOneAnotherRunEachJobOnce)
{
	// The calls are short, so many threads wake after the caller returned. A late thread must not do a job twice or
	// do a job of the next call.
	WorkerPool pool(6);
	for (size_t call = 0; call < 5'000; ++call)
	{
		const size_t count = 2 + call % 5;
		std::array<std::atomic<int>, 8> runs {};
		pool.ForEach(count, [&runs](size_t index, size_t /*lane*/) { runs[index].fetch_add(1); });
		for (size_t index = 0; index < runs.size(); ++index)
		{
			ASSERT_EQ(runs[index].load(), index < count ? 1 : 0) << "call " << call << " index " << index;
		}
	}
}

TEST(WorkerPool, WithNoThreadsTheCallerDoesEveryJob)
{
	WorkerPool pool(0);
	ASSERT_EQ(pool.GetLaneCount(), 1U);
	const auto squares = MakeSquares(pool, 10);
	EXPECT_TRUE(std::ranges::all_of(squares.lanes, [](size_t lane) { return lane == 0; }));
	EXPECT_EQ(squares.values[9], 81U);
}

TEST(WorkerPool, AStartedCallFinishesEveryJobOnce)
{
	for (const size_t threads : {size_t {0}, size_t {1}, size_t {4}})
	{
		WorkerPool pool(threads);
		for (int round = 0; round < 50; ++round)
		{
			const size_t count = static_cast<size_t>(round % 7);
			std::vector<std::atomic<int>> done(count);
			pool.Start(count, [&done](size_t index, size_t /*lane*/) { done[index].fetch_add(1); });
			// The caller does other work while the threads do the jobs, then a call of ForEach after Finish
			pool.Finish();
			for (const auto& runs : done)
			{
				EXPECT_EQ(runs.load(), 1);
			}
			std::vector<int> after(5, 0);
			pool.ForEach(after.size(), [&after](size_t index, size_t /*lane*/) { after[index] = 1; });
			EXPECT_EQ(std::accumulate(after.begin(), after.end(), 0), 5);
		}
		// A Finish with no Start does nothing
		pool.Finish();
	}
}

TEST(WorkerPool, PoolsThatShareThreadsUseTheSameLanes)
{
	const auto threads = std::make_shared<WorkerThreads>(3);
	WorkerPool first(threads);
	WorkerPool second(threads);
	EXPECT_EQ(first.GetLaneCount(), 4U);
	EXPECT_EQ(second.GetLaneCount(), 4U);
	const auto squares = MakeSquares(second, 200);
	EXPECT_EQ(squares.values[199], uint64_t {199} * 199);
	EXPECT_TRUE(std::ranges::all_of(squares.lanes, [](size_t lane) { return lane < 4; }));
}

TEST(WorkerPool, ThreadsThatShareAPoolCallItAtTheSameTime)
{
	// Each caller has a pool of its own on the same threads, as the systems have in the browser
	const auto threads = std::make_shared<WorkerThreads>(4);
	constexpr size_t k_Callers = 4;
	constexpr int k_Calls = 500;
	std::array<uint64_t, k_Callers> totals {};
	{
		std::vector<std::jthread> callers;
		for (size_t caller = 0; caller < k_Callers; ++caller)
		{
			callers.emplace_back([&threads, &totals, caller] {
				WorkerPool pool(threads);
				for (int call = 0; call < k_Calls; ++call)
				{
					const auto squares = MakeSquares(pool, 37);
					totals[caller] += std::accumulate(squares.values.begin(), squares.values.end(), uint64_t {0});
				}
			});
		}
	}
	for (const auto total : totals)
	{
		EXPECT_EQ(total, uint64_t {k_Calls} * 16'206);
	}
}

TEST(WorkerPool, AStartedCallAndACallOfAnotherPoolShareTheThreads)
{
	const auto threads = std::make_shared<WorkerThreads>(3);
	WorkerPool background(threads);
	WorkerPool frame(threads);
	for (int round = 0; round < 200; ++round)
	{
		std::vector<std::atomic<int>> started(50);
		background.Start(started.size(), [&started](size_t index, size_t /*lane*/) { started[index].fetch_add(1); });
		const auto squares = MakeSquares(frame, 20);
		EXPECT_EQ(squares.values[19], 361U);
		background.Finish();
		for (const auto& runs : started)
		{
			ASSERT_EQ(runs.load(), 1) << "round " << round;
		}
	}
}

TEST(WorkerPool, AJobCanCallAnotherPoolOfTheSameThreads)
{
	const auto threads = std::make_shared<WorkerThreads>(3);
	WorkerPool outer(threads);
	WorkerPool inner(threads);
	std::vector<uint64_t> sums(8, 0);
	outer.ForEach(sums.size(), [&inner, &sums](size_t index, size_t /*lane*/) {
		std::vector<uint64_t> values(10, 0);
		inner.ForEach(values.size(), [&values, index](size_t at, size_t /*lane*/) { values[at] = index * at; });
		sums[index] = std::accumulate(values.begin(), values.end(), uint64_t {0});
	});
	for (size_t index = 0; index < sums.size(); ++index)
	{
		EXPECT_EQ(sums[index], index * 45);
	}
}

TEST(WorkerPools, WithACountAllPoolsShareTheThreads)
{
	const WorkerPools pools(2);
	ASSERT_NE(pools.GetShared(), nullptr);
	const auto first = pools.Make();
	const auto second = pools.Make();
	EXPECT_EQ(first->GetLaneCount(), 3U);
	EXPECT_EQ(second->GetLaneCount(), 3U);
	// The service and the two pools own the threads
	EXPECT_EQ(pools.GetShared().use_count(), 3);
	EXPECT_EQ(MakeSquares(*first, 9).values[8], 64U);
}

TEST(WorkerPools, WithNoCountEachPoolHasThreadsOfItsOwn)
{
	const WorkerPools pools(std::nullopt);
	EXPECT_EQ(pools.GetShared(), nullptr);
	const auto pool = pools.Make();
	EXPECT_EQ(pool->GetLaneCount(), WorkerPool::DefaultThreadCount() + 1);
}
