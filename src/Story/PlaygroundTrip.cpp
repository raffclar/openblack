/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PlaygroundTrip.h"

namespace openblack::playground_trip
{

bool MayAskToGo(const GoSituation& situation) noexcept
{
	return !situation.skirmish && !situation.networkGame && !situation.inTemple && situation.land != k_PlaygroundLand &&
	       !situation.boxUp && !situation.scriptHoldsCinemaBars;
}

void StartLesson(Lessons& lessons) noexcept
{
	lessons.running = true;
}

bool AskEnd(Lessons& lessons) noexcept
{
	if (!lessons.running)
	{
		return false;
	}
	lessons.endAsked = true;
	return true;
}

bool TakeEnd(Lessons& lessons) noexcept
{
	if (!lessons.endAsked)
	{
		return false;
	}
	lessons = {};
	return true;
}

void Leave(Lessons& lessons) noexcept
{
	// The saved game that loads next has no lesson and no end that Escape asked for
	lessons = {};
}

} // namespace openblack::playground_trip
