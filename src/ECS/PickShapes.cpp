/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PickShapes.h"

#include <algorithm>

#include <glm/geometric.hpp>

using namespace openblack::ecs;

float pick_shapes::FlatRadius(glm::vec3 halfExtents, float scale)
{
	return std::max(halfExtents.x, halfExtents.z) * scale;
}

float pick_shapes::Height(glm::vec3 halfExtents, float scale)
{
	const float half = halfExtents.y * scale;
	return half + half;
}

pick_shapes::Ball pick_shapes::DispenserBall(glm::vec3 position, glm::vec3 halfExtents, float scale)
{
	return {
	    .centre = position + glm::vec3(0.0f, Height(halfExtents, scale) * k_DispenserBallHeight, 0.0f),
	    .radius = FlatRadius(halfExtents, scale) * k_DispenserBallWidth,
	};
}

pick_shapes::Ball pick_shapes::BubbleBall(glm::vec3 modelMiddle, float scale)
{
	return {.centre = modelMiddle, .radius = scale * k_BubbleBallPerScale};
}

glm::vec3 pick_shapes::BubblePickOffset(glm::vec3 modelMiddle, glm::vec3 camera, glm::vec3 halfExtents, float scale)
{
	const auto towards = camera - modelMiddle;
	const float length = glm::length(towards);
	if (length <= 0.0f)
	{
		return glm::vec3(0.0f);
	}
	return towards / length * FlatRadius(halfExtents, scale);
}
