/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FramePace.h"

#include <cmath>

#include <utility>

namespace openblack::frame_pace
{

std::chrono::milliseconds Step(Pace& pace, std::chrono::milliseconds frameRealTime, std::chrono::milliseconds frameGameTime)
{
	pace.gameSinceStep += frameGameTime;
	if (!Due(pace, frameRealTime))
	{
		return std::chrono::milliseconds {0};
	}
	return std::exchange(pace.gameSinceStep, std::chrono::milliseconds {0});
}

bool Due(Pace& pace, std::chrono::milliseconds frameRealTime)
{
	pace.sinceStep += frameRealTime;
	if (pace.sinceStep < Pace::k_StepInterval)
	{
		return false;
	}
	// One step a frame at most: a long frame leaves no steps owed, only its share of the next
	pace.sinceStep = std::chrono::duration<float, std::milli>(
	    std::fmod((pace.sinceStep - Pace::k_StepInterval).count(), Pace::k_StepInterval.count()));
	return true;
}

} // namespace openblack::frame_pace
