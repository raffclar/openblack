/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WorkerPools.h"

#include <algorithm>

#include "Locator.h"

using namespace openblack;

WorkerPools::WorkerPools(std::optional<size_t> sharedThreads)
    : _shared(sharedThreads ? std::make_shared<WorkerThreads>(*sharedThreads) : nullptr)
{
}

std::optional<size_t> WorkerPools::SharedThreadCount()
{
#if defined(__EMSCRIPTEN__)
	// The shared threads are a part of the workers that the page makes at the start (PTHREAD_POOL_SIZE in
	// src/CMakeLists.txt). The other workers are for the game thread, the render thread, the load queue, the sound
	// and the side threads.
	constexpr size_t k_MostSharedThreads = 6;
	return std::min(WorkerPool::DefaultThreadCount(), k_MostSharedThreads);
#else
	return std::nullopt;
#endif
}

std::unique_ptr<WorkerPool> WorkerPools::Make() const
{
	if (_shared)
	{
		return std::make_unique<WorkerPool>(_shared);
	}
	return std::make_unique<WorkerPool>(WorkerPool::DefaultThreadCount());
}

std::unique_ptr<WorkerPool> openblack::MakeWorkerPool()
{
	if (Locator::workerPools::has_value())
	{
		return Locator::workerPools::value().Make();
	}
	return std::make_unique<WorkerPool>(WorkerPool::DefaultThreadCount());
}
