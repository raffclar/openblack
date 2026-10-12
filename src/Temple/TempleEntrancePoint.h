/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cmath>

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

namespace openblack::temple
{

/// The place of the temple's model the scripts take as its entrance: the sixteenth of the points its model marks
constexpr size_t k_EntrancePlace = 15;

/// A point `radius` from the temple towards its entrance and `height` above that, as the scripts ask for one to put
/// their cameras at. The way to the entrance is taken in three dimensions; an entrance at the temple's own place leaves
/// the point over the temple.
[[nodiscard]] inline glm::vec3 TowardsEntrance(glm::vec3 temple, glm::vec3 entrance, float radius, float height)
{
	auto way = entrance - temple;
	if (way != glm::vec3(0.0f))
	{
		way *= 1.0f / std::sqrt(glm::dot(way, way));
	}
	auto point = temple + (way * radius);
	point.y += height;
	return point;
}

} // namespace openblack::temple
