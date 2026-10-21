/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BufferedLogSink.h"

#include <exception>
#include <utility>

#include <spdlog/details/file_helper.h>
#include <spdlog/pattern_formatter.h>

namespace openblack::logging
{

BufferedLogSink::BufferedLogSink(Writer writer, std::optional<std::chrono::milliseconds> period)
    : _writer(std::move(writer))
    , _formatter(std::make_unique<spdlog::pattern_formatter>())
{
	if (period.has_value())
	{
		_thread = std::jthread([this, every = *period](const std::stop_token& stop) { WriteEachPeriod(stop, every); });
	}
}

std::shared_ptr<BufferedLogSink> BufferedLogSink::ToFile(const std::filesystem::path& path, std::chrono::milliseconds period)
{
	auto file = std::make_shared<spdlog::details::file_helper>();
	file->open(path.string(), false);
	return std::make_shared<BufferedLogSink>(
	    [file](const spdlog::memory_buf_t& bytes) {
		    file->write(bytes);
		    file->flush();
	    },
	    period);
}

BufferedLogSink::~BufferedLogSink()
{
	if (_thread.joinable())
	{
		_thread.request_stop();
		_wake.notify_all();
		_thread.join();
	}
	WriteQuietly();
}

void BufferedLogSink::log(const spdlog::details::log_msg& msg)
{
	const std::lock_guard lock(_linesMutex);
	_formatter->format(msg, _lines);
}

void BufferedLogSink::flush()
{
	const std::lock_guard write(_writeMutex);
	{
		const std::lock_guard lines(_linesMutex);
		std::swap(_lines, _writing);
	}
	if (_writing.size() == 0)
	{
		return;
	}
	// The lines that were taken go also when the write fails, so that the next write does not repeat them
	try
	{
		_writer(_writing);
	}
	catch (...)
	{
		_writing.clear();
		throw;
	}
	_writing.clear();
}

void BufferedLogSink::set_pattern(const std::string& pattern)
{
	set_formatter(std::make_unique<spdlog::pattern_formatter>(pattern));
}

void BufferedLogSink::set_formatter(std::unique_ptr<spdlog::formatter> sinkFormatter)
{
	const std::lock_guard lock(_linesMutex);
	_formatter = std::move(sinkFormatter);
}

void BufferedLogSink::WriteEachPeriod(const std::stop_token& stop, std::chrono::milliseconds period)
{
	while (!stop.stop_requested())
	{
		{
			std::unique_lock lock(_wakeMutex);
			_wake.wait_for(lock, stop, period, [] { return false; });
		}
		WriteQuietly();
	}
}

void BufferedLogSink::WriteQuietly() noexcept
{
	// A write that fails, as on a full disk, drops those lines and the game goes on
	try
	{
		flush();
	}
	catch (const std::exception&)
	{
		return;
	}
}

} // namespace openblack::logging
