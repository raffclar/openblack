/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// The testbed moves a hand that it holds still over the land along a sweep of the pointer. It does not use the pick of
/// a drawn frame, so the hand also moves in a fast run that draws no frames.
namespace openblack::testbed::hand_sweep
{

/// The place (x, z) where a ray from the camera through the pointer meets a flat plane at a height. None when the ray
/// does not go to the plane.
[[nodiscard]] constexpr std::optional<glm::vec2> OnPlane(glm::vec3 origin, glm::vec3 direction, float height)
{
	if (direction.y == 0.0f)
	{
		return std::nullopt;
	}
	const float distance = (height - origin.y) / direction.y;
	if (distance < 0.0f)
	{
		return std::nullopt;
	}
	return glm::vec2(origin.x + direction.x * distance, origin.z + direction.z * distance);
}

} // namespace openblack::testbed::hand_sweep
