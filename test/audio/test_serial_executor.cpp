/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <future>
#include <mutex>
#include <thread>
#include <vector>

#include <Audio/SerialExecutor.h>
#include <gtest/gtest.h>

using namespace openblack::audio;

TEST(SerialExecutor, JobsRunInTheOrderOfTheirSubmit)
{
	std::vector<int> ran;
	std::vector<std::shared_future<int>> futures;
	{
		SerialExecutor executor;
		for (int i = 0; i < 100; ++i)
		{
			futures.push_back(executor.Submit([&ran, i] {
				ran.push_back(i);
				return i;
			}));
		}
		for (int i = 0; i < 100; ++i)
		{
			EXPECT_EQ(futures[i].get(), i);
		}
	}
	ASSERT_EQ(ran.size(), 100u);
	for (int i = 0; i < 100; ++i)
	{
		EXPECT_EQ(ran[i], i);
	}
}

TEST(SerialExecutor, AllJobsRunOnOneThreadThatIsNotTheCaller)
{
	SerialExecutor executor;
	auto first = executor.Submit([] { return std::this_thread::get_id(); });
	auto second = executor.Submit([] { return std::this_thread::get_id(); });
	auto third = executor.Submit([] { return std::this_thread::get_id(); });
	EXPECT_EQ(first.get(), second.get());
	EXPECT_EQ(second.get(), third.get());
	EXPECT_NE(first.get(), std::this_thread::get_id());
}

TEST(SerialExecutor, TheDestructorRunsTheJobsThatAreLeft)
{
	std::vector<std::shared_future<int>> futures;
	std::promise<void> release;
	{
		SerialExecutor executor;
		auto gate = release.get_future().share();
		futures.push_back(executor.Submit([gate] {
			gate.wait();
			return 0;
		}));
		for (int i = 1; i < 20; ++i)
		{
			futures.push_back(executor.Submit([i] { return i; }));
		}
		release.set_value();
	}
	for (int i = 0; i < 20; ++i)
	{
		ASSERT_EQ(futures[i].wait_for(std::chrono::seconds(0)), std::future_status::ready);
		EXPECT_EQ(futures[i].get(), i);
	}
}

TEST(SerialExecutor, AJobCanWaitForAJobBeforeIt)
{
	SerialExecutor executor;
	auto first = executor.Submit([] { return 7; });
	auto second = executor.Submit([first] { return first.get() + 1; });
	EXPECT_EQ(second.get(), 8);
}

TEST(SerialExecutor, SubmitsFromManyThreadsLoseNoJob)
{
	std::mutex mutex;
	int count = 0;
	{
		SerialExecutor executor;
		std::vector<std::thread> threads;
		std::vector<std::shared_future<void>> futures;
		for (int t = 0; t < 4; ++t)
		{
			threads.emplace_back([&] {
				for (int i = 0; i < 50; ++i)
				{
					auto f = executor.Submit([&] {
						const std::lock_guard lock(mutex);
						++count;
					});
					f.wait();
				}
			});
		}
		for (auto& thread : threads)
		{
			thread.join();
		}
	}
	EXPECT_EQ(count, 200);
}
