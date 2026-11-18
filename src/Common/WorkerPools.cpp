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
#include "WebThreadBudget.h"

using namespace openblack;

WorkerPools::WorkerPools(std::optional<size_t> sharedThreads)
    : _shared(sharedThreads ? std::make_shared<WorkerThreads>(*sharedThreads) : nullptr)
{
}

std::optional<size_t> WorkerPools::SharedThreadCount()
{
#if defined(__EMSCRIPTEN__)
	// The shared threads are a part of the workers that the page makes at the start (see Common/WebThreadBudget.h)
	return std::min(WorkerPool::DefaultThreadCount(), web_threads::k_SharedWorkers);
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
#if defined(__EMSCRIPTEN__)
	// In the browser, a pool with threads of its own would go past the workers of the page. With no service, the
	// calling thread does all the jobs.
	return std::make_unique<WorkerPool>(0);
#else
	return std::make_unique<WorkerPool>(WorkerPool::DefaultThreadCount());
#endif
}
