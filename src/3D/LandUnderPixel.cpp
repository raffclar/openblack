/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LandUnderPixel.h"

#include <cmath>

#include <glm/vec2.hpp>

#include "3D/LandIslandInterface.h"

using namespace openblack;

std::optional<glm::vec3> land_pick::UnderPixel(const LandIslandInterface& island, glm::vec3 camera, glm::vec3 nearPoint,
                                               bool withSea)
{
	glm::vec2 hit(0.0f);
	glm::vec3 point(0.0f);
	if (withSea ? island.RayCast(camera, nearPoint, hit, camera) : island.RayCastLand(camera, nearPoint, hit))
	{
		point = glm::vec3(hit.x, 0.0f, hit.y);
	}
	else if (withSea && camera.y > nearPoint.y)
	{
		// the sea's level wherever the line from the camera goes down, however far
		const float share = -(camera.y / (nearPoint.y - camera.y));
		point = glm::vec3((nearPoint.x - camera.x) * share + camera.x, 0.0f, (nearPoint.z - camera.z) * share + camera.z);
	}
	else
	{
		return std::nullopt;
	}
	point.y = island.GetHeightAt(glm::vec2(point.x, point.z));
	return KeptInReach(point);
}

glm::vec3 land_pick::KeptInReach(glm::vec3 point)
{
	auto fromMiddle = glm::vec3(point.x - k_MapMiddle, point.y, point.z - k_MapMiddle);
	const float length = std::sqrt(fromMiddle.z * fromMiddle.z + fromMiddle.y * fromMiddle.y + fromMiddle.x * fromMiddle.x);
	if (!(k_PickReach < length))
	{
		return point;
	}
	fromMiddle *= k_PickReach / length;
	return {fromMiddle.x + k_MapMiddle, fromMiddle.y, fromMiddle.z + k_MapMiddle};
}
