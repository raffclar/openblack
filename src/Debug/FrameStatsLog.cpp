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
#include <fstream>
#include <optional>
#include <utility>
#include <vector>

#include <bgfx/bgfx.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <intrin.h>
#include <windows.h>
// psapi.h needs windows.h first
#include <psapi.h>
#else
#include <ctime>

#include <unistd.h>
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
/// The time that the game thread was on a processor. A web browser does not give this time: its thread clock is the
/// wall clock, which also counts the wait for the display frame.
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

/// How much memory the process holds, in megabytes: in physical memory, and of its own (private) where the system
/// tells it
struct ProcessMemory
{
	double resident {0.0};
	std::optional<double> own;
};

constexpr double k_BytesPerMegabyte = 1024.0 * 1024.0;

std::optional<ProcessMemory> MemoryOfProcess()
{
#ifdef _WIN32
	PROCESS_MEMORY_COUNTERS_EX counters {};
	if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters)) == 0)
	{
		return std::nullopt;
	}
	return ProcessMemory {.resident = static_cast<double>(counters.WorkingSetSize) / k_BytesPerMegabyte,
	                      .own = static_cast<double>(counters.PrivateUsage) / k_BytesPerMegabyte};
#elif defined(__linux__)
	// The second number of the file is the pages in physical memory
	std::ifstream statm("/proc/self/statm");
	long pages = 0;
	long resident = 0;
	if (!(statm >> pages >> resident))
	{
		return std::nullopt;
	}
	return ProcessMemory {.resident =
	                          static_cast<double>(resident) * static_cast<double>(sysconf(_SC_PAGESIZE)) / k_BytesPerMegabyte,
	                      .own = std::nullopt};
#else
	return std::nullopt;
#endif
}

/// A graphics memory amount that the renderer may not know, which it then gives as a negative number
std::string Megabytes(int64_t bytes)
{
	return bytes < 0 ? std::string("unknown") : fmt::format("{:.0f} MB", static_cast<double>(bytes) / k_BytesPerMegabyte);
}

} // namespace

namespace openblack::debug
{

FrameStatsLog::FrameStatsLog(uint32_t interval, bool views, uint32_t pipelinesMade, std::filesystem::path out)
    : _interval(std::max<uint32_t>(interval, 1))
    , _views(views)
    , _lastPipelinesMade(pipelinesMade)
    , _lastCpuMilliseconds(ThreadCpuMilliseconds())
{
	_out = std::move(out);
	if (!_out.empty())
	{
		_records.reserve(100000);
	}
}

FrameStatsLog::~FrameStatsLog()
{
	const auto run = _run.Peek();
	if (auto logger = spdlog::get("game"); logger && run.count > 0)
	{
		SPDLOG_LOGGER_INFO(
		    logger,
		    "Frame stats of the run: {} frames, avg {:.2f} ms p95 {:.2f} p99 {:.2f} max {:.2f}; {} frames of {:.0f} ms or more",
		    run.count, run.average, run.p95, run.p99, run.max, run.slow, k_SpikeMilliseconds);
	}
	if (!_out.empty())
	{
		std::ofstream file(_out, std::ios::binary | std::ios::trunc);
		const auto text = frame_stats::ToCsv(_records);
		file.write(text.data(), static_cast<std::streamsize>(text.size()));
	}
}

void FrameStatsLog::Frame(FrameTime time, const openblack::Profiler& profiler, uint32_t pipelinesMade)
{
	const auto milliseconds = time.milliseconds;
	_frames.Add(milliseconds);
	_run.Add(milliseconds);
	_work.Add(time.workMilliseconds);
	const bool turnFrame = time.playedTurn;
	if (turnFrame)
	{
		_turnWork.Add(time.workMilliseconds);
	}
	++_frameNumber;
	const auto cpuMilliseconds = ThreadCpuMilliseconds();
	const auto lastCpuMilliseconds = std::exchange(_lastCpuMilliseconds, cpuMilliseconds);
	const auto onCpuMilliseconds = cpuMilliseconds.has_value() && lastCpuMilliseconds.has_value()
	                                   ? std::optional<double>(*cpuMilliseconds - *lastCpuMilliseconds)
	                                   : std::nullopt;
	const auto onCpu = onCpuMilliseconds.has_value() ? fmt::format("{:.2f} ms", *onCpuMilliseconds) : std::string("unknown");
	if (!_out.empty())
	{
		_records.push_back({.milliseconds = milliseconds,
		                    .processorMilliseconds = static_cast<float>(onCpuMilliseconds.value_or(0.0)),
		                    .turn = time.turn,
		                    .played = turnFrame,
		                    .workMilliseconds = time.workMilliseconds});
	}
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
	const auto newPipelines = pipelinesMade - std::exchange(_lastPipelinesMade, pipelinesMade);
	_pipelinesSinceReport += newPipelines;
	if (milliseconds >= k_SpikeMilliseconds)
	{
		// A slow frame: which stages took its time, and what the renderer waited for
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "Frame spike: frame {} took {:.2f} ms, {} of it on the processor, {:.2f} ms of work; turn {}{}; "
		                   "render submit {:.2f} wait {:.2f}; {} draws; {} new resources; {} new pipelines; stages (ms):{}",
		                   _frameNumber, milliseconds, onCpu, time.workMilliseconds, time.turn, turnFrame ? " played" : "",
		                   static_cast<double>(stats->cpuTimeEnd - stats->cpuTimeBegin) * toMsCpu,
		                   static_cast<double>(stats->waitRender) * toMsCpu, stats->numDraw, newResources, newPipelines,
		                   spikeStages);
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
	const auto work = _work.Take();
	const auto turnWork = _turnWork.Take();
	auto logger = spdlog::get("game");
	++_reports;
	SPDLOG_LOGGER_INFO(
	    logger,
	    "Frame stats #{}: {} frames, frame avg {:.2f} ms p95 {:.2f} max {:.2f} ({:.1f} FPS); GPU avg {:.2f} "
	    "p95 {:.2f}; render submit {:.2f} wait {:.2f}; {:.0f} draws; {} new pipelines; work avg {:.2f} p95 {:.2f} "
	    "max {:.2f}; {} frames with a turn: work avg {:.2f} max {:.2f}; frame p50 {:.2f} p99 {:.2f}",
	    _reports, frames.count, frames.average, frames.p95, frames.max, 1000.0f / frames.average, gpu.average, gpu.p95,
	    _submitTotal / count, _waitRenderTotal / count, static_cast<double>(_drawCallTotal) / count,
	    std::exchange(_pipelinesSinceReport, 0u), work.average, work.p95, work.max, turnWork.count, turnWork.average,
	    turnWork.max, frames.p50, frames.p99);

	// How much memory the game holds, and the graphics memory that the renderer knows of
	const auto* stats = bgfx::getStats();
	const auto memory = MemoryOfProcess();
	SPDLOG_LOGGER_INFO(logger, "Frame stats #{} memory: process {}{}; textures {}, render targets {}, graphics card {} of {}",
	                   _reports, memory.has_value() ? fmt::format("{:.0f} MB", memory->resident) : "unknown",
	                   memory.has_value() && memory->own.has_value() ? fmt::format(" ({:.0f} MB its own)", *memory->own) : "",
	                   Megabytes(stats->textureMemoryUsed), Megabytes(stats->rtMemoryUsed), Megabytes(stats->gpuMemoryUsed),
	                   Megabytes(stats->gpuMemoryMax));

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
