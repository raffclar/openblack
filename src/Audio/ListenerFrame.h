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

namespace openblack::audio
{

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
