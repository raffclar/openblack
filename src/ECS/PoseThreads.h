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

#include <algorithm>
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

/// The runs of indices that each lane gets for one call. A thread takes a run, not one index, so that the threads share
/// fewer counters.
inline constexpr size_t k_RunsPerLane = 4;

/// The most jobs of one walk that the calling thread does alone. Below these counts, the time to wake the threads is
/// more than the work that they take, on the desktop and in the browser. The jobs of each walk have a different size,
/// so each walk has its own count.
inline constexpr size_t k_AnimalPosesOnCaller = 192;
inline constexpr size_t k_VillagerClipsOnCaller = 512;
inline constexpr size_t k_VillagerPosesOnCaller = 128;

/// Does the job one time for each index. When the count is at most onCaller, the calling thread does all the jobs in
/// order as lane 0. Otherwise the pool is made at the first such call, and each thread does a run of indices in order.
inline void ForEach(std::unique_ptr<WorkerPool>& pool, size_t count, size_t onCaller, const WorkerPool::Job& job)
{
	if (count <= onCaller)
	{
		for (size_t index = 0; index < count; ++index)
		{
			job(index, 0);
		}
		return;
	}
	if (pool == nullptr)
	{
		pool = MakePool();
	}
	const size_t runs = std::min(count, pool->GetLaneCount() * k_RunsPerLane);
	const size_t length = (count + runs - 1) / runs;
	const size_t used = (count + length - 1) / length;
	pool->ForEach(used, [&job, count, length](size_t run, size_t lane) {
		const size_t end = std::min(count, (run + 1) * length);
		for (size_t index = run * length; index < end; ++index)
		{
			job(index, lane);
		}
	});
}

} // namespace openblack::ecs::pose_threads
