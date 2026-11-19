/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ThreadCpuTime.h"

#ifdef _WIN32
#include <chrono>

#include <intrin.h>
#include <windows.h>
#elif !defined(__EMSCRIPTEN__)
#include <ctime>
#endif

namespace openblack::debug
{

namespace
{

#ifdef _WIN32
/// The processor's time stamp counter ticks in a millisecond, measured over two milliseconds
double TicksPerMillisecond()
{
	const auto start = std::chrono::steady_clock::now();
	const auto first = __rdtsc();
	while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(2))
	{
	}
	const auto last = __rdtsc();
	const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
	return static_cast<double>(last - first) / elapsed;
}
#endif

} // namespace

std::optional<double> ThreadCpuMilliseconds()
{
#if defined(__EMSCRIPTEN__)
	return std::nullopt;
#elif defined(_WIN32)
	static const double k_TicksPerMillisecond = TicksPerMillisecond();
	ULONG64 cycles = 0;
	QueryThreadCycleTime(GetCurrentThread(), &cycles);
	return static_cast<double>(cycles) / k_TicksPerMillisecond;
#else
	timespec time {};
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &time);
	return static_cast<double>(time.tv_sec) * 1000.0 + static_cast<double>(time.tv_nsec) / 1.0e6;
#endif
}

} // namespace openblack::debug
