/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "SimulationSystem.h"

#include <utility>

#if defined(_WIN32)
#include <windows.h>
// psapi.h needs the types of windows.h first
#include <psapi.h>
#elif defined(__linux__)
#include <fstream>

#include <unistd.h>
#endif

using namespace openblack::ecs::systems;

openblack::sim_run::Timings SimulationSystem::TakeTimings()
{
	return std::exchange(_timings, sim_run::Timings {});
}

std::optional<uint64_t> SimulationSystem::ProcessMemoryBytes() const
{
#if defined(_WIN32)
	PROCESS_MEMORY_COUNTERS counters {};
	if (K32GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)) == 0)
	{
		return std::nullopt;
	}
	return static_cast<uint64_t>(counters.WorkingSetSize);
#elif defined(__linux__)
	// The second number of the file is the pages that the process has in memory
	std::ifstream statm("/proc/self/statm");
	uint64_t size = 0;
	uint64_t resident = 0;
	if (!(statm >> size >> resident))
	{
		return std::nullopt;
	}
	return resident * static_cast<uint64_t>(sysconf(_SC_PAGESIZE));
#else
	return std::nullopt;
#endif
}
