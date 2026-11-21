/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS 1

#include <cstddef>

#include <atomic>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/Systems/Implementations/MindWorkSystem.h"

using openblack::ecs::systems::MindWorkSystem;

namespace
{

/// The sum of the squares of the indices, from a result for each index
size_t SumOfSquares(MindWorkSystem& work, size_t count, size_t onCaller)
{
	std::vector<size_t> results(count, 0);
	work.ForEach(count, onCaller, [&results](size_t index) { results[index] = index * index; });
	size_t sum = 0;
	for (const auto result : results)
	{
		sum += result;
	}
	return sum;
}

} // namespace

TEST(MindWorkSystem, EachIndexRunsOneTimeForEachThreadCount)
{
	for (const size_t threads : {0, 1, 2, 5})
	{
		MindWorkSystem work;
		work.SetThreads(threads);
		EXPECT_EQ(work.GetThreads(), threads);
		for (const size_t count : {0, 1, 2, 7, 33, 1001})
		{
			std::vector<std::atomic<int>> runs(count);
			work.ForEach(count, 0, [&runs](size_t index) { ++runs.at(index); });
			for (const auto& run : runs)
			{
				EXPECT_EQ(run, 1) << "threads " << threads << " count " << count;
			}
		}
	}
}

TEST(MindWorkSystem, TheResultsAreTheSameForEachThreadCount)
{
	MindWorkSystem alone;
	alone.SetThreads(0);
	const auto expected = SumOfSquares(alone, 5000, 0);
	for (const size_t threads : {1, 3, 6})
	{
		MindWorkSystem work;
		work.SetThreads(threads);
		EXPECT_EQ(SumOfSquares(work, 5000, 0), expected) << "threads " << threads;
	}
}

TEST(MindWorkSystem, FewJobsRunInOrderOnTheCallingThread)
{
	MindWorkSystem work;
	work.SetThreads(4);
	const auto caller = std::this_thread::get_id();
	std::vector<size_t> order;
	work.ForEach(10, 10, [&order, caller](size_t index) {
		EXPECT_EQ(std::this_thread::get_id(), caller);
		order.push_back(index);
	});
	ASSERT_EQ(order.size(), 10u);
	for (size_t index = 0; index < order.size(); ++index)
	{
		EXPECT_EQ(order[index], index);
	}
}

TEST(MindWorkSystem, NoThreadsRunsAllJobsInOrderOnTheCallingThread)
{
	MindWorkSystem work;
	work.SetThreads(0);
	const auto caller = std::this_thread::get_id();
	std::vector<size_t> order;
	work.ForEach(100, 0, [&order, caller](size_t index) {
		EXPECT_EQ(std::this_thread::get_id(), caller);
		order.push_back(index);
	});
	ASSERT_EQ(order.size(), 100u);
	for (size_t index = 0; index < order.size(); ++index)
	{
		EXPECT_EQ(order[index], index);
	}
}

TEST(MindWorkSystem, AJobCanGiveJobsAgain)
{
	// A job that gives jobs again does them on its own thread, and every index runs one time
	MindWorkSystem work;
	work.SetThreads(3);
	std::vector<std::atomic<int>> runs(64 * 8);
	work.ForEach(64, 0, [&work, &runs](size_t outer) {
		work.ForEach(8, 0, [&runs, outer](size_t inner) { ++runs.at(outer * 8 + inner); });
	});
	for (const auto& run : runs)
	{
		EXPECT_EQ(run, 1);
	}
}
