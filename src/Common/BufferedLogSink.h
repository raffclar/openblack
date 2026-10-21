/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <condition_variable>

#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>

#include <spdlog/common.h>
#include <spdlog/details/log_msg.h>
#include <spdlog/formatter.h>
#include <spdlog/sinks/sink.h>

namespace openblack::logging
{

/// A log sink that keeps the lines in memory and writes them to the disk on its own thread.
///
/// A thread that logs a line only formats it and adds it to the memory. It never waits for the disk, also when the sink
/// writes at that time. The lines go to the disk in the order that they came. A flush writes all the lines in memory
/// before it returns, thus the crash handler and the lines of errors write all that came before them.
class BufferedLogSink final: public spdlog::sinks::sink
{
public:
	/// Writes the bytes at the end of the log and puts them on the disk
	using Writer = std::function<void(const spdlog::memory_buf_t& bytes)>;

	/// A sink that writes through the writer. With a period, its thread writes the lines in memory each period.
	BufferedLogSink(Writer writer, std::optional<std::chrono::milliseconds> period);
	/// A sink that adds to the file at the path, as the file sink of spdlog does
	[[nodiscard]] static std::shared_ptr<BufferedLogSink> ToFile(const std::filesystem::path& path,
	                                                             std::chrono::milliseconds period);
	/// Stops the thread and writes the lines that stay in memory
	~BufferedLogSink() override;

	BufferedLogSink(const BufferedLogSink&) = delete;
	BufferedLogSink& operator=(const BufferedLogSink&) = delete;
	BufferedLogSink(BufferedLogSink&&) = delete;
	BufferedLogSink& operator=(BufferedLogSink&&) = delete;

	void log(const spdlog::details::log_msg& msg) override;
	/// Writes every line in memory before it returns
	void flush() override;
	void set_pattern(const std::string& pattern) override;
	void set_formatter(std::unique_ptr<spdlog::formatter> sinkFormatter) override;

private:
	void WriteEachPeriod(const std::stop_token& stop, std::chrono::milliseconds period);
	void WriteQuietly() noexcept;

	Writer _writer;
	/// Held only to add a line or to take the lines: never while the disk writes
	std::mutex _linesMutex;
	std::unique_ptr<spdlog::formatter> _formatter;
	spdlog::memory_buf_t _lines;
	/// Held while the lines that were taken go to the disk, so that two writes keep their order
	std::mutex _writeMutex;
	spdlog::memory_buf_t _writing;
	std::mutex _wakeMutex;
	std::condition_variable_any _wake;
	std::jthread _thread;
};

} // namespace openblack::logging
