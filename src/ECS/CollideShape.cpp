/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CollideShape.h"

#include <cmath>
#include <cstdint>

#include <algorithm>

#include <glm/common.hpp>

using namespace openblack::ecs::collide;

namespace
{

/// The size of a map cell, in metres
constexpr int32_t k_CellMetres = 10;
/// The smallest half side a thing's box is taken to have, in metres
constexpr float k_SmallestHalfSide = 1.0f;

/// Whether two circles overlap or touch, worked out as the game does: the sum of the radii squared, spelt out, with
/// each radius squared as the circle holds it
[[nodiscard]] bool Overlap(const Circle& a, float aSquared, const Circle& b, float bSquared)
{
	const float dx = a.centre.x - b.centre.x;
	const float dz = a.centre.y - b.centre.y;
	const float both = b.radius * a.radius;
	return dx * dx + dz * dz <= both + both + bSquared + aSquared;
}

[[nodiscard]] float Squared(const Circle& circle)
{
	return circle.radius * circle.radius;
}

/// The row of circles covering a long box, from one end of its long side to the other
[[nodiscard]] std::vector<Circle> RowAlong(glm::vec2 centre, glm::vec2 halfSize, glm::vec2 xAxis, glm::vec2 zAxis)
{
	const bool alongX = halfSize.x > halfSize.y;
	const float longHalf = alongX ? halfSize.x : halfSize.y;
	const float shortHalf = alongX ? halfSize.y : halfSize.x;
	const int32_t count = static_cast<int32_t>(longHalf / shortHalf) + 1;
	const float length = longHalf + longHalf;
	const float spacing = length / static_cast<float>(count);
	std::vector<Circle> row;
	row.reserve(static_cast<size_t>(count));
	for (int32_t i = 0; i < count; ++i)
	{
		const float offset = (static_cast<float>(i) + 0.5f) * spacing - length * 0.5f;
		const glm::vec2 local = alongX ? glm::vec2(offset, 0.0f) : glm::vec2(0.0f, offset);
		row.push_back({.centre = centre + xAxis * local.x + zAxis * local.y, .radius = shortHalf});
	}
	return row;
}

} // namespace

Shape openblack::ecs::collide::ShapeOfBox(glm::vec2 centre, glm::vec2 halfSize, glm::vec2 xAxis, glm::vec2 zAxis)
{
	const glm::vec2 half = glm::max(halfSize, glm::vec2(k_SmallestHalfSide));
	const float longer = std::max(half.x, half.y);
	const float shorter = std::min(half.x, half.y);
	if (longer / shorter <= k_SquareEnough)
	{
		return {.bounds = {.centre = centre, .radius = longer}, .boundsSquared = longer * longer, .row = {}};
	}
	const float cornerSquared = half.y * half.y + half.x * half.x;
	return {.bounds = {.centre = centre, .radius = std::sqrt(cornerSquared)},
	        .boundsSquared = cornerSquared,
	        .row = RowAlong(centre, half, xAxis, zAxis)};
}

bool openblack::ecs::collide::Touches(const Circle& circle, const Shape& shape)
{
	const float squared = Squared(circle);
	if (!Overlap(circle, squared, shape.bounds, shape.boundsSquared))
	{
		return false;
	}
	return shape.row.empty() || std::ranges::any_of(shape.row, [&circle, squared](const Circle& part) {
		       return Overlap(part, Squared(part), circle, squared);
	       });
}

std::vector<glm::ivec2> openblack::ecs::collide::FootprintCells(const Shape& shape, float reach, glm::ivec2 cells)
{
	constexpr float k_CellsPerMetre = 0.1f;
	const glm::vec2 centre = shape.bounds.centre;
	glm::ivec2 min {static_cast<int32_t>((centre.x - reach) * k_CellsPerMetre),
	                static_cast<int32_t>((centre.y - reach) * k_CellsPerMetre)};
	glm::ivec2 max {static_cast<int32_t>((centre.x + reach) * k_CellsPerMetre),
	                static_cast<int32_t>((centre.y + reach) * k_CellsPerMetre)};
	// Off the low edges the range starts at the first cell
	for (int axis = 0; axis < 2; ++axis)
	{
		if (min[axis] < 0)
		{
			min[axis] = 0;
			max[axis] = std::max(max[axis], 0);
		}
	}

	std::vector<glm::ivec2> filed;
	for (int32_t x = min.x; x <= max.x; ++x)
	{
		for (int32_t z = min.y; z <= max.y; ++z)
		{
			if (x >= cells.x || z >= cells.y)
			{
				continue;
			}
			const Circle cell {.centre = {static_cast<float>(x * k_CellMetres + k_CellMetres / 2),
			                              static_cast<float>(z * k_CellMetres + k_CellMetres / 2)},
			                   .radius = k_CellReach};
			if (Touches(cell, shape))
			{
				filed.emplace_back(x, z);
			}
		}
	}
	if (filed.empty())
	{
		// The middle cell of the range, unless that is off the map
		const glm::ivec2 middle = min + (max - min + 1) / 2;
		if (middle.x < cells.x && middle.y < cells.y)
		{
			filed.push_back(middle);
		}
	}
	return filed;
}
