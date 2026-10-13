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

pick_shapes::Ball pick_shapes::PictureBall(glm::vec3 picture, float size, glm::vec3 seedModelMiddle)
{
	return {.centre = picture + seedModelMiddle, .radius = size * k_PictureBallPerSize};
}

pick_shapes::Ball pick_shapes::BubbleBall(glm::vec3 modelMiddle, float scale, glm::vec3 seedModelMiddle)
{
	return PictureBall(modelMiddle, scale * k_BubblePictureSize, seedModelMiddle);
}

pick_shapes::Ball pick_shapes::VortexBall(glm::vec3 centre, glm::vec3 cylinderHalfExtents)
{
	return {.centre = centre, .radius = FlatRadius(cylinderHalfExtents, 1.0f) * k_VortexBallWidth};
}

pick_shapes::Ball pick_shapes::TeleportBall(glm::vec3 position)
{
	return {.centre = position, .radius = k_TeleportBallRadius};
}

pick_shapes::Ball pick_shapes::LeashPostBall(glm::vec3 hangingPoint)
{
	return {.centre = hangingPoint, .radius = k_LeashPostBallRadius};
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
