/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <chrono>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>

#include <gtest/gtest.h>
#include <spdlog/logger.h>
#include <spdlog/pattern_formatter.h>

#include "Common/BufferedLogSink.h"

using openblack::logging::BufferedLogSink;

namespace
{

/// A fake disk: it keeps what the sink writes, and it can hold a write until the test lets it go on
struct FakeDisk
{
	std::mutex mutex;
	std::string written;
	int writes {0};
	std::optional<std::promise<void>> entered;
	std::optional<std::shared_future<void>> release;

	BufferedLogSink::Writer Writer()
	{
		return [this](const spdlog::memory_buf_t& bytes) {
			if (entered.has_value())
			{
				entered->set_value();
				entered.reset();
				release->wait();
			}
			const std::lock_guard lock(mutex);
			written.append(bytes.data(), bytes.size());
			++writes;
		};
	}

	std::string Written()
	{
		const std::lock_guard lock(mutex);
		return written;
	}
};

std::shared_ptr<spdlog::logger> LoggerOn(const std::shared_ptr<BufferedLogSink>& sink)
{
	auto logger = std::make_shared<spdlog::logger>("test", sink);
	// Each line ends with a plain new line on every platform
	sink->set_formatter(std::make_unique<spdlog::pattern_formatter>("%v", spdlog::pattern_time_type::local, "\n"));
	return logger;
}

} // namespace

TEST(BufferedLogSink, KeepsTheLinesUntilAFlush)
{
	FakeDisk disk;
	auto sink = std::make_shared<BufferedLogSink>(disk.Writer(), std::nullopt);
	auto logger = LoggerOn(sink);
	logger->info("one");
	logger->info("two");
	EXPECT_EQ(disk.Written(), "");
	sink->flush();
	EXPECT_EQ(disk.Written(), "one\ntwo\n");
	EXPECT_EQ(disk.writes, 1);
}

TEST(BufferedLogSink, AFlushWithNoLinesWritesNothing)
{
	FakeDisk disk;
	auto sink = std::make_shared<BufferedLogSink>(disk.Writer(), std::nullopt);
	sink->flush();
	EXPECT_EQ(disk.writes, 0);
}

TEST(BufferedLogSink, KeepsTheOrderOfTheLinesOverManyFlushes)
{
	FakeDisk disk;
	auto sink = std::make_shared<BufferedLogSink>(disk.Writer(), std::nullopt);
	auto logger = LoggerOn(sink);
	std::string expected;
	for (int i = 0; i < 20; ++i)
	{
		logger->info("{}", i);
		expected += std::to_string(i) + "\n";
		if (i % 3 == 0)
		{
			sink->flush();
		}
	}
	sink->flush();
	EXPECT_EQ(disk.Written(), expected);
}

TEST(BufferedLogSink, ALineDoesNotWaitForTheDisk)
{
	FakeDisk disk;
	std::promise<void> release;
	disk.release = release.get_future().share();
	disk.entered.emplace();
	auto entered = disk.entered->get_future();
	auto sink = std::make_shared<BufferedLogSink>(disk.Writer(), std::nullopt);
	auto logger = LoggerOn(sink);
	logger->info("before");
	auto flush = std::async(std::launch::async, [&sink] { sink->flush(); });
	ASSERT_EQ(entered.wait_for(std::chrono::seconds(10)), std::future_status::ready);
	// The disk holds the write of "before". A new line goes to the memory and the logger does not wait.
	auto line = std::async(std::launch::async, [&logger] { logger->info("during"); });
	const auto status = line.wait_for(std::chrono::seconds(10));
	release.set_value();
	flush.wait();
	ASSERT_EQ(status, std::future_status::ready);
	line.wait();
	EXPECT_EQ(disk.Written(), "before\n");
	sink->flush();
	EXPECT_EQ(disk.Written(), "before\nduring\n");
}

TEST(BufferedLogSink, WritesTheLinesEachPeriod)
{
	FakeDisk disk;
	auto sink = std::make_shared<BufferedLogSink>(disk.Writer(), std::chrono::milliseconds(5));
	auto logger = LoggerOn(sink);
	logger->info("soon");
	const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(10);
	while (disk.Written().empty() && std::chrono::steady_clock::now() < until)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	EXPECT_EQ(disk.Written(), "soon\n");
}

TEST(BufferedLogSink, WritesTheLastLinesWhenItGoes)
{
	FakeDisk disk;
	{
		auto sink = std::make_shared<BufferedLogSink>(disk.Writer(), std::chrono::hours(1));
		auto logger = LoggerOn(sink);
		logger->info("last");
	}
	EXPECT_EQ(disk.Written(), "last\n");
}

TEST(BufferedLogSink, AFailedWriteDropsOnlyItsLines)
{
	std::string written;
	bool fail = true;
	auto sink = std::make_shared<BufferedLogSink>(
	    [&written, &fail](const spdlog::memory_buf_t& bytes) {
		    if (fail)
		    {
			    throw std::runtime_error("full");
		    }
		    written.append(bytes.data(), bytes.size());
	    },
	    std::nullopt);
	auto logger = LoggerOn(sink);
	logger->info("lost");
	EXPECT_THROW(sink->flush(), std::runtime_error);
	fail = false;
	logger->info("kept");
	sink->flush();
	EXPECT_EQ(written, "kept\n");
}
