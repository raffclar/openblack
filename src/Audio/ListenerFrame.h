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
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

namespace openblack::audio
{

/// Where the sounds are heard from: an eye, the way it looks and the way up from it
struct Listener
{
	glm::vec3 origin {0.0f};
	glm::vec3 forward {0.0f, 0.0f, 1.0f};
	glm::vec3 up {0.0f, 1.0f, 0.0f};
};

/// A listener at an eye looking at a point, upright, as the camera looks from its origin at its focus
[[nodiscard]] inline Listener ListenerLookingAt(const glm::vec3& origin, const glm::vec3& focus)
{
	const auto rotation = glm::transpose(glm::mat3(glm::lookAt(origin, focus, glm::vec3(0.0f, 1.0f, 0.0f))));
	return {.origin = origin, .forward = rotation * glm::vec3(0.0f, 0.0f, 1.0f), .up = rotation * glm::vec3(0.0f, 1.0f, 0.0f)};
}

/// Where a point in the world is heard from by a listener at an eye looking along forward with up above: x to the
/// listener's right, as the screen shows it, y ahead and z up. The world is left-handed, so the right is up crossed
/// with forward.
[[nodiscard]] inline glm::vec3 ToListenerFrame(const glm::vec3& point, const glm::vec3& eye, const glm::vec3& forward,
                                               const glm::vec3& up)
{
	const auto ahead = glm::normalize(forward);
	const auto above = glm::normalize(up);
	const auto right = glm::normalize(glm::cross(above, ahead));
	const auto offset = point - eye;
	return {glm::dot(offset, right), glm::dot(offset, ahead), glm::dot(offset, above)};
}

} // namespace openblack::audio
