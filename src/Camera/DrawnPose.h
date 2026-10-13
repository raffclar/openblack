/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

/// The eye and the focus that the world camera draws from. These can be different from where the camera is: the frame
/// does not draw from an eye on its focus or below the land.
namespace openblack::camera_drawn
{

struct Pose
{
	glm::vec3 eye;
	glm::vec3 focus;
};

/// An eye nearer to its focus than this square of the distance moves away from the focus
inline constexpr float k_SmallestGapSquared = 0.001f;
/// How the eye moves away from a focus that is too near
inline constexpr glm::vec3 k_GapStep {-1.0f, 1.0f, 0.0f};
/// The eye stays this height above the land below it
inline constexpr float k_HeightAboveLand = 1.0f;

/// The eye moves away from a focus that is almost at the same point, so that the view has a direction
[[nodiscard]] inline Pose Separate(Pose pose)
{
	const auto gap = pose.eye - pose.focus;
	if (glm::dot(gap, gap) < k_SmallestGapSquared)
	{
		pose.eye += k_GapStep;
	}
	return pose;
}

/// When the eye is lower than the height above the land, the eye moves vertically to that height. The focus moves the same
/// distance, thus the view keeps its direction. `land` is the height of the land below the eye.
[[nodiscard]] inline Pose AboveLand(Pose pose, float land)
{
	const float lift = land + k_HeightAboveLand - pose.eye.y;
	if (lift > 0.0f)
	{
		pose.eye.y += lift;
		pose.focus.y += lift;
	}
	return pose;
}

} // namespace openblack::camera_drawn
