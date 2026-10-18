/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SaveFileWriter.h"

#include <exception>
#include <utility>

using namespace openblack::ecs::save_files;

BackgroundWriter::BackgroundWriter(Failed failed)
    : _failed(std::move(failed))
    , _thread([this](const std::stop_token& stop) { Run(stop); })
{
}

BackgroundWriter::~BackgroundWriter()
{
	Flush();
	_thread.request_stop();
	_wake.notify_all();
}

void BackgroundWriter::Queue(std::string name, Job job)
{
	{
		const std::lock_guard lock(_mutex);
		_jobs.push_back({.name = std::move(name), .job = std::move(job)});
	}
	_wake.notify_one();
}

void BackgroundWriter::Flush()
{
	std::unique_lock lock(_mutex);
	_done.wait(lock, [this]() { return _jobs.empty() && _running == 0; });
}

size_t BackgroundWriter::Pending() const
{
	const std::lock_guard lock(_mutex);
	return _jobs.size() + _running;
}

size_t BackgroundWriter::Failures() const
{
	const std::lock_guard lock(_mutex);
	return _failures;
}

void BackgroundWriter::Run(const std::stop_token& stop)
{
	std::unique_lock lock(_mutex);
	while (true)
	{
		_wake.wait(lock, stop, [this]() { return !_jobs.empty(); });
		if (_jobs.empty())
		{
			// Asked to stop with no job left
			return;
		}
		auto next = std::move(_jobs.front());
		_jobs.pop_front();
		++_running;
		lock.unlock();
		bool written = false;
		try
		{
			written = next.job();
		}
		catch (const std::exception& /*error*/)
		{
			// A job that throws did not write its file
		}
		if (!written && _failed)
		{
			_failed(next.name);
		}
		lock.lock();
		--_running;
		_failures += written ? 0 : 1;
		if (_jobs.empty() && _running == 0)
		{
			_done.notify_all();
		}
	}
}
