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
#include <filesystem>
#include <map>
#include <optional>
#include <string>

#include "Common/FrameStats.h"

#include "../Profiler.h"

namespace openblack::debug
{

/// Logs how long the frames take every so many frames: the average, 95th percentile and slowest frame, what the
/// renderer reports of the CPU and GPU, the average of each profiler stage and, when the renderer profiles its views,
/// the GPU time of each view, and how much memory the process and the graphics hold
class FrameStatsLog
{
public:
	/// pipelinesMade: how many pipelines the renderer made before the first frame. out: a file to write each frame of
	/// the run into, as CSV, when the run ends; none when empty.
	FrameStatsLog(uint32_t interval, bool views, uint32_t pipelinesMade, std::filesystem::path out = {});
	/// Logs the summary of all the frames of the run, and writes them into the file if there is one
	~FrameStatsLog();
	FrameStatsLog(const FrameStatsLog&) = delete;
	FrameStatsLog& operator=(const FrameStatsLog&) = delete;
	FrameStatsLog(FrameStatsLog&&) = delete;
	FrameStatsLog& operator=(FrameStatsLog&&) = delete;

	/// After each frame, with how long the frame took, the profiler that timed its stages, how many pipelines the
	/// renderer made since it started and the game turn at the end of the frame
	void Frame(float milliseconds, const openblack::Profiler& profiler, uint32_t pipelinesMade, uint32_t turn);

	/// The log shows each frame that takes this time or more, with its stages
	static constexpr float k_SpikeMilliseconds = frame_stats::k_SlowMilliseconds;

private:
	void Report();

	uint32_t _interval;
	bool _views;
	frame_stats::Window _frames;
	frame_stats::Window _gpu;
	/// All the frames of the run
	frame_stats::Window _run;
	/// Each frame of the run, for the file
	std::vector<frame_stats::FrameRecord> _records;
	std::filesystem::path _out;
	/// The game turn at the end of the last frame, to know the frames that play a turn
	std::optional<uint32_t> _lastTurn;
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
