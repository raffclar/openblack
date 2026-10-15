/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/mat4x4.hpp>

namespace openblack::ecs::components
{

/// The feet of a creature in the world, as the last frame that drew the land showed them. A footstep lays its print
/// from this pose, not from the pose of the frame in which the footstep falls.
struct CreatureDrawnFeet
{
	glm::mat4 right {1.0f};
	glm::mat4 left {1.0f};
};

} // namespace openblack::ecs::components
