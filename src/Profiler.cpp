/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Profiler.h"

#include <cassert>
#include <cstdlib>

#include <algorithm>
#include <vector>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "Debug/MemoryStats.h"

namespace
{
/// A SetDirty caller for the log: the file's name and the line, or the operation and the component's own name
std::string CallerName(std::string_view where, std::string_view what, uint32_t line)
{
	if (line != 0)
	{
		const auto slash = where.find_last_of("/\\");
		return fmt::format("{}:{}", slash == std::string_view::npos ? where : where.substr(slash + 1), line);
	}
	if (what.empty())
	{
		return std::string(where);
	}
	const auto colons = what.rfind("::");
	return fmt::format("{}<{}>", where, colons == std::string_view::npos ? what : what.substr(colons + 2));
}
} // namespace

void openblack::Profiler::Begin(Stage stage)
{
	assert(_currentLevel < 255);
	auto& entry = _entries.at(_currentEntry).stages.at(static_cast<uint8_t>(stage));
	entry.level = _currentLevel;
	_currentLevel++;
	entry.start = std::chrono::system_clock::now();
	entry.finalized = false;
}

void openblack::Profiler::End(Stage stage)
{
	assert(_currentLevel > 0);
	auto& entry = _entries.at(_currentEntry).stages.at(static_cast<uint8_t>(stage));
	assert(!entry.finalized);
	_currentLevel--;
	assert(entry.level == _currentLevel);
	entry.end = std::chrono::system_clock::now();
	entry.finalized = true;
}

void openblack::Profiler::Frame()
{
	auto& prevEntry = _entries.at(_currentEntry);
	_currentEntry = (_currentEntry + 1) % k_BufferSize;
	prevEntry.frameEnd = _entries.at(_currentEntry).frameStart = std::chrono::system_clock::now();
	if (_summaryInterval > 0.0f && prevEntry.frameStart.time_since_epoch().count() != 0)
	{
		Accumulate(prevEntry);
	}
}

void openblack::Profiler::Accumulate(const Entry& entry)
{
	using Ms = std::chrono::duration<double, std::milli>;
	if (_summaryStart.time_since_epoch().count() == 0)
	{
		_summaryStart = entry.frameStart;
	}
	const double frame = Ms(entry.frameEnd - entry.frameStart).count();
	_framesTotal += frame;
	_framesWorst = std::max(_framesWorst, frame);
	++_frames;
	for (size_t i = 0; i < _totals.size(); ++i)
	{
		// The entries are a ring buffer: a stage that did not run this frame still holds an old time.
		const auto& stage = entry.stages.at(i);
		if (!stage.finalized || stage.start < entry.frameStart || stage.end > entry.frameEnd)
		{
			continue;
		}
		const double ms = Ms(stage.end - stage.start).count();
		_totals.at(i).total += ms;
		_totals.at(i).worst = std::max(_totals.at(i).worst, ms);
		++_totals.at(i).runs;
	}

	const double elapsed = Ms(entry.frameEnd - _summaryStart).count() / 1000.0;
	if (elapsed < _summaryInterval || _frames == 0)
	{
		return;
	}
	std::string text = fmt::format("Profile over {:.1f} s: {} frames, {:.1f} fps, frame avg {:.3f} ms, worst {:.3f} ms",
	                               elapsed, _frames, _frames / elapsed, _framesTotal / _frames, _framesWorst);
	for (size_t i = 0; i < _totals.size(); ++i)
	{
		const auto& totals = _totals.at(i);
		if (totals.runs == 0)
		{
			continue;
		}
		// avg per frame (the share of the frame budget), how often it ran, and its worst single run
		text +=
		    fmt::format("\n  {:<22} {:8.3f} ms/frame {:5.1f}%  ran {:5} x  worst {:8.3f} ms", k_StageNames.at(i),
		                totals.total / _frames, 100.0 * totals.total / std::max(_framesTotal, 1e-9), totals.runs, totals.worst);
	}
	AppendCounters(text);
	// the memory at the end of the interval (Debug/MemoryStats.h)
	text += "\n  " + memory_stats::Format(memory_stats::Take(true));
	if (auto logger = spdlog::get("game"); logger)
	{
		SPDLOG_LOGGER_INFO(logger, "{}", text);
	}
	_totals = {};
	_framesTotal = 0.0;
	_framesWorst = 0.0;
	_frames = 0;
	_summaryStart = entry.frameEnd;
	_counters = {};
	_dirtyCallers.clear();
}

