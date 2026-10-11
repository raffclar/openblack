/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AbodeFoundation.h"

#include <algorithm>

namespace abode_foundation = openblack::ecs::abode_foundation;

namespace
{
/// A building is lowered by at most this share of its reach across, or by this height when that is deeper
constexpr float k_SinkShareOfRadius = 0.2f;
constexpr float k_SinkLeast = 0.8f;
} // namespace

abode_foundation::GroundBox abode_foundation::GroundBoxOf(glm::vec3 minima, glm::vec3 maxima)
{
	return {
	    .minima = glm::min(glm::vec2(minima.x, minima.z), glm::vec2(0.0f)),
	    .maxima = glm::max(glm::vec2(maxima.x, maxima.z), glm::vec2(0.0f)),
	};
}

float abode_foundation::GroundRadius(const GroundBox& box, float scale)
{
	const auto half = (box.maxima - box.minima) * 0.5f;
	return scale * std::max(half.x, half.y);
}

std::array<glm::vec2, 4> abode_foundation::GroundCorners(const GroundBox& box, const glm::mat3& rotation, float scale,
                                                         glm::vec2 position)
{
	const auto place = [&](float x, float z) {
		const auto world = rotation * (glm::vec3(x, 0.0f, z) * scale);
		return position + glm::vec2(world.x, world.z);
	};
	return {
	    place(box.minima.x, box.minima.y),
	    place(box.maxima.x, box.minima.y),
	    place(box.minima.x, box.maxima.y),
	    place(box.maxima.x, box.maxima.y),
	};
}

float abode_foundation::DeepestDip(float originHeight, std::span<const float, 4> cornerHeights)
{
	return std::min(originHeight, std::ranges::min(cornerHeights)) - originHeight;
}

float abode_foundation::SinkLimit(float groundRadius)
{
	return std::min(-(groundRadius * k_SinkShareOfRadius), -k_SinkLeast);
}

float abode_foundation::Sink(float deepestDip, float groundRadius)
{
	return std::max(deepestDip, SinkLimit(groundRadius));
}
