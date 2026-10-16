/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Seabed.h"

#include <cmath>
#include <cstddef>

#include <algorithm>
#include <limits>

#include <glm/common.hpp>

using namespace openblack;

namespace
{
/// A number from 0 to 1 for a lattice point, the same each time
float Hash(int x, int z)
{
	auto h = static_cast<uint32_t>(x) * 374761393u + static_cast<uint32_t>(z) * 668265263u;
	h = (h ^ (h >> 13u)) * 1274126177u;
	h ^= h >> 16u;
	return static_cast<float>(h & 0xFFFFu) / 65535.0f;
}

/// Smooth noise from 0 to 1 at a point in cells, with features about `cells` corners across
float ValueNoise(float x, float z, float cells)
{
	const float fx = x / cells;
	const float fz = z / cells;
	const auto ix = static_cast<int>(std::floor(fx));
	const auto iz = static_cast<int>(std::floor(fz));
	auto tx = fx - static_cast<float>(ix);
	auto tz = fz - static_cast<float>(iz);
	tx = tx * tx * (3.0f - 2.0f * tx);
	tz = tz * tz * (3.0f - 2.0f * tz);
	const float low = glm::mix(Hash(ix, iz), Hash(ix + 1, iz), tx);
	const float high = glm::mix(Hash(ix, iz + 1), Hash(ix + 1, iz + 1), tx);
	return glm::mix(low, high, tz);
}

/// Broad hills and dips, from -1 to 1: three layers of noise, the point first moved by more noise so that the hills
/// are not in rows
float Hills(int x, int z)
{
	const auto fx = static_cast<float>(x);
	const auto fz = static_cast<float>(z);
	const float warpX = (ValueNoise(fx + 37.0f, fz, 40.0f) - 0.5f) * 30.0f;
	const float warpZ = (ValueNoise(fx, fz + 91.0f, 40.0f) - 0.5f) * 30.0f;
	const float wx = fx + warpX;
	const float wz = fz + warpZ;
	const float sum = ValueNoise(wx, wz, 48.0f) * 0.55f + ValueNoise(wx, wz, 22.0f) * 0.3f + ValueNoise(wx, wz, 9.0f) * 0.15f;
	return sum * 2.0f - 1.0f;
}
} // namespace

float seabed::DepthForDistance(float cells)
{
	if (cells <= 0.0f)
	{
		return 0.0f;
	}
	// A gentle shelf near the coast, then the shelf's edge and a steeper slope down into deep water
	const float shelf = k_ShelfDepth * (1.0f - std::exp(-cells / k_SlopeCells));
	const float t = std::clamp((cells - 25.0f) / 95.0f, 0.0f, 1.0f);
	return shelf + k_SlopeDepth * t * t * (3.0f - 2.0f * t);
}

namespace
{
/// The distance of each corner to the nearest source corner, in cells: a chamfer pass forwards, then backwards
std::vector<float> DistanceTo(const std::vector<bool>& sources, glm::ivec2 size)
{
	constexpr float k_Far = std::numeric_limits<float>::max() / 4.0f;
	constexpr float k_Diagonal = 1.41421356f;
	std::vector<float> distance(sources.size(), k_Far);
	const auto at = [&size](int x, int z) {
		return static_cast<size_t>(z) * static_cast<size_t>(size.x) + static_cast<size_t>(x);
	};
	for (size_t i = 0; i < sources.size(); ++i)
	{
		if (sources[i])
		{
			distance[i] = 0.0f;
		}
	}
	const auto relax = [&](int x, int z, int dx, int dz, float step) {
		const int nx = x + dx;
		const int nz = z + dz;
		if (nx >= 0 && nz >= 0 && nx < size.x && nz < size.y)
		{
			distance[at(x, z)] = std::min(distance[at(x, z)], distance[at(nx, nz)] + step);
		}
	};
	for (int z = 0; z < size.y; ++z)
	{
		for (int x = 0; x < size.x; ++x)
		{
			relax(x, z, -1, 0, 1.0f);
			relax(x, z, 0, -1, 1.0f);
			relax(x, z, -1, -1, k_Diagonal);
			relax(x, z, 1, -1, k_Diagonal);
		}
	}
	for (int z = size.y - 1; z >= 0; --z)
	{
		for (int x = size.x - 1; x >= 0; --x)
		{
			relax(x, z, 1, 0, 1.0f);
			relax(x, z, 0, 1, 1.0f);
			relax(x, z, 1, 1, k_Diagonal);
			relax(x, z, -1, 1, k_Diagonal);
		}
	}
	return distance;
}
} // namespace

