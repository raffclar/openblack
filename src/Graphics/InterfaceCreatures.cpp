/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "InterfaceCreatures.h"

#include <cmath>

#include <glm/ext/matrix_transform.hpp>
#include <glm/matrix.hpp>
#include <glm/vec4.hpp>

namespace openblack::graphics::interface_creatures
{

glm::vec3 Eye(float turn)
{
	return {k_CameraDistance * std::cos(turn), k_CameraHeight, k_CameraDistance * std::sin(turn)};
}

glm::vec3 Focus()
{
	return {0.0f, k_FocusHeight, 0.0f};
}

glm::mat4 View(float turn)
{
	return glm::lookAt(Eye(turn), Focus(), glm::vec3(0.0f, 1.0f, 0.0f));
}

float Depth(float creatureSize, int boxSize, float interfaceShare)
{
	constexpr float k_SmallestHeight = 0.1f;
	float depth = creatureSize * k_HeightPerSize;
	if (depth > k_SmallestHeight)
	{
		depth = static_cast<float>(boxSize) / depth;
	}
	if (depth > 1.0f)
	{
		depth = k_DepthScale / depth;
	}
	if (interfaceShare != 0.0f)
	{
		depth /= interfaceShare;
	}
	return depth;
}

glm::mat4 SpaceProjection(const glm::mat4& worldProjection)
{
	auto projection = worldProjection;
	projection[1][1] = projection[0][0] * (k_Space.x / k_Space.y);
	return projection;
}

glm::vec3 PointFromScreen(const glm::mat4& view, const glm::mat4& projection, glm::vec2 screen, glm::vec2 pixel, float depth)
{
	// The pixel in the clip space, then the line through it in the camera's space, scaled to the depth along the camera
	const glm::vec2 clip {(2.0f * pixel.x / screen.x) - 1.0f, 1.0f - (2.0f * pixel.y / screen.y)};
	auto along = glm::inverse(projection) * glm::vec4(clip, 0.5f, 1.0f);
	along /= along.w;
	const glm::vec3 inCamera = glm::vec3(along) * (depth / along.z);
	return glm::vec3(glm::inverse(view) * glm::vec4(inCamera, 1.0f));
}

glm::vec3 MoveToPoint(const glm::vec3& point, const glm::mat4& model, std::span<const glm::mat4> bones, float feetHeight)
{
	glm::vec2 middle {0.0f};
	for (const auto& bone : bones)
	{
		const auto at = model * bone[3];
		middle += glm::vec2(at.x, at.z);
	}
	if (!bones.empty())
	{
		middle *= 1.0f / static_cast<float>(bones.size());
	}
	return {point.x - middle.x, point.y - feetHeight, point.z - middle.y};
}

} // namespace openblack::graphics::interface_creatures
