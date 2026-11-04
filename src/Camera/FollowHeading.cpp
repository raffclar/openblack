/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FollowHeading.h"

#include <algorithm>
#include <array>

#include "ScriptCameraModel.h"

namespace openblack::follow_heading
{

bool TurnsTooFast(uint32_t planAction)
{
	constexpr std::array<uint32_t, 6> k_TurningActions {8, 82, 88, 89, 90, 92};
	return std::ranges::find(k_TurningActions, planAction) != k_TurningActions.end();
}

float GlideSeconds(float secondsSinceStart)
{
	if (secondsSinceStart <= k_BlendSeconds)
	{
		return (secondsSinceStart / k_BlendSeconds) * (k_EndSeconds - k_StartSeconds) + k_StartSeconds;
	}
	return k_EndSeconds;
}

View Step(Settings& settings, const Thing& thing, float secondsSinceStart, float& heldHeading)
{
	// The added heading turns by its speed, and comes back by a full turn when it goes past a full turn
	settings.heading = settings.headingSpeed + settings.heading;
	if (settings.heading > k_FullTurn)
	{
		settings.heading = settings.heading - k_FullTurn;
	}
	float facing = thing.facing;
	if (facing > k_FullTurn)
	{
		facing = facing - k_FullTurn;
	}
	if (!thing.turnsTooFast)
	{
		float turn = facing + settings.heading;
		if (turn > k_FullTurn)
		{
			turn = turn - k_FullTurn;
		}
		heldHeading = k_FullTurn - turn;
	}
	const float pitch = settings.pitch <= k_LowestPitch ? k_LowestPitch : settings.pitch;
	return {
	    .origin = script_camera::PointFromDistanceHeadingAndPitch(thing.point, settings.distance, heldHeading, pitch),
	    .focus = thing.point,
	    .seconds = GlideSeconds(secondsSinceStart),
	};
}

} // namespace openblack::follow_heading
