/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PhysicsDrawMatrix.h"

#include <glm/ext/matrix_transform.hpp>

std::optional<glm::mat4> openblack::physics_draw::ModelMatrix(const glm::mat4& standing,
                                                              const ecs::components::PhysicsDrawPose* pose)
{
	if (pose == nullptr)
	{
		return standing;
	}
	if (pose->underSea)
	{
		return std::nullopt;
	}
	return glm::translate(glm::mat4(1.0f), pose->origin) * glm::mat4(pose->axes);
}
