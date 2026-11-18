/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LakeSurface.h"

#include <algorithm>
#include <array>

using namespace openblack::graphics;
using namespace openblack::graphics::lake_surface;

namespace
{
constexpr int k_Cells = openblack::land_lakes::k_CornersPerSide - 1;
constexpr float k_CellSize = openblack::land_lakes::k_CellSize;

/// A point of a triangle of the land: where it is, the land's height, and the height that the cut goes by
struct Point
{
	float x;
	float z;
	float height;
	float cut;
};
using Polygon = std::vector<Point>;

Point Between(const Point& a, const Point& b, float t)
{
	return {.x = a.x + ((b.x - a.x) * t),
	        .z = a.z + ((b.z - a.z) * t),
	        .height = a.height + ((b.height - a.height) * t),
	        .cut = a.cut + ((b.cut - a.cut) * t)};
}

/// The part of the polygon where the cut height is at the limit or below it
Polygon Clip(const Polygon& polygon, float limit)
{
	Polygon kept;
	const auto inside = [limit](const Point& point) { return point.cut <= limit; };
	for (size_t i = 0; i < polygon.size(); ++i)
	{
		const auto& current = polygon[i];
		const auto& next = polygon[(i + 1) % polygon.size()];
		const bool currentIn = inside(current);
		const bool nextIn = inside(next);
		if (currentIn)
		{
			kept.push_back(current);
		}
		if (currentIn != nextIn)
		{
			kept.push_back(Between(current, next, (limit - current.cut) / (next.cut - current.cut)));
		}
	}
	return kept;
}

/// Adds the polygon as a fan of triangles, flat at the level
void Emit(Surface& surface, const Polygon& polygon, float level)
{
	if (polygon.size() < 3)
	{
		return;
	}
	const auto first = static_cast<uint32_t>(surface.vertices.size());
	for (const auto& point : polygon)
	{
		surface.vertices.push_back(
		    {.x = point.x, .y = level, .z = point.z, .landHeight = point.height, .level = level, .fade = point.cut});
	}
	for (uint32_t i = 1; i + 1 < polygon.size(); ++i)
	{
		surface.indices.insert(surface.indices.end(), {first, first + i, first + i + 1});
	}
}
} // namespace

Surface lake_surface::Build(const land_lakes::LakeMap& lakes, const HeightAt& heightAt, const SplitAt& splitAt,
                            float shoreReach)
{
	Surface surface;
	std::vector<int32_t> cellLake(static_cast<size_t>(k_Cells) * static_cast<size_t>(k_Cells), -1);
	const auto all = lakes.GetLakes();
	for (size_t lakeIndex = 0; lakeIndex < all.size(); ++lakeIndex)
	{
		const auto& lake = all[lakeIndex];
		const float level = lake.level;
		for (const auto corner : lake.corners)
		{
			const int cx = static_cast<int>(corner / land_lakes::k_CornersPerSide);
			const int cz = static_cast<int>(corner % land_lakes::k_CornersPerSide);
			for (int x = cx - 1; x <= cx; ++x)
			{
				for (int z = cz - 1; z <= cz; ++z)
				{
					if (x < 0 || z < 0 || x >= k_Cells || z >= k_Cells)
					{
						continue;
					}
					auto& owner = cellLake[(static_cast<size_t>(x) * k_Cells) + static_cast<size_t>(z)];
					if (owner == static_cast<int32_t>(lakeIndex))
					{
						continue;
					}
					owner = static_cast<int32_t>(lakeIndex);

					// The four corners of the cell. A low corner outside the lake counts as land a little above the level, so
					// the water stops before it.
					std::array<Point, 4> points {};
					bool hasLand = true;
					constexpr std::array<std::array<int, 2>, 4> k_Corners {{{0, 0}, {1, 0}, {1, 1}, {0, 1}}};
					for (size_t i = 0; i < k_Corners.size(); ++i)
					{
						const int px = x + k_Corners[i][0];
						const int pz = z + k_Corners[i][1];
						const auto height = heightAt(px, pz);
						if (!height.has_value())
						{
							hasLand = false;
							break;
						}
						const bool inLake = lakes.LakeOfCorner(px, pz) == lakeIndex;
						points[i] = {.x = static_cast<float>(px) * k_CellSize,
						             .z = static_cast<float>(pz) * k_CellSize,
						             .height = *height,
						             .cut = inLake ? *height : std::max(*height, level + shoreReach)};
					}
					if (!hasLand)
					{
						continue;
					}
					// The two triangles of the cell, cut as the land is cut
					const bool split = splitAt(x, z);
					const std::array<Polygon, 2> triangles =
					    split ? std::array<Polygon, 2> {Polygon {points[0], points[1], points[3]},
					                                    Polygon {points[1], points[2], points[3]}}
					          : std::array<Polygon, 2> {Polygon {points[0], points[1], points[2]},
					                                    Polygon {points[0], points[2], points[3]}};
					for (const auto& triangle : triangles)
					{
						Emit(surface, Clip(triangle, level), level);
					}
				}
			}
		}
	}
	return surface;
}
