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
#include <glm/geometric.hpp>

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

float seabed::BeachDepth(float cells)
{
	// A gentle slope down to below the lowest tide, then steeper out to the shelf
	const float beyond = std::max(cells - 10.0f, 0.0f);
	return k_BeachSlope * std::max(cells, 0.0f) + 0.15f * beyond * std::sqrt(beyond);
}

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

namespace
{
/// The shore map and the cliffs of a sea bed whose depths are made
void BuildShore(seabed::Seabed& seabed, std::span<const uint8_t> altitudes, const std::vector<bool>& land,
                const std::vector<float>& toLand)
{
	using namespace seabed;
	const auto size = seabed.size;
	const auto count = land.size();
	const auto at = [&size](int x, int z) {
		return static_cast<size_t>(z) * static_cast<size_t>(size.x) + static_cast<size_t>(x);
	};
	std::vector<bool> sea(count);
	for (size_t i = 0; i < count; ++i)
	{
		sea[i] = !land[i];
	}
	const auto toSea = DistanceTo(sea, size);
	// The distance to the coast, the waterline half a cell between the last land and the first sea
	std::vector<float> distance(count);
	for (size_t i = 0; i < count; ++i)
	{
		distance[i] = land[i] ? -(std::min(toSea[i], 1000.0f) - 0.5f) * seabed.spacing
		                      : (std::min(toLand[i], 1000.0f) - 0.5f) * seabed.spacing;
	}
	seabed.shore.resize(count);
	seabed.cliff.assign(count, 0.0f);
	seabed.cliffFoot.clear();
	for (int z = 0; z < size.y; ++z)
	{
		for (int x = 0; x < size.x; ++x)
		{
			const auto d = [&](int px, int pz) {
				return distance[at(std::clamp(px, 0, size.x - 1), std::clamp(pz, 0, size.y - 1))];
			};
			const glm::vec2 rise {d(x + 1, z) - d(x - 1, z), d(x, z + 1) - d(x, z - 1)};
			const auto towards = glm::length(rise) > 1e-4f ? -glm::normalize(rise) : glm::vec2(0.0f);
			const auto i = at(x, z);
			const float ground = land[i] ? static_cast<float>(altitudes[i]) * 0.67f : -seabed.depth[i];
			seabed.shore[i] = {distance[i], towards, ground};
		}
	}
	// The cliffs: coast land that rises steeply within two cells, with deep water next to it
	for (int z = 0; z < size.y; ++z)
	{
		for (int x = 0; x < size.x; ++x)
		{
			if (!land[at(x, z)])
			{
				continue;
			}
			uint8_t highest = 0;
			float deepest = 0.0f;
			glm::ivec2 seaSide {-1, -1};
			for (int dz = -2; dz <= 2; ++dz)
			{
				for (int dx = -2; dx <= 2; ++dx)
				{
					const int nx = std::clamp(x + dx, 0, size.x - 1);
					const int nz = std::clamp(z + dz, 0, size.y - 1);
					highest = std::max(highest, altitudes[at(nx, nz)]);
					if (!land[at(nx, nz)])
					{
						deepest = std::max(deepest, seabed.depth[at(nx, nz)]);
						if (std::abs(dx) + std::abs(dz) == 1)
						{
							seaSide = {nx, nz};
						}
					}
				}
			}
			if (seaSide.x < 0)
			{
				continue;
			}
			const float rise = static_cast<float>(highest) - static_cast<float>(k_SeaLevelAltitude);
			const float steep = std::clamp((rise - k_CliffRiseStart) / (k_CliffRiseFull - k_CliffRiseStart), 0.0f, 1.0f);
			const float weight = steep * std::clamp((deepest - 2.0f) / 3.0f, 0.0f, 1.0f);
			if (weight <= 0.0f)
			{
				continue;
			}
			// The foot of the cliff reaches out over the water round it
			const auto reach = static_cast<int>(k_CliffReach);
			for (int dz = -reach; dz <= reach; ++dz)
			{
				for (int dx = -reach; dx <= reach; ++dx)
				{
					const int nx = x + dx;
					const int nz = z + dz;
					if (nx < 0 || nz < 0 || nx >= size.x || nz >= size.y)
					{
						continue;
					}
					const float fall = 1.0f - std::sqrt(static_cast<float>(dx * dx + dz * dz)) / (k_CliffReach + 1.0f);
					auto& cliff = seabed.cliff[at(nx, nz)];
					cliff = std::max(cliff, weight * std::max(fall, 0.0f));
				}
			}
			// Every other foot point along the cliff, on the sea's side, facing out to sea
			if (weight > 0.5f && ((x + z) % 2) == 0)
			{
				const auto& toCoast = seabed.shore[at(seaSide.x, seaSide.y)];
				seabed.cliffFoot.emplace_back(static_cast<float>(seaSide.x) * seabed.spacing + seabed.origin.x,
				                              static_cast<float>(seaSide.y) * seabed.spacing + seabed.origin.y, -toCoast.y,
				                              -toCoast.z);
			}
		}
	}
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
	// The coasts that slope gently: land next to the sea that rises little over the two cells round it
	std::vector<bool> gentle(count, false);
	for (int z = 0; z < size.y; ++z)
	{
		for (int x = 0; x < size.x; ++x)
		{
			if (!land[at(x, z)])
			{
				continue;
			}
			bool coast = false;
			uint8_t highest = 0;
			for (int dz = -2; dz <= 2; ++dz)
			{
				for (int dx = -2; dx <= 2; ++dx)
				{
					const int nx = std::clamp(x + dx, 0, size.x - 1);
					const int nz = std::clamp(z + dz, 0, size.y - 1);
					highest = std::max(highest, altitudes[at(nx, nz)]);
					coast = coast || (std::abs(dx) + std::abs(dz) == 1 && !land[at(nx, nz)]);
				}
			}
			gentle[at(x, z)] = coast && highest <= k_SeaLevelAltitude + k_GentleRise;
		}
	}
	const auto toGentle = DistanceTo(gentle, size);
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
			// Out from a gentle coast the land goes on under the water as a long beach, so the tide has room to move
			const float beachShare = 1.0f - std::clamp((toGentle[at(x, z)] - cells - 1.0f) / 3.0f, 0.0f, 1.0f);
			depth = glm::mix(depth, std::min(depth, BeachDepth(cells)), beachShare);
			// Deeper again towards the map's edge, away from the coast
			const auto edge = static_cast<float>(std::min(std::min(x, z), std::min(size.x - 1 - x, size.y - 1 - z)));
			const float towardsEdge = 1.0f - std::clamp(edge / 80.0f, 0.0f, 1.0f);
			depth += k_EdgeDepth * towardsEdge * towardsEdge * std::clamp((cells - 10.0f) / 30.0f, 0.0f, 1.0f);
			// The hills and dips: none at the coast, and larger in deeper water
			const float amplitude = std::min(cells / 4.0f, 1.0f) * (1.5f + depth * 0.1f) * (1.0f - beachShare * 0.7f);
			depth += Hills(x, z) * amplitude;
			seabed.depth[at(x, z)] = std::clamp(depth, 0.5f, k_MaxDepth);
		}
	}
	BuildShore(seabed, altitudes, land, toLand);
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

