/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>

#include <memory>

#include "Common/WorkerPool.h"

/// The threads that pose the animals and the villagers of a frame. Each job poses one thing and writes only the parts
/// of that thing that the checksum does not include.
namespace openblack::ecs::pose_threads
{

#if defined(__EMSCRIPTEN__)
/// The web page has a fixed set of threads, and the threads of a new pool would go past it. On the web, the main thread
/// does all the jobs.
inline constexpr bool k_OnThreads = false;
#else
inline constexpr bool k_OnThreads = true;
#endif

/// A pool for the jobs of the posing walks: with threads where the platform lets the pool have them, else with none
[[nodiscard]] inline std::unique_ptr<WorkerPool> MakePool()
{
	return std::make_unique<WorkerPool>(k_OnThreads ? WorkerPool::DefaultThreadCount() : 0);
}

/// Does the job one time for each index. The pool is made at the first call.
inline void ForEach(std::unique_ptr<WorkerPool>& pool, size_t count, const WorkerPool::Job& job)
{
	if (pool == nullptr)
	{
		pool = MakePool();
	}
	pool->ForEach(count, job);
}

} // namespace openblack::ecs::pose_threads
