/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WorldTurn.h"

#include <cmath>

#include <numbers>

#include "Common/X87.h"
#include "ScriptCameraModel.h"

namespace openblack
{

float world_turn::ClampPitch(float pitch)
{
	// The compare with the lowest pitch comes first, so a pitch that is not a number becomes the lowest pitch
	if (!(pitch > k_LowestPitch))
	{
		return k_LowestPitch;
	}
	return pitch < k_HighestPitch ? pitch : k_HighestPitch;
}

float world_turn::Turned(float heading, float turnInput, float screenWidth)
{
	return ((turnInput * std::numbers::pi_v<float>) / screenWidth) + heading;
}

float world_turn::Tilted(float pitch, float tiltInput)
{
	const float tilt = k_TiltPerInput * tiltInput;
	return ClampPitch(pitch - tilt);
}

glm::vec3 world_turn::Unit(glm::vec3 vector)
{
	if (x87::EqualOrUnordered(vector.x, 0.0f) && x87::EqualOrUnordered(vector.y, 0.0f) && x87::EqualOrUnordered(vector.z, 0.0f))
	{
		return vector;
	}
	const float scale = 1.0f / std::sqrt(((vector.x * vector.x) + (vector.z * vector.z)) + (vector.y * vector.y));
	return {vector.x * scale, vector.y * scale, vector.z * scale};
}

glm::vec3 world_turn::AlongSight(glm::vec3 origin, glm::vec3 focus, float distance)
{
	const auto unit = Unit(focus - origin);
	return {origin.x + (unit.x * distance), origin.y + (unit.y * distance), origin.z + (unit.z * distance)};
}

world_turn::View world_turn::TurnAbout(glm::vec3 origin, glm::vec3 focus, glm::vec3 turnPoint, float distance,
                                       float sightDistance, float heading, float pitch, bool zoomed)
{
	const auto before = AlongSight(origin, focus, sightDistance);
	View view {
	    .origin = script_camera::PointFromDistanceHeadingAndPitch(turnPoint, distance, heading, ClampPitch(pitch)),
	    .turnPoint = turnPoint,
	};
	if (!zoomed)
	{
		const auto after = AlongSight(view.origin, view.turnPoint, sightDistance);
		const auto shift = before - after;
		view.turnPoint += shift;
		view.origin += shift;
	}
	return view;
}

} // namespace openblack
