/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ModelSurface.h"

#include <cmath>

#include <L3DFile.h>

#include "Common/GameRandom.h"

namespace openblack::model_surface
{

std::vector<Triangle> DrawnTriangles(const l3d::L3DFile& model, DrawnParts drawn)
{
	std::vector<Triangle> triangles;
	const auto corner = [](const l3d::L3DVertex& vertex) {
		return Corner {
		    .position = {vertex.position.x, vertex.position.y, vertex.position.z},
		    .normal = {vertex.normal.x, vertex.normal.y, vertex.normal.z},
		};
	};
	const auto& parts = model.GetSubmeshHeaders();
	for (uint32_t part = 0; part < parts.size(); ++part)
	{
		const auto flags = parts[part].flags;
		if ((flags.lodMask & drawn.detailLevels) == 0 || flags.status > drawn.latestState)
		{
			continue;
		}
		const auto& vertices = model.GetVertexSpan(part);
		const auto& indices = model.GetIndexSpan(part);
		// Each primitive's triangles count from its own first vertex
		size_t firstIndex = 0;
		size_t firstVertex = 0;
		for (const auto& primitive : model.GetPrimitiveSpan(part))
		{
			for (size_t i = 0; i < primitive.numTriangles; ++i)
			{
				Triangle triangle;
				bool whole = true;
				for (size_t c = 0; c < 3; ++c)
				{
					const auto at = firstIndex + i * 3 + c;
					const auto index = at < indices.size() ? firstVertex + indices[at] : vertices.size();
					if (index >= vertices.size())
					{
						whole = false;
						break;
					}
					triangle.at(c) = corner(vertices[index]);
				}
				if (whole)
				{
					triangles.push_back(triangle);
				}
			}
			firstIndex += primitive.numTriangles * 3ul;
			firstVertex += primitive.numVertices;
		}
	}
	return triangles;
}

namespace
{

/// The first corner's value, then the way to the second by one fraction, then the way to the third by the other
glm::vec3 Blend(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, float toB, float toC)
{
	return (a + (b - a) * toB) + (c - a) * toC;
}

} // namespace

std::optional<Point> RandomPoint(std::span<const Triangle> triangles, const glm::mat4& placement, GameRandomInterface& random)
{
	if (triangles.empty())
	{
		return std::nullopt;
	}
	const auto& triangle = triangles[random.LocalRand(static_cast<int32_t>(triangles.size()))];
	auto toB = random.LocalFloatRand(1.0f);
	auto toC = random.LocalFloatRand(1.0f);
	// Past the far edge the point is folded back into the triangle
	if (static_cast<double>(toC) + static_cast<double>(toB) > 1.0)
	{
		toB = 1.0f - toB;
		toC = 1.0f - toC;
	}
	const auto& [a, b, c] = triangle;
	const auto position = Blend(a.position, b.position, c.position, toB, toC);
	const auto normal = Blend(a.normal, b.normal, c.normal, toB, toC);
	const glm::vec3 x {placement[0]};
	const glm::vec3 y {placement[1]};
	const glm::vec3 z {placement[2]};
	const glm::vec3 at {placement[3]};
	const auto turn = [&](const glm::vec3& v) { return x * v.x + y * v.y + z * v.z; };
	const auto turned = turn(normal);
	const auto length = std::sqrt(glm::dot(turned, turned));
	return Point {
	    .position = turn(position) + at,
	    .normal = length > 0.0f ? turned / length : turned,
	};
}

std::optional<Point> RandomUpwardPoint(std::span<const Triangle> triangles, const glm::mat4& placement,
                                       GameRandomInterface& random)
{
	for (uint32_t draw = 0; draw < k_MostDraws; ++draw)
	{
		const auto point = RandomPoint(triangles, placement, random);
		if (!point.has_value())
		{
			return std::nullopt;
		}
		if (!(point->normal.y < k_LowestNormalY))
		{
			return point;
		}
	}
	return std::nullopt;
}

} // namespace openblack::model_surface
