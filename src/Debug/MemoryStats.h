/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <string>

/// (openblack) The memory openblack uses, for the profile log (OPENBLACK_PROFILE, Profiler.cpp) and the map cycle leak
/// check: the process's RAM (Windows GetProcessMemoryInfo; 0 elsewhere) and bgfx's own counters (VRAM of textures and
/// render targets, the GPU total when the backend reports it, the transient buffers used this frame).
namespace openblack::memory_stats
{

struct Sample
{
	uint64_t workingSet {0};     ///< RAM in use now
	uint64_t peakWorkingSet {0}; ///< the most RAM it has used
	uint64_t privateBytes {0};   ///< committed memory of its own
	int64_t gpuUsed {0};         ///< bgfx::Stats::gpuMemoryUsed (-INT64_MAX when the backend does not say)
	int64_t gpuMax {0};          ///< bgfx::Stats::gpuMemoryMax
	int64_t textures {0};        ///< bgfx::Stats::textureMemoryUsed
	int64_t renderTargets {0};   ///< bgfx::Stats::rtMemoryUsed
	int32_t transientVb {0};     ///< bgfx::Stats::transientVbUsed
	int32_t transientIb {0};     ///< bgfx::Stats::transientIbUsed
};

/// bgfx must be initialised (the main thread, after the renderer is up); without it only the process part is set
[[nodiscard]] Sample Take(bool withBgfx);
/// One line for the log: "RAM 812.4 MB (peak 900.1, private 760.2), VRAM textures 210.3 MB, targets 64.0 MB, ..."
[[nodiscard]] std::string Format(const Sample& sample);

} // namespace openblack::memory_stats
