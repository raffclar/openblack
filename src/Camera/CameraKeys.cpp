/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CameraKeys.h"

using namespace openblack;

float camera_keys::FrameSeconds(int32_t milliseconds)
{
	return static_cast<float>(milliseconds) * k_SecondsPerMillisecond;
}

bool camera_keys::ArrowsRotate(const Held& held)
{
	return !held.zoom && (held.fight || held.rotate);
}

camera_keys::Steps camera_keys::Step(const Held& held, float seconds)
{
	Steps steps;
	const float step = seconds * k_StepPerSecond;
	const float negativeStep = seconds * -k_StepPerSecond;
	const bool rotate = ArrowsRotate(held);
	// The turn and tilt keys come first
	if (held.turnRight)
	{
		steps.turn -= negativeStep;
	}
	if (held.turnLeft)
	{
		steps.turn -= step;
	}
	if (held.tiltDown)
	{
		steps.tilt -= step;
	}
	if (held.tiltUp)
	{
		steps.tilt = step + steps.tilt;
	}
	// Then the arrow keys, which tilt, zoom or move the camera
	if (held.forwards)
	{
		(rotate ? steps.tilt : held.zoom ? steps.zoom : steps.forwards) -= step;
	}
	if (held.backwards)
	{
		auto& value = rotate ? steps.tilt : held.zoom ? steps.zoom : steps.forwards;
		value = step + value;
	}
	// The left and right keys turn the camera when the zoom key or the rotate key is held
	if (held.left)
	{
		if (rotate || held.zoom)
		{
			steps.turn -= negativeStep;
		}
		else
		{
			steps.strafe -= step;
		}
	}
	if (held.right)
	{
		if (rotate || held.zoom)
		{
			steps.turn -= step;
		}
		else
		{
			steps.strafe = step + steps.strafe;
		}
	}
	return steps;
}
