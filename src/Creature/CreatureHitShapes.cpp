/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureHitShapes.h"

#include <cmath>

#include <algorithm>

#include <glm/geometric.hpp>
#include <glm/matrix.hpp>

using namespace openblack;
using namespace openblack::creature_hit_shapes;

namespace
{
void Grow(Shape& shape, const glm::vec3& point)
{
	shape.min = glm::min(shape.min, point);
	shape.max = glm::max(shape.max, point);
}
} // namespace

std::vector<Shape> creature_hit_shapes::ShapesOf(size_t count, bool boned, std::span<const Primitive> primitives)
{
	std::vector<Shape> shapes(boned ? count : 1);
	for (const auto& primitive : primitives)
	{
		if (!boned)
		{
			for (const auto& position : primitive.positions)
			{
				Grow(shapes.front(), position);
			}
			continue;
		}
		// The groups take the vertices in turn
		size_t next = 0;
		for (const auto& group : primitive.groups)
		{
			for (uint16_t i = 0; i < group.vertexCount && next < primitive.positions.size(); ++i, ++next)
			{
				if (group.bone < shapes.size())
				{
					Grow(shapes.at(group.bone), primitive.positions[next]);
				}
			}
		}
	}
	return shapes;
}

std::vector<PlacedShape> creature_hit_shapes::Place(std::span<const Shape> shapes, std::span<const glm::mat4> toWorld)
{
	std::vector<PlacedShape> placed;
	const auto count = std::min(shapes.size(), toWorld.size());
	placed.reserve(count);
	for (size_t i = 0; i < count; ++i)
	{
		placed.push_back({.shape = shapes[i], .toWorld = toWorld[i], .toShape = glm::inverse(toWorld[i])});
	}
	return placed;
}

std::optional<Hit> creature_hit_shapes::LineMeets(const PlacedShape& placed, const glm::vec3& origin,
                                                  const glm::vec3& direction)
{
	const auto& [min, max] = placed.shape;
	const auto half = (max - min) * 0.5f;
	if (!(half.x > k_LeastExtent && half.y > k_LeastExtent && half.z > k_LeastExtent))
	{
		return std::nullopt;
	}
	// The line in the space of the box, where the ellipsoid is the unit sphere
	const auto start = ((glm::vec3(placed.toShape * glm::vec4(origin, 1.0f)) - min) / half) - 1.0f;
	const auto step = glm::vec3(placed.toShape * glm::vec4(direction, 0.0f)) / half;
	const auto a = glm::dot(step, step);
	if (!(a > k_LeastExtent))
	{
		return std::nullopt;
	}
	const auto b = 2.0f * glm::dot(step, start);
	const auto discriminant = (b * b) - ((glm::dot(start, start) - 1.0f) * a * 4.0f);
	if (!(discriminant >= 0.0f))
	{
		return std::nullopt;
	}
	const auto along = (-b - std::sqrt(discriminant)) / (a + a);
	const auto local = ((start + (step * along) + 1.0f) * half) + min;
	return Hit {.point = glm::vec3(placed.toWorld * glm::vec4(local, 1.0f)), .along = along};
}

std::optional<Hit> creature_hit_shapes::NearestHit(std::span<const PlacedShape> shapes, const glm::vec3& origin,
                                                   const glm::vec3& direction)
{
	std::optional<Hit> nearest;
	for (const auto& shape : shapes)
	{
		const auto hit = LineMeets(shape, origin, direction);
		if (hit.has_value() && (!nearest.has_value() || hit->along < nearest->along))
		{
			nearest = hit;
		}
	}
	return nearest;
}

bool creature_hit_shapes::WithinLine(const glm::vec3& point, const glm::vec3& origin, const glm::vec3& direction)
{
	const auto length = glm::length(direction);
	if (length <= 0.0f)
	{
		return false;
	}
	const auto ahead = glm::dot(point - origin, direction / length);
	return ahead > 0.0f && ahead < length;
}

std::optional<float> creature_hit_shapes::DepthToBody(std::span<const PlacedShape> shapes, const glm::vec3& point,
                                                      const glm::vec3& towards, float opponentSize)
{
	const auto width = static_cast<float>(static_cast<double>(opponentSize * k_DepthWidthPerSize) * k_DepthWidthShare);
	const auto spacing = (width + width) * k_DepthSpacingShare;
	// The row goes across the direction, flat on the land
	const auto across = glm::vec3(towards.z, 0.0f, -towards.x);
	const auto acrossLength = glm::length(across);
	const auto unitAcross = acrossLength > 0.0f ? across / acrossLength : glm::vec3(0.0f);
	auto origin = point - (unitAcross * width);
	const auto next = unitAcross * spacing;
	std::optional<float> depth;
	for (int i = 0; i < k_DepthLines; ++i)
	{
		if (const auto hit = NearestHit(shapes, origin, towards); hit.has_value())
		{
			depth = std::min(depth.value_or(k_NoDepth), glm::dot(hit->point - point, towards));
		}
		origin += next;
	}
	return depth;
}
