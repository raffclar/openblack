/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SpiritView.h"

#include <glm/common.hpp>
#include <glm/vec4.hpp>

using namespace openblack;
using namespace openblack::help::spirits;

namespace
{
glm::vec2 HalfResolution(const Screen& screen)
{
	return {static_cast<float>(screen.width) * 0.5f, static_cast<float>(screen.height) * 0.5f};
}
} // namespace

glm::vec3 help::spirits::PointFromScreen(const SpiritView& view, glm::vec2 pixel, float depth)
{
	const glm::vec2 half = HalfResolution(view.screen);
	const float acrossAtNear = ((pixel.x - half.x) * (view.halfExtent.x * view.nearClip)) / half.x;
	const float upAtNear = ((half.y - pixel.y) * (view.halfExtent.y * view.nearClip)) / half.y;
	float across = acrossAtNear;
	float up = upAtNear;
	float ahead = view.nearClip;
	if (depth != 0.0f)
	{
		across = (depth / view.nearClip) * acrossAtNear;
		up = (depth / view.nearClip) * upAtNear;
		ahead = depth;
	}
	return view.eye + (view.right * across + view.up * up + view.forward * ahead);
}

std::optional<ProjectedPoint> help::spirits::ProjectPoint(const SpiritView& view, const glm::vec3& point)
{
	const glm::vec4 clip = view.viewProjection * glm::vec4(point, 1.0f);
	if (clip.w < view.nearClip)
	{
		return std::nullopt;
	}
	const glm::vec2 half = HalfResolution(view.screen);
	const float inverse = 1.0f / clip.w;
	return ProjectedPoint {.x = static_cast<int32_t>((inverse * clip.x + 1.0f) * half.x),
	                       .y = static_cast<int32_t>(half.y - inverse * clip.y * half.y),
	                       .depth = clip.w};
}

std::optional<glm::vec2> help::spirits::WorldToPixel(const SpiritView& view, const glm::vec3& point, bool force)
{
	const glm::vec4 clip = view.viewProjection * glm::vec4(point, 1.0f);
	if (clip.w < view.nearClip)
	{
		return std::nullopt;
	}
	const bool offScreen = clip.x > clip.w || clip.x < -clip.w || clip.y > clip.w || clip.y < -clip.w;
	if (offScreen && !force)
	{
		return std::nullopt;
	}
	const glm::vec2 half = HalfResolution(view.screen);
	const float inverse = 1.0f / clip.w;
	if (offScreen)
	{
		const float x = inverse * clip.x;
		const float y = inverse * clip.y;
		return glm::vec2((x + 1.0f) * half.x, (1.0f - y) * half.y);
	}
	// On the screen it is kept on the screen
	const glm::vec2 pixel((inverse * clip.x + 1.0f) * half.x, half.y - inverse * clip.y * half.y);
	return glm::clamp(pixel, glm::vec2(0.0f), glm::vec2(view.screen.width, view.screen.height));
}
