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

#include <array>
#include <map>
#include <string>

#include "Common/FrameStats.h"

#include "../Profiler.h"

namespace openblack::debug
{

/// Logs how long the frames take every so many frames: the average, 95th percentile and slowest frame, what the
/// renderer reports of the CPU and GPU, the average of each profiler stage and, when the renderer profiles its views,
/// the GPU time of each view
class FrameStatsLog
{
public:
	/// pipelinesMade: how many pipelines the renderer made before the first frame
	FrameStatsLog(uint32_t interval, bool views, uint32_t pipelinesMade);

	/// After each frame, with how long the frame took, the profiler that timed its stages and how many pipelines the
	/// renderer made since it started
	void Frame(float milliseconds, const openblack::Profiler& profiler, uint32_t pipelinesMade);

	/// The log shows each frame that takes this time or more, with its stages
	static constexpr float k_SpikeMilliseconds = 10.0f;

private:
	void Report();

	uint32_t _interval;
	bool _views;
	frame_stats::Window _frames;
	frame_stats::Window _gpu;
	std::array<double, static_cast<size_t>(openblack::Profiler::Stage::_count)> _stageTotals {};
	std::array<double, static_cast<size_t>(openblack::Profiler::Stage::_count)> _stageMax {};
	std::map<std::string, double> _viewTotals;
	double _submitTotal {0.0};
	double _waitRenderTotal {0.0};
	uint64_t _drawCallTotal {0};
	uint32_t _reports {0};
	uint64_t _frameNumber {0};
	int64_t _lastResourceCount {0};
	uint32_t _lastPipelinesMade {0};
	uint32_t _pipelinesSinceReport {0};
	/// The time that the game's thread was on a processor, at the end of the last frame
	double _lastCpuMilliseconds {0.0};
};

} // namespace openblack::debug
