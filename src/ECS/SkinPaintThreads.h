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
#include <span>

#include "Common/WorkerPool.h"
#include "ECS/PoseThreads.h"

/// The threads that paint the creature skins of a frame. The main thread collects the paints first, because a paint can
/// need a mesh file that the main thread loads. Then each job paints one skin and writes only that skin.
namespace openblack::ecs::skin_paint_threads
{

/// The most paints of one frame that the calling thread does alone. One paint takes more time than the wake of the
/// threads, so two paints or more go to the pool.
inline constexpr size_t k_PaintsOnCaller = 1;

/// Does the paint one time for each job. Each job must write only its own skin, so the order of the jobs does not
/// change the result. The pool is made at the first frame that has more than one paint.
template <typename Job, typename Paint>
void PaintAll(std::unique_ptr<WorkerPool>& pool, std::span<Job> jobs, const Paint& paint)
{
	pose_threads::ForEach(pool, jobs.size(), k_PaintsOnCaller,
	                      [jobs, &paint](size_t index, size_t /*lane*/) { paint(jobs[index]); });
}

} // namespace openblack::ecs::skin_paint_threads
