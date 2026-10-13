/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FrameStatsLog.h"

#include <algorithm>
#include <chrono>
#include <utility>
#include <vector>

#include <bgfx/bgfx.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <intrin.h>
#include <windows.h>
#else
#include <ctime>
#endif

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

/// The time that this thread was on a processor since it started, in milliseconds. A frame that takes much more time
/// than this waited: for the renderer, or for the system, which gave the processor to other programs.
double ThreadCpuMilliseconds()
{
#ifdef _WIN32
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

} // namespace

namespace openblack::debug
{

FrameStatsLog::FrameStatsLog(uint32_t interval, bool views)
    : _interval(std::max<uint32_t>(interval, 1))
    , _views(views)
    , _lastCpuMilliseconds(ThreadCpuMilliseconds())
{
}

void FrameStatsLog::Frame(float milliseconds, const openblack::Profiler& profiler)
{
	_frames.Add(milliseconds);
	++_frameNumber;
	const auto cpuMilliseconds = ThreadCpuMilliseconds();
	const auto onCpu = cpuMilliseconds - std::exchange(_lastCpuMilliseconds, cpuMilliseconds);
	std::string spikeStages;

	// The stages timed in this frame: those that started since the frame did
	const auto& entry = profiler.GetEntries().at(profiler.GetEntryIndex(0));
	for (size_t i = 0; i < entry.stages.size(); ++i)
	{
		const auto& stage = entry.stages.at(i);
		if (stage.finalized && stage.start >= entry.frameStart)
		{
			const auto ms = std::chrono::duration<double, std::milli>(stage.end - stage.start).count();
			_stageTotals.at(i) += ms;
			_stageMax.at(i) = std::max(_stageMax.at(i), ms);
			if (milliseconds >= k_SpikeMilliseconds && ms >= 0.25)
			{
				spikeStages += fmt::format(" {}={:.2f}", openblack::Profiler::k_StageNames.at(i), ms);
			}
		}
	}

	const auto* stats = bgfx::getStats();
	const double toMsCpu = 1000.0 / static_cast<double>(stats->cpuTimerFreq);
	const double toMsGpu = 1000.0 / static_cast<double>(stats->gpuTimerFreq);
	_gpu.Add(static_cast<float>(static_cast<double>(stats->gpuTimeEnd - stats->gpuTimeBegin) * toMsGpu));
	_submitTotal += static_cast<double>(stats->cpuTimeEnd - stats->cpuTimeBegin) * toMsCpu;
	_waitRenderTotal += static_cast<double>(stats->waitRender) * toMsCpu;
	_drawCallTotal += stats->numDraw;
	const auto made = static_cast<int64_t>(stats->numTextures) + stats->numVertexBuffers + stats->numIndexBuffers +
	                  stats->numDynamicVertexBuffers + stats->numDynamicIndexBuffers + stats->numPrograms;
	const auto newResources = made - _lastResourceCount;
	_lastResourceCount = made;
	if (milliseconds >= k_SpikeMilliseconds)
	{
		// A slow frame: which stages took its time, and what the renderer waited for
		SPDLOG_LOGGER_INFO(
		    spdlog::get("game"),
		    "Frame spike: frame {} took {:.2f} ms, {:.2f} ms of it on the processor; render submit {:.2f} wait {:.2f}; "
		    "{} draws; {} new resources; stages (ms):{}",
		    _frameNumber, milliseconds, onCpu, static_cast<double>(stats->cpuTimeEnd - stats->cpuTimeBegin) * toMsCpu,
		    static_cast<double>(stats->waitRender) * toMsCpu, stats->numDraw, newResources, spikeStages);
	}
	if (_views)
	{
		for (uint16_t i = 0; i < stats->numViews; ++i)
		{
			const auto& view = stats->viewStats[i];
			_viewTotals[view.name] += static_cast<double>(view.gpuTimeEnd - view.gpuTimeBegin) * toMsGpu;
		}
	}

	if (_frames.Count() >= _interval)
	{
		Report();
	}
}

void FrameStatsLog::Report()
{
	const auto count = static_cast<double>(_frames.Count());
	const auto frames = _frames.Take();
	const auto gpu = _gpu.Take();
	auto logger = spdlog::get("game");
	++_reports;
	SPDLOG_LOGGER_INFO(logger,
	                   "Frame stats #{}: {} frames, frame avg {:.2f} ms p95 {:.2f} max {:.2f} ({:.1f} FPS); GPU avg {:.2f} "
	                   "p95 {:.2f}; render submit {:.2f} wait {:.2f}; {:.0f} draws",
	                   _reports, frames.count, frames.average, frames.p95, frames.max, 1000.0f / frames.average, gpu.average,
	                   gpu.p95, _submitTotal / count, _waitRenderTotal / count, static_cast<double>(_drawCallTotal) / count);

	// The stages that took the longest, nested ones among them
	std::vector<std::pair<double, size_t>> stages;
	for (size_t i = 0; i < _stageTotals.size(); ++i)
	{
		if (_stageTotals.at(i) > 0.0)
		{
			stages.emplace_back(_stageTotals.at(i) / count, i);
		}
	}
	std::ranges::sort(stages, std::greater {});
	std::string line;
	for (const auto& [ms, i] : stages)
	{
		if (ms >= 0.05)
		{
			line += fmt::format(" {}={:.2f}", openblack::Profiler::k_StageNames.at(i), ms);
		}
	}
	SPDLOG_LOGGER_INFO(logger, "Frame stats #{} stages (ms):{}", _reports, line);

	// The stages behind the slowest frames
	std::vector<std::pair<double, size_t>> slowest;
	for (size_t i = 0; i < _stageMax.size(); ++i)
	{
		if (_stageMax.at(i) >= 1.0)
		{
			slowest.emplace_back(_stageMax.at(i), i);
		}
	}
	std::ranges::sort(slowest, std::greater {});
	line.clear();
	for (const auto& [ms, i] : slowest)
	{
		line += fmt::format(" {}={:.2f}", openblack::Profiler::k_StageNames.at(i), ms);
	}
	SPDLOG_LOGGER_INFO(logger, "Frame stats #{} slowest stages (ms):{}", _reports, line);

	if (_views)
	{
		std::vector<std::pair<double, std::string>> views;
		for (const auto& [name, total] : _viewTotals)
		{
			views.emplace_back(total / count, name);
		}
		std::ranges::sort(views, std::greater {});
		line.clear();
		for (const auto& [ms, name] : views)
		{
			if (ms >= 0.02)
			{
				line += fmt::format(" [{}]={:.2f}", name, ms);
			}
		}
		SPDLOG_LOGGER_INFO(logger, "Frame stats #{} GPU views (ms):{}", _reports, line);
	}

	_stageTotals.fill(0.0);
	_stageMax.fill(0.0);
	_viewTotals.clear();
	_submitTotal = 0.0;
	_waitRenderTotal = 0.0;
	_drawCallTotal = 0;
}

} // namespace openblack::debug
