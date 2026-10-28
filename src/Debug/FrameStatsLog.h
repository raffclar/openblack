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

	/// How long one frame took
	struct FrameTime
	{
		/// The time from the end of the last frame to the end of this frame. In a web browser, this time includes the
		/// wait for the display frame of the browser.
		float milliseconds {0.0f};
		/// The time that the game thread did the work of this frame, without a wait for the display frame or for the
		/// pace of a simulation run
		float workMilliseconds {0.0f};
		/// The frame played one or more game turns
		bool playedTurn {false};
		/// The game turn at the end of the frame
		uint32_t turn {0};
	};

	/// After each frame, with how long the frame took, the profiler that timed its stages and how many pipelines the
	/// renderer made since it started
	void Frame(FrameTime time, const openblack::Profiler& profiler, uint32_t pipelinesMade);

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
	frame_stats::Window _work;
	frame_stats::Window _turnWork;
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
	std::optional<double> _lastCpuMilliseconds;
};

} // namespace openblack::debug
