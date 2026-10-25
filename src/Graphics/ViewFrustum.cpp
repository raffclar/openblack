/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ViewFrustum.h"

#include <algorithm>
#include <array>

#include <glm/geometric.hpp>

namespace openblack::graphics::view_frustum
{

Frustum FromViewProjection(const glm::mat4& viewProjection) noexcept
{
	// A clip-space point is on screen where -w <= x <= w and -w <= y <= w; each side is the matrix's w row plus or minus
	// its x or y row
	const auto row = [&viewProjection](int index) {
		return glm::vec4(viewProjection[0][index], viewProjection[1][index], viewProjection[2][index],
		                 viewProjection[3][index]);
	};
	Frustum frustum {.sides = {row(3) + row(0), row(3) - row(0), row(3) + row(1), row(3) - row(1)}};
	// Made unit length, so that a sphere's radius compares with a plane's distance
	for (auto& side : frustum.sides)
	{
		const auto length = glm::length(glm::vec3(side));
		if (length > 0.0f)
		{
			side /= length;
		}
	}
	return frustum;
}

bool SeesSphere(const Frustum& frustum, glm::vec3 centre, float radius) noexcept
{
	return std::ranges::all_of(frustum.sides, [centre, radius](const glm::vec4& side) {
		return glm::dot(glm::vec3(side), centre) + side.w >= -radius;
	});
}

bool SeesSphereOrReflection(const Frustum& frustum, glm::vec3 centre, float radius) noexcept
{
	return SeesSphere(frustum, centre, radius) || SeesSphere(frustum, {centre.x, -centre.y, centre.z}, radius);
}

bool SeesBox(const Frustum& frustum, const glm::mat4& model, glm::vec3 minima, glm::vec3 maxima) noexcept
{
	std::array<glm::vec3, 8> corners {};
	for (size_t i = 0; i < corners.size(); ++i)
	{
		const glm::vec3 corner {(i & 1u) != 0 ? maxima.x : minima.x, (i & 2u) != 0 ? maxima.y : minima.y,
		                        (i & 4u) != 0 ? maxima.z : minima.z};
		corners.at(i) = glm::vec3(model * glm::vec4(corner, 1.0f));
	}
	return std::ranges::none_of(frustum.sides, [&corners](const glm::vec4& side) {
		return std::ranges::all_of(
		    corners, [&side](const glm::vec3& corner) { return glm::dot(glm::vec3(side), corner) + side.w < -k_BoxMargin; });
	});
}

} // namespace openblack::graphics::view_frustum
