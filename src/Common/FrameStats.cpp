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
	// The nearest rank: the smallest time that at least this part of the frames are no longer than
	const auto rank = [&sorted](size_t percent) {
		return sorted.at(std::max<size_t>((sorted.size() * percent + 99) / 100, 1) - 1);
	};
	const auto total = std::accumulate(sorted.begin(), sorted.end(), 0.0);
	return {
	    .count = sorted.size(),
	    .average = static_cast<float>(total / static_cast<double>(sorted.size())),
	    .p50 = rank(50),
	    .p95 = rank(95),
	    .p99 = rank(99),
	    .max = sorted.back(),
	};
}

Summary Window::Take()
{
	const auto summary = Summarise(_times);
	_times.clear();
	return summary;
}

} // namespace openblack::frame_stats
