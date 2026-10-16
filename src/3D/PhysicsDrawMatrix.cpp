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

#include "ECS/Systems/UnderwaterSystemInterface.h"
#include "Locator.h"

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
	const auto model = glm::translate(glm::mat4(1.0f), pose->origin) * glm::mat4(pose->axes);
	// EXPERIMENT (not for merge): a thing that floats rides the experiment's waves and tide
	return Locator::underwaterSystem::has_value() ? Locator::underwaterSystem::value().FloatOnWaves(model) : model;
}

glm::vec3 openblack::physics_draw::Position(const glm::vec3& standing, const ecs::components::PhysicsDrawPose* pose)
{
	return pose == nullptr ? standing : pose->origin;
}