std::vector<seabed::ReefPlant> seabed::PlaceReef(const Seabed& seabed, float lowestTide, size_t mostCoral, size_t mostPlants)
{
	std::vector<ReefPlant> plants;
	if (seabed.size.x <= 1 || seabed.size.y <= 1)
	{
		return plants;
	}
	constexpr float k_Step = 4.0f;
	size_t coral = 0;
	size_t weed = 0;
	const glm::vec2 extent = glm::vec2(seabed.size - 1) * seabed.spacing;
	for (float z = 0.0f; z < extent.y; z += k_Step)
	{
		for (float x = 0.0f; x < extent.x; x += k_Step)
		{
			const auto ix = static_cast<int>(x / k_Step);
			const auto iz = static_cast<int>(z / k_Step);
			// A point a little off the grid, the same each time
			const glm::vec2 at =
			    seabed.origin + glm::vec2(x, z) + glm::vec2(Hash(ix, iz * 7 + 1) - 0.5f, Hash(ix * 3 + 2, iz) - 0.5f) * k_Step;
			const float depth = seabed.DepthAt(at);
			const float underTide = depth - lowestTide;
			if (depth <= 0.0f || underTide < k_PlantTopUnderTide || underTide > k_PlantBottomUnderTide)
			{
				continue;
			}
			const float dx = seabed.DepthAt(at + glm::vec2(2.0f, 0.0f)) - seabed.DepthAt(at - glm::vec2(2.0f, 0.0f));
			const float dz = seabed.DepthAt(at + glm::vec2(0.0f, 2.0f)) - seabed.DepthAt(at - glm::vec2(0.0f, 2.0f));
			if (glm::length(glm::vec2(dx, dz)) / 4.0f > k_ReefSteepest)
			{
				continue;
			}
			const float roll = Hash(ix * 13 + 5, iz * 17 + 3);
			const float tint = Hash(ix * 5 + 11, iz * 3 + 7);
			const float yaw = Hash(ix + 19, iz * 11 + 2) * 6.2831853f;
			const glm::vec3 foot {at.x, -depth, at.y};
			// The reef's patches: a broad noise and a finer one, so the coral grows in clumps
			const float reef =
			    ValueNoise(at.x / 10.0f, at.y / 10.0f, 14.0f) * 0.7f + ValueNoise(at.x / 10.0f, at.y / 10.0f, 4.0f) * 0.3f;
			if (coral < mostCoral && underTide >= k_CoralTopUnderTide && underTide <= k_CoralBottomUnderTide && reef > 0.55f &&
			    roll < (reef - 0.55f) * 4.5f)
			{
				const float variant = std::floor(Hash(ix * 23 + 1, iz * 29 + 4) * 9.0f);
				plants.push_back({.kind = ReefPlant::Kind::Coral,
				                  .position = foot,
				                  .scale = 1.2f + 2.2f * Hash(ix * 31 + 6, iz * 7 + 9),
				                  .yaw = yaw,
				                  .variant = std::min(variant, 8.0f),
				                  .tint = tint});
				++coral;
				continue;
			}
			const float meadow = ValueNoise(at.x / 10.0f + 50.0f, at.y / 10.0f, 10.0f);
			if (weed < mostPlants && meadow > 0.58f && roll < (meadow - 0.58f) * 2.5f)
			{
				plants.push_back({.kind = ReefPlant::Kind::Seaweed,
				                  .position = foot,
				                  .scale = 1.5f + 3.0f * Hash(ix * 37 + 8, iz * 13 + 1) * std::min(underTide / 6.0f, 1.0f),
				                  .yaw = yaw,
				                  .variant = Hash(ix * 41 + 3, iz * 19 + 5),
				                  .tint = tint});
				++weed;
			}
		}
	}
	return plants;
}
