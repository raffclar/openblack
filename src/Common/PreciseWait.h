/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <chrono>
#include <memory>

namespace openblack::precise_wait
{

/// A wait to a time of the steady clock that does not go past that time by more than a fraction of a millisecond. On
/// Windows, a sleep of the thread can go past its time by up to the period of the system timer (often 1 ms to 15.6 ms).
/// Thus the waiter uses a high-resolution waitable timer for most of the wait. Then it yields the thread until the time
/// comes. Without that timer, the thread sleeps for less time and yields for longer. On other systems, the sleep of the
/// thread is precise, and the waiter yields only for the last part.
class Waiter
{
public:
	/// The last part of the wait, in which the waiter yields the thread and does not sleep
	static constexpr std::chrono::microseconds k_YieldPart {1000};

	Waiter();
	~Waiter();
	Waiter(const Waiter&) = delete;
	Waiter& operator=(const Waiter&) = delete;
	Waiter(Waiter&&) noexcept;
	Waiter& operator=(Waiter&&) noexcept;

	/// Wait until the time comes. No wait when the time is now or before now.
	void WaitUntil(std::chrono::steady_clock::time_point due);

private:
	struct Timer;
	std::unique_ptr<Timer> _timer;
};

} // namespace openblack::precise_wait
