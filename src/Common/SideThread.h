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

#include <atomic>
#include <exception>
#include <functional>
#include <mutex>
#include <stop_token>
#include <thread>

namespace openblack
{

/// A thread that does one piece of work at a time beside the main thread, such as the turns of the next frame while the
/// main thread finishes the draw. The main thread gives the work with Start and waits for its end with Wait. The thread
/// stays for the next work, so that each piece of work does not make a new thread.
class SideThread
{
public:
	SideThread();
	SideThread(const SideThread&) = delete;
	SideThread& operator=(const SideThread&) = delete;
	/// Waits for the work that runs, then stops the thread
	~SideThread();

	/// Gives the thread its work. The thread must have no work. When the work ends, the thread calls `ended`, if it has
	/// one, after Done gives true.
	void Start(std::function<void()> work, std::function<void()> ended = {});
	/// The work given last ended, or no work was given
	[[nodiscard]] bool Done() const noexcept;
	/// Waits until the work given last ends. An exception of the work comes out here.
	void Wait();
	/// The thread that does the work
	[[nodiscard]] std::thread::id GetId() const noexcept;

private:
	void Loop(const std::stop_token& stop);

	mutable std::mutex _mutex;
	std::condition_variable_any _changed;
	std::function<void()> _work;
	std::function<void()> _ended;
	std::exception_ptr _error;
	std::atomic<bool> _done {true};
	/// The last member, so that the thread starts after the others are made and stops before they go
	std::jthread _thread;
};

} // namespace openblack
