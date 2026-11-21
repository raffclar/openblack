/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS 1

#include "MindWorkSystem.h"

#include <algorithm>

#include "Common/WorkerPool.h"
#include "Common/WorkerPools.h"

using namespace openblack::ecs::systems;

MindWorkSystem::MindWorkSystem() = default;

MindWorkSystem::~MindWorkSystem() = default;

openblack::WorkerPool* MindWorkSystem::Pool()
{
	if (_pool == nullptr)
	{
#if defined(__EMSCRIPTEN__)
		// In the browser, a pool with threads of its own would go past the workers of the page. A count above 0 uses the
		// threads that all pools share.
		if (!_threads.has_value() || *_threads != 0)
		{
			_pool = MakeWorkerPool();
		}
#else
		if (!_threads.has_value())
		{
			_pool = MakeWorkerPool();
		}
		else if (*_threads != 0)
		{
			_pool = std::make_unique<WorkerPool>(*_threads);
		}
#endif
	}
	return _pool.get();
}

void MindWorkSystem::ForEach(size_t count, size_t onCaller, const Job& job)
{
	auto* pool = count > onCaller ? Pool() : nullptr;
	// Only one call at a time gives jobs to the threads. Another call does its jobs alone, with the same results.
	if (pool == nullptr || pool->GetLaneCount() == 1 || _busy.exchange(true))
	{
		for (size_t index = 0; index < count; ++index)
		{
			job(index);
		}
		return;
	}
	const size_t runs = std::min(count, pool->GetLaneCount() * k_RunsPerLane);
	const size_t length = (count + runs - 1) / runs;
	const size_t used = (count + length - 1) / length;
	pool->ForEach(used, [&job, count, length](size_t run, size_t /*lane*/) {
		const size_t end = std::min(count, (run + 1) * length);
		for (size_t index = run * length; index < end; ++index)
		{
			job(index);
		}
	});
	_busy = false;
}

void MindWorkSystem::SetThreads(std::optional<size_t> threads)
{
	_threads = threads;
	_pool.reset();
}

size_t MindWorkSystem::GetThreads() const
{
	if (_pool != nullptr)
	{
		return _pool->GetLaneCount() - 1;
	}
	if (_threads.has_value())
	{
		return *_threads;
	}
	if (const auto shared = WorkerPools::SharedThreadCount())
	{
		return *shared;
	}
	return WorkerPool::DefaultThreadCount();
}
