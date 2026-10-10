/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LoadQueue.h"

#include <algorithm>
#include <utility>

using namespace openblack::resources;

LoadQueue::LoadQueue(size_t threads)
{
	_threads.reserve(threads);
	for (size_t i = 0; i < threads; ++i)
	{
		_threads.emplace_back([this](const std::stop_token& stop) { Work(stop); });
	}
}

LoadQueue::~LoadQueue()
{
	Cancel();
	for (auto& thread : _threads)
	{
		thread.request_stop();
	}
	_wake.notify_all();
	// The threads are joined as they go
}

size_t LoadQueue::DefaultThreadCount()
{
#if defined(__ANDROID__)
	// Android's files are read through the Java environment of the thread that opened them, so everything is loaded on
	// the main thread when first wanted
	return 0;
#else
	// The game's main thread and the renderer's keep two cores busy
	constexpr size_t k_ReservedThreads = 2;
	constexpr size_t k_MostThreads = 6;
	const size_t hardware = std::thread::hardware_concurrency();
	return std::clamp<size_t>(hardware > k_ReservedThreads ? hardware - k_ReservedThreads : 1, 1, k_MostThreads);
#endif
}

void LoadQueue::Submit(std::function<void()> job)
{
	if (_threads.empty())
	{
		return;
	}
	{
		const std::scoped_lock lock(_mutex);
		_jobs.push_back(std::move(job));
	}
	_wake.notify_one();
}

void LoadQueue::Cancel()
{
	// A job waiting for room to upload in a frame that won't come would never end
	_uploadPacer.Release();
	std::unique_lock lock(_mutex);
	_jobs.clear();
	_idle.wait(lock, [this] { return _running == 0; });
}

void LoadQueue::WaitIdle()
{
	std::unique_lock lock(_mutex);
	_idle.wait(lock, [this] { return _jobs.empty() && _running == 0; });
}

void LoadQueue::Work(const std::stop_token& stop)
{
	const graphics::UploadPacer::Scope paced(_uploadPacer);
	while (true)
	{
		std::function<void()> job;
		{
			std::unique_lock lock(_mutex);
			if (!_wake.wait(lock, stop, [this] { return !_jobs.empty(); }))
			{
				return;
			}
			job = std::move(_jobs.front());
			_jobs.pop_front();
			++_running;
		}
		job();
		{
			const std::scoped_lock lock(_mutex);
			--_running;
		}
		_idle.notify_all();
	}
}
