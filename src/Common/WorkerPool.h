/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdint>

#include <atomic>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace openblack
{

/// Threads that stay ready to share a frame's work, so that many independent jobs of a frame finish sooner.
///
/// The thread that calls ForEach does jobs too and waits until all are done. It does not wait for a thread that wakes
/// late and finds no job: the next call waits for that thread before it gives new jobs. Each job gets its index and the
/// lane that does it, so a job can keep its scratch memory per lane and put its result at its own index. The order in
/// which the jobs run is not fixed: a job must not use the result of another job of the same call.
///
/// A job must only read the game's state and write its own result. It must not use the game's random numbers or
/// change the game's state, because then the game would not be the same from one run to the next.
class WorkerPool
{
public:
	using Job = std::function<void(size_t index, size_t lane)>;

	/// With no threads, the calling thread does all the jobs
	explicit WorkerPool(size_t threads);
	WorkerPool(const WorkerPool&) = delete;
	WorkerPool& operator=(const WorkerPool&) = delete;
	/// Stops the threads
	~WorkerPool();

	/// How many threads to share a frame's work with on this machine: all but the game's and the renderer's own
	[[nodiscard]] static size_t DefaultThreadCount();

	/// How many lanes do the jobs: the threads and the calling thread, which is lane 0
	[[nodiscard]] size_t GetLaneCount() const { return _threads.size() + 1; }

	/// Does the job once for each index from 0 to count, then returns. Call it from one thread at a time.
	void ForEach(size_t count, const Job& job);

private:
	void Work(const std::stop_token& stop, size_t lane);
	void DoJobs(size_t lane);

	std::mutex _mutex;
	std::condition_variable_any _wake;
	std::condition_variable _done;
	const Job* _job {nullptr};
	size_t _count {0};
	uint64_t _generation {0};
	/// The threads that did not yet see that the jobs of the last call are all started
	size_t _busy {0};
	std::atomic<size_t> _next {0};
	/// The jobs of this call that are done
	std::atomic<size_t> _finished {0};
	std::vector<std::jthread> _threads;
};

} // namespace openblack
