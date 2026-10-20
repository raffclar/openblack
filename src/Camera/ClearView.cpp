/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ClearView.h"

#include <cmath>

#include "ScriptCameraModel.h"

using namespace openblack;

float clear_view::OrbitRadius(float distance)
{
	if (distance > k_FarDistance)
	{
		return k_FarRadius;
	}
	if (distance < k_NearDistance)
	{
		return k_NearRadius;
	}
	return k_MiddleRadius;
}

float clear_view::OrbitPitch(float pitch)
{
	return (pitch + k_PitchShift) * k_PitchShare;
}

clear_view::Orbit clear_view::Start(const glm::vec3& origin, const glm::vec3& focus)
{
	const auto look = script_camera::HeadingAndPitchFromPoints(origin, focus);
	// The squares are added in the order z, y, x
	const glm::vec3 offset = origin - focus;
	const float squares = offset.z * offset.z + offset.y * offset.y;
	const float distance = std::sqrt(squares + offset.x * offset.x);
	return {.startOrigin = origin,
	        .startFocus = focus,
	        .radius = OrbitRadius(distance),
	        .heading = look.heading,
	        .pitch = OrbitPitch(look.pitch)};
}

clear_view::View clear_view::Eased(const Orbit& orbit, const glm::vec3& centre, float share)
{
	const auto origin = script_camera::PointFromDistanceHeadingAndPitch(centre, orbit.radius, orbit.heading, orbit.pitch);
	return {.origin = (origin - orbit.startOrigin) * share + orbit.startOrigin,
	        .focus = (centre - orbit.startFocus) * share + orbit.startFocus};
}
