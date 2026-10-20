/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ClickDistance.h"

#include <cmath>

#include "ScriptCameraModel.h"

namespace openblack::click_distance
{

glm::vec2 RayCoord(int32_t ray)
{
	return {0.5f, static_cast<float>(ray) / static_cast<float>(k_Rays)};
}

float HitDistance(const glm::vec3& origin, const glm::vec3& hit)
{
	// The squares add z first, then y, then x
	const auto d = hit - origin;
	return std::sqrt(((d.z * d.z) + (d.y * d.y)) + (d.x * d.x));
}

float MeanDistance(std::span<const float> hitDistances)
{
	float inverseSum = 1.0f / k_NoHitDistance;
	for (const float distance : hitDistances)
	{
		inverseSum = (1.0f / distance) + inverseSum;
	}
	const auto count = static_cast<float>(hitDistances.size() + 1);
	return 1.0f / (inverseSum / count);
}

float TowardsFocus(float meanDistance, float focusDistance, const glm::vec3& origin, const glm::vec3& focus)
{
	const float share = script_camera::HeadingAndPitchFromPoints(origin, focus).pitch * k_PitchShare;
	float weight = 0.0f;
	if (0.0f < share)
	{
		weight = 1.0f <= share ? 1.0f : share;
	}
	return ((focusDistance - meanDistance) * weight) + meanDistance;
}

} // namespace openblack::click_distance
