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

#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>

namespace openblack::audio
{

/// Runs jobs one after the other on one thread that lives as long as the executor. A job that is submitted first
/// runs first. The executor starts no thread for a job, which is important on the web, where a new thread can need
/// a new worker. The destructor runs all the jobs that are left, so that no future stays without a value.
class SerialExecutor
{
public:
	SerialExecutor()
	{
		_thread = std::thread([this] { Run(); });
	}

	~SerialExecutor()
	{
		{
			const std::lock_guard lock(_mutex);
			_stopping = true;
		}
		_wake.notify_one();
		_thread.join();
	}

	SerialExecutor(const SerialExecutor&) = delete;
	SerialExecutor& operator=(const SerialExecutor&) = delete;

	/// Adds a job to the end of the queue. The future gets the value of the job when the job ends.
	template <typename Job>
	[[nodiscard]] auto Submit(Job&& job) -> std::shared_future<std::invoke_result_t<Job&>>
	{
		auto task = std::make_shared<std::packaged_task<std::invoke_result_t<Job&>()>>(std::forward<Job>(job));
		auto future = task->get_future().share();
		{
			const std::lock_guard lock(_mutex);
			_jobs.emplace_back([task = std::move(task)] { (*task)(); });
		}
		_wake.notify_one();
		return future;
	}

private:
	void Run()
	{
		for (;;)
		{
			std::function<void()> job;
			{
				std::unique_lock lock(_mutex);
				_wake.wait(lock, [this] { return _stopping || !_jobs.empty(); });
				if (_jobs.empty())
				{
					return;
				}
				job = std::move(_jobs.front());
				_jobs.pop_front();
			}
			job();
		}
	}

	std::mutex _mutex;
	std::condition_variable _wake;
	std::deque<std::function<void()>> _jobs;
	bool _stopping {false};
	std::thread _thread;
};

} // namespace openblack::audio
