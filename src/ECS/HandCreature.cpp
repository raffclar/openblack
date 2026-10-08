/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HandCreature.h"

using namespace openblack::ecs;

hand_creature::LockStep hand_creature::StepLock(const LockState& state, const LockFacts& facts)
{
	LockStep step {.next = {.phase = state.phase, .released = state.released || facts.actionReleased}};
	if (!facts.available)
	{
		// a creature that is no longer there ends the locked select, unless its end is sent already
		step.sendEnd = state.phase != LockPhase::Completion;
		step.letGo = true;
		return step;
	}
	switch (state.phase)
	{
	case LockPhase::Waiting:
		// the start packet locked it: held. One the turn refused has had its end sent already
		if (facts.locked)
		{
			step.next.phase = LockPhase::Held;
		}
		else if (!facts.stillSelected)
		{
			step.letGo = true;
		}
		break;
	case LockPhase::Held:
		if (!facts.locked)
		{
			step.letGo = true;
		}
		else if (step.next.released)
		{
			step.next.phase = LockPhase::WaitingForLockOff;
		}
		break;
	case LockPhase::WaitingForLockOff:
		// (approximate) the original waits for the creature to be ready to be let go; it is taken as ready one frame
		// after the release
		step.sendEnd = true;
		step.next.phase = LockPhase::Completion;
		break;
	case LockPhase::Completion:
		// once the end packet has been applied
		if (!facts.locked)
		{
			step.letGo = true;
		}
		break;
	}
	return step;
}
