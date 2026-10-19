/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs::components
{

/// The angles that a placed thing has, in radians about the x, y and z axes. The game keeps the angles as it gets them
/// and the scripts read them back with no change. A thing that does not lean keeps only the angle about y.
struct TurnAngles
{
	glm::vec3 angles {0.0f};
	/// The turn of the thing that these angles made. When a different turn replaces it, the angles are no longer correct.
	glm::mat3 turn {1.0f};
};

} // namespace openblack::ecs::components
