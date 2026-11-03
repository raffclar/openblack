/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "EditorPaths.h"

#include <algorithm>

#include <glm/geometric.hpp>

#include "3D/MapCoords.h"

namespace openblack::editor::paths
{

bool Draft::Add(glm::vec3 point)
{
	if (!_points.empty() &&
	    glm::distance(glm::vec2(_points.back().x, _points.back().z), glm::vec2(point.x, point.z)) < k_MinimumSpacing)
	{
		return false;
	}
	_points.push_back(point);
	return true;
}

void Draft::RemoveLast()
{
	if (!_points.empty())
	{
		_points.pop_back();
	}
}

int NextStreamId(std::span<const int> ids)
{
	return ids.empty() ? 0 : std::ranges::max(ids) + 1;
}

glm::ivec2 PlaceOf(glm::vec3 point)
{
	return {map_coords::ToFixed(point.x), map_coords::ToFixed(point.z)};
}

float Length(std::span<const glm::vec3> points)
{
	float length = 0.0f;
	for (size_t i = 1; i < points.size(); ++i)
	{
		length += glm::distance(glm::vec2(points[i - 1].x, points[i - 1].z), glm::vec2(points[i].x, points[i].z));
	}
	return length;
}

} // namespace openblack::editor::paths
