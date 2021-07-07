/*****************************************************************************
 * Copyright (c) 2018-2020 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#ifdef __linux__
#include <sys/sysinfo.h>
#endif

#ifdef __APPLE__
#include <mach/mach_init.h>
#include <mach/task.h>
#endif

#ifdef _WINDOWS
extern "C" {
#include <windows.h>
#include <psapi.h>
};
#else
#include <sys/resource.h>
#endif

class MemoryStats
{
	/// The amount of memory currently being used by this process, in bytes.
	/// By default, returns the full virtual arena, but if resident=true,
	/// it will report just the resident set in RAM (if supported on that OS).
	size_t MemoryUsed(bool resident = false)
	{
#if defined(__linux__)
		// Ugh, getrusage doesn't work well on Linux.  Try grabbing info
		// directly from the /proc pseudo-filesystem.  Reading from
		// /proc/self/statm gives info on your own process, as one line of
		// numbers that are: virtual mem program size, resident set size,
		// shared pages, text/code, data/stack, library, dirty pages.  The
		// mem sizes should all be multiplied by the page size.
		size_t size = 0;
		FILE* file = fopen("/proc/self/statm", "r");
		if (file)
		{
			unsigned long vm = 0;
			fscanf(file, "%ul", &vm); // Just need the first num: vm size
			fclose(file);
			size = (size_t)vm * getpagesize();
		}
		return size;
#elif defined(__APPLE__)
		// Inspired by:
		// http://miknight.blogspot.com/2005/11/resident-set-size-in-mac-os-x.html
		struct task_basic_info t_info;
		mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;
		task_info(current_task(), TASK_BASIC_INFO, (task_info_t)&t_info, &t_info_count);
		size_t size = (resident ? t_info.resident_size : t_info.virtual_size);
		return size;
#elif defined(_WINDOWS)
		// According to MSDN...
		PROCESS_MEMORY_COUNTERS counters;
		if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)))
			return counters.PagefileUsage;
		else
			return 0;
#else
		return 0;
#endif
	}
};
