/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MemoryStats.h"

#include <bgfx/bgfx.h>
#include <fmt/format.h>

#if defined(_WIN32)
// (NOMINMAX and WIN32_LEAN_AND_MEAN come from src/CMakeLists.txt)
#include <windows.h>
// after windows.h
#include <psapi.h>
#endif

namespace openblack::memory_stats
{
namespace
{
constexpr double k_Mb = 1024.0 * 1024.0;
}

Sample Take(bool withBgfx)
{
	Sample sample;
#if defined(_WIN32)
	PROCESS_MEMORY_COUNTERS_EX counters {};
	counters.cb = sizeof(counters);
	// with PSAPI_VERSION 2 (the default for the SDK's targets) the macro is K32GetProcessMemoryInfo, in kernel32
	if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters)) != 0)
	{
		sample.workingSet = counters.WorkingSetSize;
		sample.peakWorkingSet = counters.PeakWorkingSetSize;
		sample.privateBytes = counters.PrivateUsage;
	}
#endif
	if (withBgfx)
	{
		if (const auto* stats = bgfx::getStats(); stats != nullptr)
		{
			sample.gpuUsed = stats->gpuMemoryUsed;
			sample.gpuMax = stats->gpuMemoryMax;
			sample.textures = stats->textureMemoryUsed;
			sample.renderTargets = stats->rtMemoryUsed;
			sample.transientVb = stats->transientVbUsed;
			sample.transientIb = stats->transientIbUsed;
		}
	}
	return sample;
}

std::string Format(const Sample& sample)
{
	std::string text = fmt::format("RAM {:.1f} MB (peak {:.1f}, private {:.1f})", sample.workingSet / k_Mb,
	                               sample.peakWorkingSet / k_Mb, sample.privateBytes / k_Mb);
	text += fmt::format(", VRAM textures {:.1f} MB, targets {:.1f} MB", sample.textures / k_Mb, sample.renderTargets / k_Mb);
	if (sample.gpuUsed > 0)
	{
		text += fmt::format(", GPU {:.1f} / {:.1f} MB", sample.gpuUsed / k_Mb, sample.gpuMax / k_Mb);
	}
	text += fmt::format(", transient VB {:.1f} MB IB {:.1f} MB", sample.transientVb / k_Mb, sample.transientIb / k_Mb);
	return text;
}

} // namespace openblack::memory_stats
