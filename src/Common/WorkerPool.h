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
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace openblack
{

/// The threads that do the jobs of one or more worker pools.
///
/// Many threads can give jobs at the same time. Each call gives one set of jobs. A free thread takes a job of the
/// newest set that has jobs left, so that a short set of a frame does not wait behind a long set of the background.
/// The thread that gives a set does jobs of that set too, and waits until all of them are done.
class WorkerThreads
{
public:
	using Job = std::function<void(size_t index, size_t lane)>;

	/// The jobs of one call
	struct Batch
	{
		const Job* job {nullptr};
		size_t count {0};
		/// The index of the next job that a thread takes
		std::atomic<size_t> next {0};
		/// The jobs that are done
		std::atomic<size_t> finished {0};
		/// The threads that do jobs of this set now
		size_t inside {0};
	};

	/// With no threads, the thread that gives the jobs does all of them
	explicit WorkerThreads(size_t threads);
	WorkerThreads(const WorkerThreads&) = delete;
	WorkerThreads& operator=(const WorkerThreads&) = delete;
	/// Stops the threads
	~WorkerThreads();

	/// How many lanes do the jobs: the threads and the thread that gives the jobs, which is lane 0
	[[nodiscard]] size_t GetLaneCount() const { return _threads.size() + 1; }

	/// Gives the set of jobs to the threads and wakes them. The set must stay until Wait returns.
	void Give(Batch& batch);
	/// Does the jobs of the set that no thread took, on the calling thread as lane 0
	void Help(Batch& batch);
	/// Waits until each job of the set is done and no thread uses the set, then removes it
	void Wait(Batch& batch);

private:
	void Work(const std::stop_token& stop, size_t lane);
	/// Does jobs of the set until it has none left. The caller must count itself in the set's inside.
	void DoJobs(Batch& batch, size_t lane);
	/// The newest set with jobs left, or none. Lock the mutex before the call.
	[[nodiscard]] Batch* NextBatch() const;

	std::mutex _mutex;
	std::condition_variable_any _wake;
	std::condition_variable _done;
	/// The sets that are not finished, the newest last
	std::vector<Batch*> _batches;
	std::vector<std::jthread> _threads;
};

/// Threads that stay ready to share a frame's work, so that many independent jobs of a frame finish sooner.
///
/// The pool has threads of its own, or it shares the threads of other pools (see WorkerPools). The thread that calls
/// ForEach does jobs too and waits until all are done. Each job gets its index and the lane that does it, so a job can
/// keep its scratch memory per lane and put its result at its own index. The order in which the jobs run is not fixed:
/// a job must not use the result of another job of the same call.
///
/// A job must only read the game's state and write its own result. It must not use the game's random numbers or
/// change the game's state, because then the game would not be the same from one run to the next.
class WorkerPool
{
public:
	using Job = WorkerThreads::Job;

	/// A pool with threads of its own. With no threads, the calling thread does all the jobs.
	explicit WorkerPool(size_t threads);
	/// A pool that shares the threads with other pools
	explicit WorkerPool(std::shared_ptr<WorkerThreads> threads);
	WorkerPool(const WorkerPool&) = delete;
	WorkerPool& operator=(const WorkerPool&) = delete;
	/// Finishes the jobs of the last Start
	~WorkerPool();

	/// How many threads to share a frame's work with on this machine: all but the game's and the renderer's own
	[[nodiscard]] static size_t DefaultThreadCount();

	/// How many lanes do the jobs: the threads and the calling thread, which is lane 0
	[[nodiscard]] size_t GetLaneCount() const { return _threads->GetLaneCount(); }

	/// Does the job once for each index from 0 to count, then returns. Call it from one thread at a time.
	void ForEach(size_t count, const Job& job);
	/// Gives the job to the threads for each index from 0 to count, and returns at once. The calling thread can do
	/// other work while the threads do the jobs. Call Finish before the next Start. The pool keeps a copy of the job,
	/// but the job must keep the data that it reads until Finish returns.
	void Start(size_t count, Job job);
	/// Does the jobs of the last Start that no thread took, then waits until all are done
	void Finish();

private:
	std::shared_ptr<WorkerThreads> _threads;
	/// The job of the last Start
	Job _started;
	/// How many jobs the last Start gave, 0 when Finish has nothing to do
	size_t _startedCount {0};
	/// The jobs of the last Start, which the threads read until Finish returns
	WorkerThreads::Batch _startedBatch;
};

} // namespace openblack
