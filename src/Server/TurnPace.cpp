/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TurnPace.h"

#include <algorithm>

namespace openblack::server::turn_pace
{

void Deadline::Restart(Clock::time_point frameStart)
{
	_due = frameStart;
}

Clock::time_point Deadline::NextFrame(Clock::time_point now, Clock::duration frameTime)
{
	// With no start, the frame that ends now was on its due time
	const auto due = _due.value_or(now) + frameTime;
	// The server makes up a part of a long stop only
	_due = std::max(due, now - frameTime * k_MostCatchUpFrames);
	return *_due;
}

} // namespace openblack::server::turn_pace