seabed::Seabed seabed::Build(std::span<const uint8_t> altitudes, glm::ivec2 size, glm::vec2 origin, float spacing)
{
	Seabed seabed {.size = size, .origin = origin, .spacing = spacing, .depth = {}};
	const auto count = static_cast<size_t>(std::max(size.x, 0)) * static_cast<size_t>(std::max(size.y, 0));
	if (count == 0 || altitudes.size() < count)
	{
		seabed.size = {0, 0};
		return seabed;
	}
	std::vector<bool> land(count);
	for (size_t i = 0; i < count; ++i)
	{
		land[i] = altitudes[i] > k_SeaLevelAltitude;
	}
	const auto toLand = DistanceTo(land, size);
	const auto at = [&size](int x, int z) {
		return static_cast<size_t>(z) * static_cast<size_t>(size.x) + static_cast<size_t>(x);
	};
	seabed.depth.resize(count);
	for (int z = 0; z < size.y; ++z)
	{
		for (int x = 0; x < size.x; ++x)
		{
			const auto cells = std::min(toLand[at(x, z)], 1000.0f);
			if (cells <= 0.0f)
			{
				seabed.depth[at(x, z)] = 0.0f;
				continue;
			}
			float depth = DepthForDistance(cells);
			// Deeper again towards the map's edge, away from the coast
			const auto edge = static_cast<float>(std::min(std::min(x, z), std::min(size.x - 1 - x, size.y - 1 - z)));
			const float towardsEdge = 1.0f - std::clamp(edge / 80.0f, 0.0f, 1.0f);
			depth += k_EdgeDepth * towardsEdge * towardsEdge * std::clamp((cells - 10.0f) / 30.0f, 0.0f, 1.0f);
			// The hills and dips: none at the coast, and larger in deeper water
			const float amplitude = std::min(cells / 4.0f, 1.0f) * (1.5f + depth * 0.1f);
			depth += Hills(x, z) * amplitude;
			seabed.depth[at(x, z)] = std::clamp(depth, 0.5f, k_MaxDepth);
		}
	}
	return seabed;
}

float seabed::Seabed::DepthAt(glm::vec2 world) const
{
	if (size.x <= 0 || size.y <= 0 || depth.empty())
	{
		return 0.0f;
	}
	const auto grid = glm::clamp((world - origin) / spacing, glm::vec2(0.0f), glm::vec2(size - 1));
	const auto first = glm::min(glm::ivec2(glm::floor(grid)), glm::max(size - 2, glm::ivec2(0)));
	const auto last = glm::min(first + 1, size - 1);
	const auto blend = glm::clamp(grid - glm::vec2(first), glm::vec2(0.0f), glm::vec2(1.0f));
	const auto value = [this](int x, int z) {
		return depth[static_cast<size_t>(z) * static_cast<size_t>(size.x) + static_cast<size_t>(x)];
	};
	const float low = glm::mix(value(first.x, first.y), value(last.x, first.y), blend.x);
	const float high = glm::mix(value(first.x, last.y), value(last.x, last.y), blend.x);
	return glm::mix(low, high, blend.y);
}

float seabed::FloorHeight(const Seabed& seabed, glm::vec2 world, float landHeight)
{
	if (landHeight > 0.0f)
	{
		return landHeight;
	}
	return -seabed.DepthAt(world);
}
