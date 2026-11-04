/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/vec3.hpp>

namespace openblack
{
class CameraModel;
}

/// A script can give the player's camera a point to turn about. While the camera has the point, each step of the camera
/// does these steps:
/// 1. At the start, the point becomes the point that the camera looks at and turns about.
/// 2. The controls turn, tilt, move and zoom the camera as usual.
/// 3. At the end, the camera looks at the point again. It keeps its distance from the point, and it looks back at the
///    point along the heading and the pitch of the step.
/// A step of the fight view that follows the fighters removes the point.
namespace openblack::forced_turn
{

/// Where the camera is and what it looks at
struct View
{
	glm::vec3 origin;
	glm::vec3 focus;
};

/// The distance from the point to the camera, in the order of the game: the squares of the height and depth first, then
/// the square across
[[nodiscard]] float Distance(glm::vec3 point, glm::vec3 origin);

/// The view at the end of a step: the camera keeps its distance from the point and looks at the point along the heading
/// and the pitch, in radians
[[nodiscard]] View AboutPoint(glm::vec3 point, glm::vec3 origin, float heading, float pitch);

/// Whether a script can give the point to this camera: the player's camera, or the fight view, which is a part of the
/// player's camera. A script's camera, the temple's camera and the cameras that follow a creature cannot take it.
[[nodiscard]] bool TakesPoint(const CameraModel& model);

} // namespace openblack::forced_turn
