/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScreenPoint.h"

#include <cmath>

using namespace openblack;
using namespace openblack::screen_point;

Lens screen_point::LensOf(int width, int height, float horizontalFov)
{
	const float w = static_cast<float>(width);
	const float h = static_cast<float>(height);
	return {w * 0.5f, h * 0.5f, std::tan(horizontalFov * 0.5f), h != 0.0f ? w / h : 1.0f};
}

glm::vec3 screen_point::CameraPointFromScreen(const Lens& lens, int32_t x, int32_t y, float depth)
{
	const float vx = (static_cast<float>(x) - lens.halfW) * lens.tanHalf / lens.halfW;
	const float vy = (lens.halfH - static_cast<float>(y)) * (lens.tanHalf / lens.aspect) / lens.halfH;
	return {vx * depth, vy * depth, depth};
}

glm::vec2 screen_point::ProjectCamera(const Lens& lens, const glm::vec3& v)
{
	const float fx = 1.0f / lens.tanHalf;
	const float fy = lens.aspect / lens.tanHalf;
	return {(fx * v.x / v.z + 1.0f) * lens.halfW, lens.halfH - fy * v.y / v.z * lens.halfH};
}

glm::vec3 screen_point::ToCamera(const graphics::billboard::CameraFrame& frame, const glm::vec3& world)
{
	const glm::vec3 d = world - frame.eye;
	return {glm::dot(d, frame.right), glm::dot(d, frame.up), glm::dot(d, frame.forward)};
}

glm::vec3 screen_point::ToWorld(const graphics::billboard::CameraFrame& frame, const glm::vec3& camera)
{
	return frame.eye + frame.right * camera.x + frame.up * camera.y + frame.forward * camera.z;
}

glm::vec3 screen_point::PointFromScreen(const graphics::billboard::CameraFrame& frame, const Lens& lens, int32_t x, int32_t y,
                                        float depth)
{
	return ToWorld(frame, CameraPointFromScreen(lens, x, y, depth));
}

std::optional<Projected> screen_point::Project(const graphics::billboard::CameraFrame& frame, const Lens& lens,
                                               const glm::vec3& world)
{
	const glm::vec3 v = ToCamera(frame, world);
	if (!(v.z > 0.0f))
	{
		return std::nullopt;
	}
	return Projected {ProjectCamera(lens, v), v.z};
}
