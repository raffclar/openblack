/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WorkerPool.h"

#include <algorithm>

using namespace openblack;

WorkerPool::WorkerPool(size_t threads)
{
	_threads.reserve(threads);
	for (size_t i = 0; i < threads; ++i)
	{
		_threads.emplace_back([this, lane = i + 1](const std::stop_token& stop) { Work(stop, lane); });
	}
}

WorkerPool::~WorkerPool()
{
	for (auto& thread : _threads)
	{
		thread.request_stop();
	}
	_wake.notify_all();
	// Each thread joins when it is destroyed
}

size_t WorkerPool::DefaultThreadCount()
{
	// Keep one core for the game and one for the renderer, and do not use more than a few: the jobs of a frame are
	// small, and more threads only add the time to wake them
	constexpr size_t k_Reserved = 2;
	constexpr size_t k_Most = 6;
	const size_t hardware = std::thread::hardware_concurrency();
	return hardware > k_Reserved ? std::min(hardware - k_Reserved, k_Most) : 0;
}

void WorkerPool::ForEach(size_t count, const Job& job)
{
	if (count == 0)
	{
		return;
	}
	// For one job, or with no threads, the calling thread does all the jobs and wakes no thread
	if (count == 1 || _threads.empty())
	{
		for (size_t index = 0; index < count; ++index)
		{
			job(index, 0);
		}
		return;
	}
	{
		// A thread that woke late for the last call must see that no job is left before the jobs change
		std::unique_lock lock(_mutex);
		_done.wait(lock, [this] { return _busy == 0; });
		_job = &job;
		_count = count;
		_next.store(0, std::memory_order_relaxed);
		_finished.store(0, std::memory_order_relaxed);
		_busy = _threads.size();
		++_generation;
	}
	_wake.notify_all();
	DoJobs(0);
	// Return when each job is done. A thread that did not wake yet finds no job and does not touch the job.
	std::unique_lock lock(_mutex);
	_done.wait(lock, [this, count] { return _finished.load(std::memory_order_acquire) == count; });
}

void WorkerPool::DoJobs(size_t lane)
{
	for (auto index = _next.fetch_add(1, std::memory_order_relaxed); index < _count;
	     index = _next.fetch_add(1, std::memory_order_relaxed))
	{
		(*_job)(index, lane);
		if (_finished.fetch_add(1, std::memory_order_acq_rel) + 1 == _count)
		{
			// Lock the mutex so that the calling thread cannot miss the signal between its test and its wait
			{
				std::scoped_lock lock(_mutex);
			}
			_done.notify_all();
		}
	}
}

void WorkerPool::Work(const std::stop_token& stop, size_t lane)
{
	uint64_t seen = 0;
	while (true)
	{
		{
			std::unique_lock lock(_mutex);
			if (!_wake.wait(lock, stop, [this, seen] { return _generation != seen; }))
			{
				return;
			}
			seen = _generation;
		}
		DoJobs(lane);
		{
			std::scoped_lock lock(_mutex);
			--_busy;
		}
		_done.notify_all();
	}
}
