/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "InputSteps.h"

#include <algorithm>

using namespace openblack::input_steps;

Steps Clock::Advance(std::chrono::microseconds frame)
{
	Steps steps;
	if (frame.count() <= 0)
	{
		return steps;
	}
	const auto counts = static_cast<int64_t>(frame.count()) * static_cast<int64_t>(k_StepsPerSecond);
	const auto before = _owed;
	_owed += counts;
	while (_owed >= k_CountsPerStep - k_EarlyCounts && steps.count < k_MostSteps)
	{
		// The step falls where the count gets to the next whole step, or at the end of the frame when it comes early
		const auto at = static_cast<int64_t>(steps.count + 1) * k_CountsPerStep - before;
		steps.points.at(steps.count) =
		    std::clamp(static_cast<float>(static_cast<double>(at) / static_cast<double>(counts)), 0.0f, 1.0f);
		++steps.count;
		_owed -= k_CountsPerStep;
	}
	// The time of a frame longer than the most steps is not used
	if (_owed >= k_CountsPerStep)
	{
		_owed %= k_CountsPerStep;
	}
	return steps;
}

void Clock::Restart()
{
	_owed = 0;
}
