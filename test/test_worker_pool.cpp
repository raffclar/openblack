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
#include <numeric>
#include <vector>

#include <gtest/gtest.h>

#include "Common/WorkerPool.h"

using openblack::WorkerPool;

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
