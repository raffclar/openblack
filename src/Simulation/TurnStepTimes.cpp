/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TurnStepTimes.h"

#include <algorithm>
#include <numeric>
#include <vector>

#include <fmt/format.h>

using namespace openblack::simulation;

void TurnStepTimes::Add(game_turn::Step step, float milliseconds)
{
	const auto index = static_cast<size_t>(step);
	if (index < _milliseconds.size())
	{
		_milliseconds[index] += milliseconds;
	}
}

float TurnStepTimes::Total() const
{
	return std::accumulate(_milliseconds.begin(), _milliseconds.end(), 0.0f);
}

std::string TurnStepTimes::Report(float least) const
{
	std::vector<size_t> steps;
	for (size_t step = 0; step < _milliseconds.size(); ++step)
	{
		if (_milliseconds[step] > 0.0f && _milliseconds[step] >= least)
		{
			steps.push_back(step);
		}
	}
	// The longest step first. Steps with the same time keep the order of their numbers.
	std::ranges::stable_sort(steps, [this](size_t a, size_t b) { return _milliseconds[a] > _milliseconds[b]; });
	std::string report;
	for (const auto step : steps)
	{
		report += fmt::format("{}{}={:.2f}", report.empty() ? "" : " ", step, _milliseconds[step]);
	}
	return report;
}
