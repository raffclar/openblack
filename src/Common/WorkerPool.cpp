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
#include <ranges>
#include <utility>

using namespace openblack;

WorkerThreads::WorkerThreads(size_t threads)
{
	_threads.reserve(threads);
	for (size_t i = 0; i < threads; ++i)
	{
		_threads.emplace_back([this, lane = i + 1](const std::stop_token& stop) { Work(stop, lane); });
	}
}

WorkerThreads::~WorkerThreads()
{
	for (auto& thread : _threads)
	{
		thread.request_stop();
	}
	_wake.notify_all();
	// Each thread joins when it is destroyed
}

void WorkerThreads::Give(Batch& batch)
{
	batch.next.store(0, std::memory_order_relaxed);
	batch.finished.store(0, std::memory_order_relaxed);
	batch.inside = 1;
	{
		const std::scoped_lock lock(_mutex);
		_batches.push_back(&batch);
	}
	_wake.notify_all();
}

void WorkerThreads::Help(Batch& batch)
{
	DoJobs(batch, 0);
}

void WorkerThreads::Wait(Batch& batch)
{
	// The thread that gave the set leaves it. Return when each job is done and no other thread still reads the set.
	std::unique_lock lock(_mutex);
	--batch.inside;
	_done.wait(lock, [&batch] { return batch.finished.load(std::memory_order_acquire) == batch.count && batch.inside == 0; });
	std::erase(_batches, &batch);
}

WorkerThreads::Batch* WorkerThreads::NextBatch() const
{
	auto newestFirst = _batches | std::views::reverse;
	const auto found = std::ranges::find_if(
	    newestFirst, [](const Batch* batch) { return batch->next.load(std::memory_order_relaxed) < batch->count; });
	return found == newestFirst.end() ? nullptr : *found;
}

void WorkerThreads::DoJobs(Batch& batch, size_t lane)
{
	for (auto index = batch.next.fetch_add(1, std::memory_order_relaxed); index < batch.count;
	     index = batch.next.fetch_add(1, std::memory_order_relaxed))
	{
		(*batch.job)(index, lane);
		if (batch.finished.fetch_add(1, std::memory_order_acq_rel) + 1 == batch.count)
		{
			// Lock the mutex so that the waiting thread cannot miss the signal between its test and its wait
			{
				const std::scoped_lock lock(_mutex);
			}
			_done.notify_all();
		}
	}
}

void WorkerThreads::Work(const std::stop_token& stop, size_t lane)
{
	while (true)
	{
		Batch* batch = nullptr;
		{
			std::unique_lock lock(_mutex);
			if (!_wake.wait(lock, stop, [this, &batch] {
				    batch = NextBatch();
				    return batch != nullptr;
			    }))
			{
				return;
			}
			// The set stays while a thread is inside it
			++batch->inside;
		}
		DoJobs(*batch, lane);
		{
			const std::scoped_lock lock(_mutex);
			--batch->inside;
		}
		_done.notify_all();
	}
}

WorkerPool::WorkerPool(size_t threads)
    : _threads(std::make_shared<WorkerThreads>(threads))
{
}

WorkerPool::WorkerPool(std::shared_ptr<WorkerThreads> threads)
    : _threads(std::move(threads))
{
}

WorkerPool::~WorkerPool()
{
	Finish();
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
	if (count == 1 || _threads->GetLaneCount() == 1)
	{
		for (size_t index = 0; index < count; ++index)
		{
			job(index, 0);
		}
		return;
	}
	WorkerThreads::Batch batch {.job = &job, .count = count};
	_threads->Give(batch);
	_threads->Help(batch);
	_threads->Wait(batch);
}

void WorkerPool::Start(size_t count, Job job)
{
	_started = std::move(job);
	_startedCount = count;
	// With no threads, Finish does all the jobs on the calling thread
	if (count != 0 && _threads->GetLaneCount() > 1)
	{
		_startedBatch.job = &_started;
		_startedBatch.count = count;
		_threads->Give(_startedBatch);
	}
}

void WorkerPool::Finish()
{
	const auto count = std::exchange(_startedCount, 0);
	if (count == 0)
	{
		return;
	}
	if (_threads->GetLaneCount() == 1)
	{
		for (size_t index = 0; index < count; ++index)
		{
			_started(index, 0);
		}
		return;
	}
	_threads->Help(_startedBatch);
	_threads->Wait(_startedBatch);
}
