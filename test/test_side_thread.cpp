/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "Common/SideThread.h"

using openblack::SideThread;

TEST(SideThread, HasNoWorkAtFirst)
{
	const SideThread thread;
	EXPECT_TRUE(thread.Done());
	EXPECT_NE(thread.GetId(), std::this_thread::get_id());
}

TEST(SideThread, DoesTheWorkOnItsOwnThreadAndCallsTheEnd)
{
	SideThread thread;
	std::thread::id worker;
	std::atomic<bool> ended {false};
	std::atomic<bool> doneAtEnd {false};
	thread.Start([&worker] { worker = std::this_thread::get_id(); },
	             [&] {
		             doneAtEnd = thread.Done();
		             ended = true;
	             });
	thread.Wait();
	EXPECT_TRUE(thread.Done());
	EXPECT_EQ(worker, thread.GetId());
	// The end comes after the work is done, on the side thread, so it may come after Wait
	while (!ended)
	{
		std::this_thread::yield();
	}
	EXPECT_TRUE(doneAtEnd);
}

TEST(SideThread, DoesEachPieceOfWorkOnTheSameThread)
{
	SideThread thread;
	std::vector<std::thread::id> workers;
	for (int i = 0; i < 3; ++i)
	{
		thread.Start([&workers] { workers.push_back(std::this_thread::get_id()); });
		thread.Wait();
	}
	ASSERT_EQ(workers.size(), 3U);
	EXPECT_EQ(workers[0], workers[1]);
	EXPECT_EQ(workers[1], workers[2]);
}

TEST(SideThread, GivesTheExceptionOfTheWorkToWait)
{
	SideThread thread;
	thread.Start([] { throw std::runtime_error("the work failed"); });
	EXPECT_THROW(thread.Wait(), std::runtime_error);
	// The thread does the next work after an exception
	bool done = false;
	thread.Start([&done] { done = true; });
	EXPECT_NO_THROW(thread.Wait());
	EXPECT_TRUE(done);
}

TEST(SideThread, WaitsForItsWorkBeforeItStops)
{
	std::atomic<bool> done {false};
	{
		SideThread thread;
		thread.Start([&done] {
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
			done = true;
		});
	}
	EXPECT_TRUE(done);
}

TEST(SideThread, WaitOrRunDoesEachPieceOfWorkOnceAndCallsEachEndOnce)
{
	// The work runs on the side thread or on the caller, as the system wakes the thread, but always once
	SideThread thread;
	constexpr int k_Count = 2000;
	std::atomic<int> works {0};
	std::atomic<int> ends {0};
	for (int i = 0; i < k_Count; ++i)
	{
		thread.Start([&works] { ++works; }, [&ends] { ++ends; });
		thread.WaitOrRun();
		EXPECT_TRUE(thread.Done());
		EXPECT_EQ(works, i + 1);
	}
	// The side thread can call the last end after WaitOrRun returns
	while (ends < k_Count)
	{
		std::this_thread::yield();
	}
	EXPECT_EQ(works, k_Count);
	EXPECT_EQ(ends, k_Count);
}

TEST(SideThread, WaitOrRunDoesTheWorkOnOneOfTheTwoThreads)
{
	SideThread thread;
	std::thread::id worker;
	thread.Start([&worker] { worker = std::this_thread::get_id(); });
	thread.WaitOrRun();
	EXPECT_TRUE(worker == thread.GetId() || worker == std::this_thread::get_id());
}

TEST(SideThread, WaitOrRunWaitsForWorkThatTheThreadStarted)
{
	SideThread thread;
	std::atomic<bool> started {false};
	std::atomic<bool> done {false};
	thread.Start([&] {
		started = true;
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
		done = true;
	});
	while (!started)
	{
		std::this_thread::yield();
	}
	thread.WaitOrRun();
	EXPECT_TRUE(done);
}

TEST(SideThread, WaitOrRunGivesTheExceptionOfTheWork)
{
	SideThread thread;
	thread.Start([] { throw std::runtime_error("the work failed"); });
	EXPECT_THROW(thread.WaitOrRun(), std::runtime_error);
	bool done = false;
	thread.Start([&done] { done = true; });
	EXPECT_NO_THROW(thread.WaitOrRun());
	EXPECT_TRUE(done);
}
