/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PreciseWait.h"

#include <thread>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace openblack::precise_wait
{

#if defined(_WIN32)
/// The high-resolution waitable timer of the waiter. None when the system does not have it (before Windows 10 1803).
struct Waiter::Timer
{
	HANDLE handle {nullptr};

	Timer()
	    : handle(CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS))
	{
	}
	~Timer()
	{
		if (handle != nullptr)
		{
			CloseHandle(handle);
		}
	}
	Timer(const Timer&) = delete;
	Timer& operator=(const Timer&) = delete;
	Timer(Timer&&) = delete;
	Timer& operator=(Timer&&) = delete;

	/// Sleep for this time with the timer. False when the timer cannot do it.
	[[nodiscard]] bool Sleep(std::chrono::steady_clock::duration time) const
	{
		if (handle == nullptr)
		{
			// A sleep of the thread can go past its time by one period of the system timer. Thus the thread sleeps for
			// less time, and the waiter yields for the remaining time.
			constexpr std::chrono::milliseconds k_LongestTimerPeriod {16};
			if (time > k_LongestTimerPeriod)
			{
				std::this_thread::sleep_for(time - k_LongestTimerPeriod);
			}
			return true;
		}
		// A negative due time is a time from now, in units of 100 ns
		constexpr int64_t k_NanosecondsPerUnit = 100;
		LARGE_INTEGER dueTime;
		dueTime.QuadPart = -(std::chrono::duration_cast<std::chrono::nanoseconds>(time).count() / k_NanosecondsPerUnit);
		if (SetWaitableTimer(handle, &dueTime, 0, nullptr, nullptr, FALSE) == 0)
		{
			return false;
		}
		return WaitForSingleObject(handle, INFINITE) == WAIT_OBJECT_0;
	}
};
#else
/// The other systems do not need a timer: their sleep is precise
struct Waiter::Timer
{
	[[nodiscard]] static bool Sleep([[maybe_unused]] std::chrono::steady_clock::duration time) { return false; }
};
#endif

Waiter::Waiter()
    : _timer(std::make_unique<Timer>())
{
}

Waiter::~Waiter() = default;
Waiter::Waiter(Waiter&&) noexcept = default;
Waiter& Waiter::operator=(Waiter&&) noexcept = default;

void Waiter::WaitUntil(std::chrono::steady_clock::time_point due)
{
	using Clock = std::chrono::steady_clock;
	// The thread sleeps until the last part of the wait
	if (const auto sleep = due - k_YieldPart - Clock::now(); sleep > Clock::duration::zero())
	{
		if (!_timer->Sleep(sleep))
		{
			std::this_thread::sleep_until(due - k_YieldPart);
		}
	}
	// The thread yields until the time comes
	while (Clock::now() < due)
	{
		std::this_thread::yield();
	}
}

} // namespace openblack::precise_wait
