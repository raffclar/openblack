/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FrameStats.h"

#include <algorithm>
#include <numeric>

#include <fmt/format.h>

namespace openblack::frame_stats
{

Summary Summarise(std::span<const float> milliseconds)
{
	if (milliseconds.empty())
	{
		return {};
	}
	std::vector<float> sorted(milliseconds.begin(), milliseconds.end());
	std::ranges::sort(sorted);
	// The nearest rank: the smallest time that at least the given part of the frames are no longer than
	const auto rank = [&sorted](size_t percent) {
		return sorted.at(std::max<size_t>((sorted.size() * percent + 99) / 100, 1) - 1);
	};
	const auto total = std::accumulate(sorted.begin(), sorted.end(), 0.0);
	return {
	    .count = sorted.size(),
	    .average = static_cast<float>(total / static_cast<double>(sorted.size())),
	    .p95 = rank(95),
	    .p99 = rank(99),
	    .max = sorted.back(),
	    .slow = static_cast<size_t>(std::ranges::distance(std::ranges::lower_bound(sorted, k_SlowMilliseconds), sorted.end())),
	};
}

std::string ToCsv(std::span<const FrameRecord> frames)
{
	std::string text = "frame,ms,processor_ms,turn,played\n";
	for (size_t i = 0; i < frames.size(); ++i)
	{
		const auto& frame = frames[i];
		text += fmt::format("{},{:.3f},{:.3f},{},{}\n", i + 1, frame.milliseconds, frame.processorMilliseconds, frame.turn,
		                    frame.played ? 1 : 0);
	}
	return text;
}

Summary Window::Peek() const
{
	return Summarise(_times);
}

Summary Window::Take()
{
	const auto summary = Summarise(_times);
	_times.clear();
	return summary;
}

} // namespace openblack::frame_stats
