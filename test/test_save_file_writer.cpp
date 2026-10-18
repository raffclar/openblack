/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The writer of saved games writes its files on a thread of its own, in the order that they came, and the game waits for
// it only before it reads a saved game back.

#include <atomic>
#include <chrono>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/SaveFileWriter.h"

using openblack::ecs::save_files::BackgroundWriter;

TEST(SaveFileWriter, JobsRunInTheOrderThatTheyCameAndFlushWaitsForThem)
{
	std::mutex mutex;
	std::vector<int> order;
	BackgroundWriter writer;
	for (int i = 0; i < 20; ++i)
	{
		writer.Queue("file " + std::to_string(i), [&mutex, &order, i]() {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			const std::lock_guard lock(mutex);
			order.push_back(i);
			return true;
		});
	}
	writer.Flush();
	EXPECT_EQ(writer.Pending(), 0u);
	ASSERT_EQ(order.size(), 20u);
	for (int i = 0; i < 20; ++i)
	{
		EXPECT_EQ(order[static_cast<size_t>(i)], i);
	}
	EXPECT_EQ(writer.Failures(), 0u);
}

TEST(SaveFileWriter, TheGameThreadDoesNotWaitForAJob)
{
	std::atomic<bool> release {false};
	std::atomic<bool> done {false};
	BackgroundWriter writer;
	const auto before = std::chrono::steady_clock::now();
	writer.Queue("slow", [&release, &done]() {
		while (!release)
		{
			std::this_thread::yield();
		}
		done = true;
		return true;
	});
	// Queue comes back while the job still waits
	EXPECT_LT(std::chrono::steady_clock::now() - before, std::chrono::seconds(1));
	EXPECT_EQ(writer.Pending(), 1u);
	EXPECT_FALSE(done);
	release = true;
	writer.Flush();
	EXPECT_TRUE(done);
}

TEST(SaveFileWriter, AJobThatFailsOrThrowsIsReported)
{
	std::vector<std::string> failed;
	{
		BackgroundWriter writer([&failed](const std::string& name) { failed.push_back(name); });
		writer.Queue("fails", []() { return false; });
		writer.Queue("throws", []() -> bool { throw std::runtime_error("no disk"); });
		writer.Queue("writes", []() { return true; });
		writer.Flush();
		EXPECT_EQ(writer.Failures(), 2u);
	}
	EXPECT_EQ(failed, (std::vector<std::string> {"fails", "throws"}));
}

TEST(SaveFileWriter, TheWriterWritesEveryJobBeforeItGoes)
{
	std::atomic<int> written {0};
	{
		BackgroundWriter writer;
		for (int i = 0; i < 5; ++i)
		{
			writer.Queue("file", [&written]() {
				std::this_thread::sleep_for(std::chrono::milliseconds(2));
				++written;
				return true;
			});
		}
	}
	EXPECT_EQ(written, 5);
}