void openblack::Profiler::CountDirty(std::string_view where, std::string_view what, uint32_t line)
{
	if (!Counting())
	{
		return;
	}
	const DirtyCaller caller {where, what, line};
	++_counters.at(static_cast<uint8_t>(Counter::SetDirtyCalls));
	++_dirtyCallers[caller].calls;
	if (!_firstDirty.has_value())
	{
		_firstDirty = caller;
	}
}

void openblack::Profiler::CountDrawRebuild(bool dirty, uint64_t rows, uint64_t uploadBytes)
{
	if (!Counting())
	{
		return;
	}
	++_counters.at(static_cast<uint8_t>(Counter::DrawRebuilds));
	_counters.at(static_cast<uint8_t>(Counter::DrawRows)) += rows;
	_counters.at(static_cast<uint8_t>(Counter::DrawUploadBytes)) += uploadBytes;
	if (dirty)
	{
		++_counters.at(static_cast<uint8_t>(Counter::DrawRebuildsDirty));
		// none counted before it: the first build (RenderContext::dirty starts true)
		++_dirtyCallers[_firstDirty.value_or(DirtyCaller {"(the first build)", {}, 0})].rebuilds;
	}
	_firstDirty.reset();
}

void openblack::Profiler::CountDrawRefill(uint64_t rows, uint64_t uploadBytes)
{
	if (!Counting())
	{
		return;
	}
	++_counters.at(static_cast<uint8_t>(Counter::DrawRefills));
	_counters.at(static_cast<uint8_t>(Counter::DrawRefillRows)) += rows;
	_counters.at(static_cast<uint8_t>(Counter::DrawRefillBytes)) += uploadBytes;
	// the SetDirty calls before it were refills' reasons, not a rebuild's
	_firstDirty.reset();
}

void openblack::Profiler::AppendCounters(std::string& text) const
{
	const auto count = [this](Counter counter) { return _counters.at(static_cast<uint8_t>(counter)); };
	const uint64_t rebuilds = count(Counter::DrawRebuilds);
	const uint64_t calls = count(Counter::SetDirtyCalls);
	const uint64_t refills = count(Counter::DrawRefills);
	if (rebuilds == 0 && calls == 0 && refills == 0)
	{
		return;
	}
	const uint64_t dirty = count(Counter::DrawRebuildsDirty);
	const auto rows = static_cast<double>(count(Counter::DrawRows));
	const auto bytes = static_cast<double>(count(Counter::DrawUploadBytes));
	const double frames = std::max(_frames, 1u);
	const double perRebuild = rows / static_cast<double>(std::max<uint64_t>(rebuilds, 1));
	text += fmt::format("\n  Draw rebuilds {} ({} after a SetDirty, {} a debug view), rows {:.0f} ({:.1f} a "
	                    "rebuild), upload {:.2f} MB ({:.1f} KB a frame)",
	                    rebuilds, dirty, rebuilds - dirty, rows, perRebuild, bytes / 1048576.0, bytes / 1024.0 / frames);
	text += fmt::format("\n  Draw refills {} (the instances written again into their ranges), rows {:.0f}, upload {:.2f} MB",
	                    refills, static_cast<double>(count(Counter::DrawRefillRows)),
	                    static_cast<double>(count(Counter::DrawRefillBytes)) / 1048576.0);
	text += fmt::format("\n  SetDirty {} calls ({:.1f} a frame), by caller: calls, rebuilds it was the first of", calls,
	                    static_cast<double>(calls) / frames);
	// the same caller from two translation units (a header's template) is one line
	std::map<std::string, DirtyCount> merged;
	for (const auto& [caller, counts] : _dirtyCallers)
	{
		auto& line = merged[CallerName(caller.where, caller.what, caller.line)];
		line.calls += counts.calls;
		line.rebuilds += counts.rebuilds;
	}
	std::vector<std::pair<std::string, DirtyCount>> sorted(merged.begin(), merged.end());
	std::stable_sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
		if (a.second.calls != b.second.calls)
		{
			return a.second.calls > b.second.calls;
		}
		return a.second.rebuilds > b.second.rebuilds;
	});
	constexpr size_t k_Callers = 24;
	DirtyCount rest;
	for (size_t i = 0; i < sorted.size(); ++i)
	{
		const auto& [name, counts] = sorted[i];
		if (i < k_Callers)
		{
			text += fmt::format("\n    {:<44} {:8} {:6}", name, counts.calls, counts.rebuilds);
			continue;
		}
		rest.calls += counts.calls;
		rest.rebuilds += counts.rebuilds;
	}
	if (sorted.size() > k_Callers)
	{
		text +=
		    fmt::format("\n    ({} more callers) {:25} {:8} {:6}", sorted.size() - k_Callers, "", rest.calls, rest.rebuilds);
	}
}
