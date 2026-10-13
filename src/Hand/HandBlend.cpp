/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HandBlend.h"

using namespace openblack;
using namespace openblack::hand_blend;
using Cycle = HandAnimation::Cycle;

HandMode hand_blend::ModeOf(const ModeInputs& inputs)
{
	if (!inputs.shown)
	{
		return HandMode::Hidden;
	}
	if (inputs.insideTemple)
	{
		return HandMode::Temple;
	}
	if (inputs.holds || inputs.pulls)
	{
		if (inputs.holdsSeed)
		{
			return HandMode::Seed;
		}
		return inputs.pulls ? HandMode::Pulling : HandMode::Holding;
	}
	if (inputs.knocks)
	{
		return HandMode::Knocking;
	}
	if (inputs.gripsTotem)
	{
		return HandMode::Totem;
	}
	if (inputs.touchesCreature)
	{
		return HandMode::Creature;
	}
	return inputs.movesCamera ? HandMode::Camera : HandMode::Normal;
}

void ChangeTracker::Enter(HandMode mode)
{
	_mode = mode;
	switch (mode)
	{
	case HandMode::Normal:
		_normalCycle.reset();
		break;
	case HandMode::Camera:
		_cameraCycle = Cycle::Wiggle;
		break;
	case HandMode::Creature:
		_creatureCycle = Cycle::Tickle;
		break;
	default:
		break;
	}
}

Changes ChangeTracker::Step(const Frame& frame)
{
	Changes changes;
	// A thing in the hand that another thing replaces starts a blend
	if (_held.has_value() && frame.held.has_value() && *_held != *frame.held)
	{
		++changes.count;
	}
	_held = frame.held;

	if (!_mode.has_value() || *_mode != frame.mode)
	{
		if (_mode.has_value())
		{
			++changes.count;
		}
		Enter(frame.mode);
	}

	switch (frame.mode)
	{
	case HandMode::Normal:
		// Each change of the animation starts a blend, but not the first animation after the hand enters the mode
		if (_normalCycle.has_value() && *_normalCycle != frame.cycle)
		{
			++changes.count;
		}
		_normalCycle = frame.cycle;
		break;
	case HandMode::Camera:
		if (frame.cycle != _cameraCycle)
		{
			// A grip of the land with nothing below the hand starts the blend two times. Thus the pose of the frame goes
			// in place of the drawn pose. One start puts the pose in place of the kept pose.
			const uint8_t starts = frame.cycle == Cycle::Grip && frame.nothingUnderHand ? 2 : 1;
			changes.count = static_cast<uint8_t>(changes.count + starts);
			changes.poseIntoKept = starts % 2 == 1;
			_cameraCycle = frame.cycle;
		}
		break;
	case HandMode::Creature:
		if (frame.cycle != _creatureCycle)
		{
			// A change to the tickle or to the slap starts a blend. A change to the wiggle does not start a blend. After
			// a slap, the next blend starts only with the next tickle.
			if (frame.cycle == Cycle::Tickle || frame.cycle == Cycle::Slap)
			{
				++changes.count;
				_creatureCycle = frame.cycle;
			}
			else if (frame.cycle == Cycle::Wiggle && _creatureCycle == Cycle::Tickle)
			{
				_creatureCycle = Cycle::Wiggle;
			}
		}
		break;
	default:
		break;
	}
	return changes;
}
