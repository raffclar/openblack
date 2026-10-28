/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "NeedFlagRules.h"

#include "3D/MapCoords.h"

namespace openblack::ecs::need_flag
{

namespace
{

/// The share of the room above the least need over which a new flag grows to its full size
constexpr float k_GrowShare = 0.2f;

} // namespace

bool Shows(float need, const Row& row)
{
	return row.showsAfter < need;
}

float Fill(float need, const Row& row)
{
	if (need < row.maxNeed)
	{
		return need / row.maxNeed;
	}
	return row.maxNeed / row.maxNeed;
}

float DrawnScale(float fill, const Row& row)
{
	const float whole = (row.maxNeed - row.showsAfter) * k_GrowShare;
	if (fill < whole)
	{
		return (fill - row.showsAfter) / (whole - row.showsAfter);
	}
	return 1.0f;
}

glm::vec2 Place(const glm::vec3& point, const glm::mat4& building)
{
	// The terms add in the order that the game adds them
	const float x = ((point.y * building[1].x) + (point.z * building[2].x)) + (point.x * building[0].x) + building[3].x;
	const float z = ((point.x * building[0].z) + (point.y * building[1].z)) + (point.z * building[2].z) + building[3].z;
	return {map_coords::Quantise(x), map_coords::Quantise(z)};
}

} // namespace openblack::ecs::need_flag
