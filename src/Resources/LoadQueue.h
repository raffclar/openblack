/******************************************************************************
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

#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

#include "Graphics/UploadPacer.h"

namespace openblack::resources
{

/// Threads that load resources in the background, in the order they were asked for
class LoadQueue
{
public:
	/// With no threads, nothing is loaded in the background: each job runs when it is waited for
	explicit LoadQueue(size_t threads);
	LoadQueue(const LoadQueue&) = delete;
	LoadQueue& operator=(const LoadQueue&) = delete;
	/// Drops the jobs not started and waits for those running
	~LoadQueue();

	/// How many threads to load with on this machine: all but the ones the game and the renderer run on
	[[nodiscard]] static size_t DefaultThreadCount();

	[[nodiscard]] size_t GetThreadCount() const { return _threads.size(); }

	/// Runs the job on one of the threads; it must not touch what only the main thread may
	void Submit(std::function<void()> job);

	/// Drops the jobs not started and waits for those running to end
	void Cancel();

	/// Waits until every job asked for has run
	void WaitIdle();

	/// What paces how much the threads hand the graphics card each frame
	[[nodiscard]] graphics::UploadPacer& GetUploadPacer() { return _uploadPacer; }

private:
	void Work(const std::stop_token& stop);

	graphics::UploadPacer _uploadPacer;
	std::mutex _mutex;
	std::condition_variable_any _wake;
	std::condition_variable _idle;
	std::deque<std::function<void()>> _jobs;
	size_t _running {0};
	std::vector<std::jthread> _threads;
};

} // namespace openblack::resources
