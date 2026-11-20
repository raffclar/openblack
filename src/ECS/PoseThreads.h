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
#include "Common/WorkerPools.h"

/// The threads that pose the animals and the villagers of a frame. Each job poses one thing and writes only the parts
/// of that thing that the checksum does not include.
namespace openblack::ecs::pose_threads
{

/// A pool for the jobs of the posing walks. In the browser, the pool uses the threads that all pools share, so the
/// posing walks make no new thread.
[[nodiscard]] inline std::unique_ptr<WorkerPool> MakePool()
{
	return MakeWorkerPool();
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
