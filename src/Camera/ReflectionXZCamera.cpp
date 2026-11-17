/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ReflectionXZCamera.h"

#include <glm/gtc/constants.hpp>

using namespace openblack;

ReflectionXZCamera::ReflectionXZCamera(float level)
    : Camera(glm::zero<glm::vec3>())
    , _level(level)
{
}

glm::mat4 ReflectionXZCamera::GetViewMatrix(Interpolation interpolation) const
{
	// The reflection in the plane y = level: the height turns over about the level
	return Camera::GetViewMatrix(interpolation) *
	       glm::mat4(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 2.0f * _level, 0.0f, 1.0f);
}
