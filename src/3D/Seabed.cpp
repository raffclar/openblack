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
#include <array>
#include <iterator>
#include <limits>
#include <optional>
#include <utility>

#include <LNDFile.h>
#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include "Creature/CreatureRoute.h"
#include "Enums.h"

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

float seabed::ShelfFromCoastLine(float from, float slope)
{
	if (from <= 0.0f)
	{
		return 0.0f;
	}
	// A cliff falls at its own slope from the line
	const float steep = std::clamp((slope - k_GentleMost) / 0.7f, 0.0f, 1.0f);
	// A gentle coast goes down to the depth that only covers a creature's feet, and then its slope grows to the coast's
	// own over k_ShallowsEase units
	constexpr float k_Gentle = k_WadeDepth / k_ShallowsWidth;
	const float grow = std::max(slope - k_Gentle, 0.0f);
	const float past = std::max(from - k_ShallowsWidth, 0.0f);
	const float easing = std::min(past, k_ShallowsEase);
	const float gentle =
	    from * k_Gentle + grow * (0.5f * easing * easing / k_ShallowsEase + std::max(past - k_ShallowsEase, 0.0f));
	return glm::mix(gentle, from * slope, steep);
}

float seabed::BeachShare(const lnd::LNDCountry& country, uint8_t altitude, std::span<const uint16_t> materialTypes)
{
	const auto& entry = country.materials.at(altitude);
	const auto sand = [&materialTypes](uint32_t material) {
		if (material >= materialTypes.size())
		{
			return 0.0f;
		}
		const auto type = static_cast<TerrainMaterialType>(materialTypes[material]);
		return type == TerrainMaterialType::Sand || type == TerrainMaterialType::WetSand ? 1.0f : 0.0f;
	};
	// The first material weighs the coefficient out of 256, the second the rest
	const float first = static_cast<float>(std::min<uint32_t>(entry.coefficient, 256u)) / 256.0f;
	return sand(entry.indices[0]) * first + sand(entry.indices[1]) * (1.0f - first);
}

bool seabed::Walkable(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
	const auto [low, high] = std::minmax({a, b, c, d});
	const float rise = static_cast<float>(high - low) * k_HeightUnit;
	return high > 0 && rise <= creature_route::k_SteepRise;
}

std::vector<bool> seabed::WalkableCells(std::span<const uint8_t> altitudes, glm::ivec2 size)
{
	const glm::ivec2 cells = glm::max(size - 1, glm::ivec2(0));
	const auto count = static_cast<size_t>(cells.x) * static_cast<size_t>(cells.y);
	std::vector<bool> walkable(count, false);
	if (count == 0 || altitudes.size() < static_cast<size_t>(size.x) * static_cast<size_t>(size.y))
	{
		return walkable;
	}
	const auto corner = [&](int x, int z) {
		return altitudes[static_cast<size_t>(z) * static_cast<size_t>(size.x) + static_cast<size_t>(x)];
	};
	const auto cell = [&cells](int x, int z) {
		return static_cast<size_t>(z) * static_cast<size_t>(cells.x) + static_cast<size_t>(x);
	};
	std::vector<bool> open(count, false);
	for (int z = 0; z < cells.y; ++z)
	{
		for (int x = 0; x < cells.x; ++x)
		{
			open[cell(x, z)] = Walkable(corner(x, z), corner(x + 1, z), corner(x, z + 1), corner(x + 1, z + 1));
		}
	}
	// The regions of open cells joined side by side; the largest is the land that a creature walks on
	std::vector<int32_t> region(count, -1);
	std::vector<size_t> sizes;
	std::vector<size_t> stack;
	for (size_t start = 0; start < count; ++start)
	{
		if (!open[start] || region[start] >= 0)
		{
			continue;
		}
		const auto label = static_cast<int32_t>(sizes.size());
		size_t found = 0;
		region[start] = label;
		stack.push_back(start);
		while (!stack.empty())
		{
			const auto at = stack.back();
			stack.pop_back();
			++found;
			const auto x = static_cast<int>(at % static_cast<size_t>(cells.x));
			const auto z = static_cast<int>(at / static_cast<size_t>(cells.x));
			for (const auto& [dx, dz] : std::array<std::pair<int, int>, 4> {{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}})
			{
				const int nx = x + dx;
				const int nz = z + dz;
				if (nx < 0 || nz < 0 || nx >= cells.x || nz >= cells.y)
				{
					continue;
				}
				const auto next = cell(nx, nz);
				if (open[next] && region[next] < 0)
				{
					region[next] = label;
					stack.push_back(next);
				}
			}
		}
		sizes.push_back(found);
	}
	if (sizes.empty())
	{
		return walkable;
	}
	const auto largest = static_cast<int32_t>(std::distance(sizes.begin(), std::ranges::max_element(sizes)));
	std::ranges::transform(region, walkable.begin(), [largest](int32_t label) { return label == largest; });
	return walkable;
}

namespace
{
/// The distance of each corner to the nearest source corner, in cells. Each corner keeps its nearest source, and a
/// pass forwards, then backwards, gives each corner the nearest source of its neighbours. The distance is the straight
/// line to that source, so the lines of equal distance are round and do not have the corners of an octagon.
std::vector<float> DistanceTo(const std::vector<bool>& sources, glm::ivec2 size, std::vector<int32_t>* nearest = nullptr)
{
	constexpr float k_Far = std::numeric_limits<float>::max() / 4.0f;
	std::vector<float> distance(sources.size(), k_Far);
	std::vector<int32_t> from(sources.size(), -1);
	const auto at = [&size](int x, int z) {
		return static_cast<size_t>(z) * static_cast<size_t>(size.x) + static_cast<size_t>(x);
	};
	for (size_t i = 0; i < sources.size(); ++i)
	{
		if (sources[i])
		{
			distance[i] = 0.0f;
			from[i] = static_cast<int32_t>(i);
		}
	}
	const auto relax = [&](int x, int z, int dx, int dz) {
		const int nx = x + dx;
		const int nz = z + dz;
		if (nx < 0 || nz < 0 || nx >= size.x || nz >= size.y)
		{
			return;
		}
		const int32_t source = from[at(nx, nz)];
		if (source < 0)
		{
			return;
		}
		const float sx = static_cast<float>(source % size.x) - static_cast<float>(x);
		const float sz = static_cast<float>(source / size.x) - static_cast<float>(z);
		const float candidate = std::sqrt(sx * sx + sz * sz);
		if (candidate < distance[at(x, z)])
		{
			distance[at(x, z)] = candidate;
			from[at(x, z)] = source;
		}
	};
	for (int z = 0; z < size.y; ++z)
	{
		for (int x = 0; x < size.x; ++x)
		{
			relax(x, z, -1, 0);
			relax(x, z, 0, -1);
			relax(x, z, -1, -1);
			relax(x, z, 1, -1);
		}
		for (int x = size.x - 1; x >= 0; --x)
		{
			relax(x, z, 1, 0);
		}
	}
	for (int z = size.y - 1; z >= 0; --z)
	{
		for (int x = size.x - 1; x >= 0; --x)
		{
			relax(x, z, 1, 0);
			relax(x, z, 0, 1);
			relax(x, z, 1, 1);
			relax(x, z, -1, 1);
		}
		for (int x = 0; x < size.x; ++x)
		{
			relax(x, z, -1, 0);
		}
	}
	if (nearest != nullptr)
	{
		*nearest = std::move(from);
	}
	return distance;
}
} // namespace

namespace
{
void SmoothField(std::vector<float>& field, glm::ivec2 size, int radius, int passes);

/// Rounds the lines of equal distance to the coast out at sea. The lines are the crests of the shore wave. Where the
/// distances to two parts of the coast are equal, the lines have a sharp bend. A blur rounds these bends. The blur
/// gets wider with the distance, so the distance at the coast does not change. `distance` is in units, out to sea
/// positive.
void RoundFarFromCoast(std::vector<float>& distance, glm::ivec2 size, float spacing)
{
	constexpr std::array<int, 4> k_Radii {1, 2, 4, 8};
	// The blur's radius is a third of the distance: its reach (two passes) stays short of the coast
	constexpr float k_RadiusForDistance = 1.0f / 3.0f;
	std::array<std::vector<float>, k_Radii.size()> blurred;
	for (size_t level = 0; level < k_Radii.size(); ++level)
	{
		blurred.at(level) = distance;
		SmoothField(blurred.at(level), size, k_Radii.at(level), 2);
	}
	for (size_t i = 0; i < distance.size(); ++i)
	{
		const float radius = std::clamp(distance[i] / spacing * k_RadiusForDistance, 0.0f, 8.0f);
		if (radius <= 0.0f)
		{
			continue;
		}
		float lower = 0.0f;
		float value = distance[i];
		for (size_t level = 0; level < k_Radii.size(); ++level)
		{
			const auto upper = static_cast<float>(k_Radii.at(level));
			const float below = level == 0 ? distance[i] : blurred.at(level - 1)[i];
			if (radius <= upper)
			{
				value = glm::mix(below, blurred.at(level)[i], (radius - lower) / (upper - lower));
				break;
			}
			lower = upper;
		}
		distance[i] = value;
	}
}
} // namespace

namespace
{
/// The shore map and the cliffs of a sea bed whose depths are made
void BuildShore(seabed::Seabed& seabed, std::span<const uint8_t> altitudes, const std::vector<bool>& land,
                const std::vector<float>& toLand, const std::vector<float>& openDepth)
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
	RoundFarFromCoast(distance, size, seabed.spacing);
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
	// The cliffs: coast land that rises sharply within one cell, with deep water next to it
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
					// Only the land within one cell counts: a sharp cliff rises at once from the water
					if (std::abs(dx) <= 1 && std::abs(dz) <= 1)
					{
						highest = std::max(highest, altitudes[at(nx, nz)]);
					}
					if (!land[at(nx, nz)])
					{
						deepest = std::max(deepest, openDepth[at(nx, nz)]);
						if (std::abs(dx) + std::abs(dz) == 1)
						{
							seaSide = {nx, nz};
						}
					}
				}
			}
			if (seaSide.x < 0 || (!seabed.pond.empty() && seabed.pond[at(seaSide.x, seaSide.y)] > 0.5f))
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
/// The distance of each corner to the line where a field of corner altitudes crosses a level, in cells. The line
/// crosses each edge of the grid between two corners on its two sides, where the altitudes, blended along the edge,
/// are at the level. The corners next to the line take their distance to it along the edges, and the rest take it
/// from them, a chamfer pass forwards, then backwards.
std::vector<float> DistanceToLine(std::span<const uint8_t> altitudes, glm::ivec2 size, float level)
{
	constexpr float k_Far = std::numeric_limits<float>::max() / 4.0f;
	constexpr float k_Diagonal = 1.41421356f;
	const auto at = [&size](int x, int z) {
		return static_cast<size_t>(z) * static_cast<size_t>(size.x) + static_cast<size_t>(x);
	};
	std::vector<float> distance(static_cast<size_t>(size.x) * static_cast<size_t>(size.y), k_Far);
	const auto cross = [&](int ax, int az, int bx, int bz) {
		const auto a = static_cast<float>(altitudes[at(ax, az)]) - level;
		const auto b = static_cast<float>(altitudes[at(bx, bz)]) - level;
		if ((a < 0.0f) == (b < 0.0f))
		{
			return;
		}
		const float share = a / (a - b);
		distance[at(ax, az)] = std::min(distance[at(ax, az)], share);
		distance[at(bx, bz)] = std::min(distance[at(bx, bz)], 1.0f - share);
	};
	for (int z = 0; z < size.y; ++z)
	{
		for (int x = 0; x < size.x; ++x)
		{
			if (x + 1 < size.x)
			{
				cross(x, z, x + 1, z);
			}
			if (z + 1 < size.y)
			{
				cross(x, z, x, z + 1);
			}
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

/// A box blur of a field over the grid, `radius` corners each way, `passes` times, along x and then along z
void SmoothField(std::vector<float>& field, glm::ivec2 size, int radius, int passes)
{
	std::vector<float> scratch(field.size());
	const auto at = [&size](int x, int z) {
		return static_cast<size_t>(z) * static_cast<size_t>(size.x) + static_cast<size_t>(x);
	};
	for (int pass = 0; pass < passes; ++pass)
	{
		for (int z = 0; z < size.y; ++z)
		{
			for (int x = 0; x < size.x; ++x)
			{
				float sum = 0.0f;
				for (int k = -radius; k <= radius; ++k)
				{
					sum += field[at(std::clamp(x + k, 0, size.x - 1), z)];
				}
				scratch[at(x, z)] = sum / static_cast<float>(2 * radius + 1);
			}
		}
		for (int z = 0; z < size.y; ++z)
		{
			for (int x = 0; x < size.x; ++x)
			{
				float sum = 0.0f;
				for (int k = -radius; k <= radius; ++k)
				{
					sum += scratch[at(x, std::clamp(z + k, 0, size.y - 1))];
				}
				field[at(x, z)] = sum / static_cast<float>(2 * radius + 1);
			}
		}
	}
}
} // namespace

seabed::Seabed seabed::Build(std::span<const uint8_t> altitudes, glm::ivec2 size, glm::vec2 origin, float spacing,
                             std::span<const float> beach, float edgeRing)
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
	// The coast: land next to the sea. Each coast corner keeps its height and how steeply the land falls to the sea
	// there: across the land inland of it, or from its own height down to the waterline for a cliff.
	std::vector<bool> coast(count, false);
	std::vector<float> coastHeight(count, 0.0f);
	std::vector<float> coastSlope(count, 0.0f);
	for (int z = 0; z < size.y; ++z)
	{
		for (int x = 0; x < size.x; ++x)
		{
			if (!land[at(x, z)])
			{
				continue;
			}
			bool nextToSea = false;
			float highest = 0.0f;
			for (int dz = -2; dz <= 2; ++dz)
			{
				for (int dx = -2; dx <= 2; ++dx)
				{
					const int nx = std::clamp(x + dx, 0, size.x - 1);
					const int nz = std::clamp(z + dz, 0, size.y - 1);
					highest = std::max(highest, static_cast<float>(altitudes[at(nx, nz)]) * k_HeightUnit);
					nextToSea = nextToSea || (std::abs(dx) + std::abs(dz) == 1 && !land[at(nx, nz)]);
				}
			}
			if (!nextToSea)
			{
				continue;
			}
			const auto i = at(x, z);
			coast[i] = true;
			coastHeight[i] = static_cast<float>(altitudes[i]) * k_HeightUnit;
			const float inland = (highest - coastHeight[i]) / (2.0f * seabed.spacing);
			const float face = (coastHeight[i] - k_LowestLandHeight) / seabed.spacing;
			coastSlope[i] = std::clamp(std::max(inland, face), k_GentlestSlope, k_SteepestSlope);
			// A beach goes on down at its own shallow grade
			if (i < beach.size())
			{
				coastSlope[i] = glm::mix(coastSlope[i], k_BeachSlope, std::clamp(beach[i], 0.0f, 1.0f));
			}
		}
	}
	std::vector<int32_t> nearestCoast;
	const auto toCoast = DistanceTo(coast, size, &nearestCoast);
	// The slope that each corner takes from its nearest coast, smoothed along the coast, so that the sea bed does not
	// step where two stretches of coast of different slopes meet
	std::vector<float> slopeOf(count, k_GentlestSlope);
	std::vector<float> beachOf(count, 0.0f);
	for (size_t i = 0; i < count; ++i)
	{
		if (nearestCoast[i] >= 0)
		{
			const auto source = static_cast<size_t>(nearestCoast[i]);
			slopeOf[i] = coastSlope[source];
			beachOf[i] = source < beach.size() ? std::clamp(beach[source], 0.0f, 1.0f) : 0.0f;
		}
	}
	SmoothField(slopeOf, size, 4, 3);
	SmoothField(beachOf, size, 4, 3);
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
			// The open sea: a shelf, deeper towards the map's edge, with broad hills and dips
			float shelf = DepthForDistance(cells);
			const auto edge = static_cast<float>(std::min(std::min(x, z), std::min(size.x - 1 - x, size.y - 1 - z)));
			const float towardsEdge = 1.0f - std::clamp(edge / 80.0f, 0.0f, 1.0f);
			shelf += k_EdgeDepth * towardsEdge * towardsEdge * std::clamp((cells - 10.0f) / 30.0f, 0.0f, 1.0f);
			shelf += Hills(x, z) * (1.5f + shelf * 0.1f);
			// Near the coast the land's own slope goes on down under the water from the waterline: a beach keeps its
			// gentle grade and a cliff keeps falling. Further out it eases into the shelf: sooner for a cliff.
			const auto source = nearestCoast[at(x, z)];
			float depth = shelf;
			if (source >= 0)
			{
				const float from = std::min(toCoast[at(x, z)], 1000.0f) * seabed.spacing;
				const float slope = slopeOf[at(x, z)];
				const float steep = std::clamp((slope - 0.15f) / 1.0f, 0.0f, 1.0f);
				const float continued = slope * std::max(from - 0.5f * seabed.spacing, 0.0f);
				const float capped = std::min(continued, std::max(shelf, 1.0f) + 30.0f * steep + 3.0f);
				const float start = glm::mix(glm::mix(60.0f, 10.0f, steep), k_BeachEaseStart, beachOf[at(x, z)]);
				const float end = glm::mix(glm::mix(260.0f, 90.0f, steep), k_BeachEaseEnd, beachOf[at(x, z)]);
				const float t = std::clamp((from - start) / (end - start), 0.0f, 1.0f);
				depth = glm::mix(capped, shelf, t * t * (3.0f - 2.0f * t));
			}
			seabed.depth[at(x, z)] = std::clamp(depth, 0.15f, k_MaxDepth);
		}
	}
	// One soft pass over the sea, so no crease shows where the coast's slope eases into the shelf; the land stays
	auto softened = seabed.depth;
	SmoothField(softened, size, 1, 1);
	for (size_t i = 0; i < count; ++i)
	{
		if (!land[i] && seabed.depth[i] > 0.0f)
		{
			seabed.depth[i] = std::max(softened[i], 0.15f);
		}
	}
	// The depth before the shallows of the original coast, which finds the cliffs: deep water close to steep land
	const auto openDepth = seabed.depth;
	// The coast of the original game: the line where its land fades into its sea, at altitude k_OriginalCoast. The
	// sea bed goes down from that line by its distance, so the water's edge at high tide follows it, smooth across the
	// cells. Where a creature can walk, the water only covers its feet.
	const auto toCoastLine = DistanceToLine(altitudes, size, k_OriginalCoast);
	std::vector<bool> wading(count, false);
	const auto walkable = WalkableCells(altitudes, size);
	for (int z = 0; z < size.y; ++z)
	{
		for (int x = 0; x < size.x; ++x)
		{
			const auto i = at(x, z);
			if (land[i])
			{
				continue;
			}
			// On the land's side of the line the ground is at the water's edge, and on the sea's side it goes down
			const bool seaSide = static_cast<float>(altitudes[i]) < k_OriginalCoast;
			const float from = seaSide ? std::min(toCoastLine[i], 1000.0f) * seabed.spacing : 0.0f;
			const float slope = std::max(slopeOf[i], k_BeachSlope);
			const float shelf = ShelfFromCoastLine(from, slope);
			float depth = std::min(seabed.depth[i], shelf);
			// The four cells that share this corner
			bool wade = false;
			for (int cz = z - 1; cz <= z && !wade; ++cz)
			{
				for (int cx = x - 1; cx <= x && !wade; ++cx)
				{
					wade = cx >= 0 && cz >= 0 && cx + 1 < size.x && cz + 1 < size.y &&
					       walkable[static_cast<size_t>(cz) * static_cast<size_t>(size.x - 1) + static_cast<size_t>(cx)];
				}
			}
			if (wade)
			{
				depth = std::min(depth, k_WadeDepth);
				wading[i] = true;
			}
			seabed.depth[i] = std::max(depth, 0.0f);
		}
	}
	// Past the water a creature can walk in, the sea bed goes on down from it as from the end of the shallows, so that
	// no ledge shows at its edge
	const auto fromWading = DistanceTo(wading, size);
	for (size_t i = 0; i < count; ++i)
	{
		if (!land[i] && !wading[i] && fromWading[i] < 1000.0f)
		{
			// The slope of a gentle coast, so the ramp starts at the depth of the wading water
			const float ramp = ShelfFromCoastLine((fromWading[i] * seabed.spacing) + k_ShallowsWidth,
			                                      std::clamp(slopeOf[i], k_BeachSlope, k_GentleMost));
			seabed.depth[i] = std::min(seabed.depth[i], ramp);
		}
	}
	seabed.pond = Ponds(land, size);
	// Near the grid's edge the sea bed eases down to the depth of the far floor past it
	for (int z = 0; edgeRing > 0.0f && z < size.y; ++z)
	{
		for (int x = 0; x < size.x; ++x)
		{
			const auto i = at(x, z);
			if (land[i])
			{
				continue;
			}
			const auto edge = static_cast<float>(std::min(std::min(x, z), std::min(size.x - 1 - x, size.y - 1 - z)));
			const float t = 1.0f - std::clamp(edge / edgeRing, 0.0f, 1.0f);
			seabed.depth[i] = glm::mix(seabed.depth[i], k_MaxDepth, t * t * (3.0f - 2.0f * t));
		}
	}
	BuildShore(seabed, altitudes, land, toLand, openDepth);
	return seabed;
}

std::vector<float> seabed::Ponds(const std::vector<bool>& land, glm::ivec2 size)
{
	const auto count = static_cast<size_t>(std::max(size.x, 0)) * static_cast<size_t>(std::max(size.y, 0));
	std::vector<float> pond(count, 0.0f);
	if (count == 0 || land.size() < count)
	{
		return pond;
	}
	const auto at = [&size](int x, int z) {
		return static_cast<size_t>(z) * static_cast<size_t>(size.x) + static_cast<size_t>(x);
	};
	// The open sea: the water that joins the edge of the grid
	std::vector<bool> open(count, false);
	std::vector<glm::ivec2> next;
	for (int z = 0; z < size.y; ++z)
	{
		for (int x = 0; x < size.x; ++x)
		{
			const bool edge = x == 0 || z == 0 || x == size.x - 1 || z == size.y - 1;
			if (edge && !land[at(x, z)])
			{
				open[at(x, z)] = true;
				next.emplace_back(x, z);
			}
		}
	}
	constexpr std::array<glm::ivec2, 4> k_Sides {glm::ivec2 {1, 0}, glm::ivec2 {-1, 0}, glm::ivec2 {0, 1}, glm::ivec2 {0, -1}};
	while (!next.empty())
	{
		const auto here = next.back();
		next.pop_back();
		for (const auto& side : k_Sides)
		{
			const auto there = here + side;
			if (there.x < 0 || there.y < 0 || there.x >= size.x || there.y >= size.y)
			{
				continue;
			}
			const auto i = at(there.x, there.y);
			if (!land[i] && !open[i])
			{
				open[i] = true;
				next.push_back(there);
			}
		}
	}
	// The water that is not open sea is a pond; the land next to it takes the pond too, so its edge is still
	for (int z = 0; z < size.y; ++z)
	{
		for (int x = 0; x < size.x; ++x)
		{
			if (land[at(x, z)] || open[at(x, z)])
			{
				continue;
			}
			for (int dz = -1; dz <= 1; ++dz)
			{
				for (int dx = -1; dx <= 1; ++dx)
				{
					const int nx = x + dx;
					const int nz = z + dz;
					if (nx >= 0 && nz >= 0 && nx < size.x && nz < size.y && !open[at(nx, nz)])
					{
						pond[at(nx, nz)] = 1.0f;
					}
				}
			}
		}
	}
	SmoothField(pond, size, 1, 1);
	return pond;
}

float seabed::Seabed::DepthAt(glm::vec2 world) const
{
	// A point that is not a number has no depth
	if (size.x <= 0 || size.y <= 0 || depth.empty() || !std::isfinite(world.x) || !std::isfinite(world.y))
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
	// A pocket of the reef at the bed under the point, if the depth and the slope let it grow there
	const auto bedAt = [&seabed, lowestTide](glm::vec2 at, float top, float bottom) -> std::optional<float> {
		const float depth = seabed.DepthAt(at);
		const float underTide = depth - lowestTide;
		if (depth <= 0.0f || underTide < top || underTide > bottom)
		{
			return std::nullopt;
		}
		const float dx = seabed.DepthAt(at + glm::vec2(2.0f, 0.0f)) - seabed.DepthAt(at - glm::vec2(2.0f, 0.0f));
		const float dz = seabed.DepthAt(at + glm::vec2(0.0f, 2.0f)) - seabed.DepthAt(at - glm::vec2(0.0f, 2.0f));
		if (glm::length(glm::vec2(dx, dz)) / 4.0f > k_ReefSteepest)
		{
			return std::nullopt;
		}
		return depth;
	};

	// One pocket can start in each cell. The centre stays in the middle half of the cell, so two pockets never touch.
	struct Pocket
	{
		glm::ivec2 cell;
		glm::vec2 centre;
		float radius;
		bool coral;
		float order;
	};
	std::vector<Pocket> pockets;
	const glm::vec2 extent = glm::vec2(seabed.size - 1) * seabed.spacing;
	const auto cells = glm::ivec2(glm::ceil(extent / k_ReefPocketCell));
	for (int cz = 0; cz < cells.y; ++cz)
	{
		for (int cx = 0; cx < cells.x; ++cx)
		{
			if (Hash(cx * 71 + 3, cz * 53 + 11) > k_ReefPocketChance)
			{
				continue;
			}
			const glm::vec2 jitter {Hash(cx * 17 + 5, cz * 29 + 2), Hash(cx * 43 + 9, cz * 13 + 8)};
			const glm::vec2 centre = seabed.origin + (glm::vec2(cx, cz) + glm::vec2(0.25f) + jitter * 0.5f) * k_ReefPocketCell;
			const float size = Hash(cx * 59 + 1, cz * 31 + 6);
			if (bedAt(centre, k_CoralTopUnderTide, k_CoralBottomUnderTide))
			{
				pockets.push_back({.cell = {cx, cz},
				                   .centre = centre,
				                   .radius = glm::mix(k_CoralPocketLeast, k_CoralPocketMost, size),
				                   .coral = true,
				                   .order = Hash(cx * 7 + 13, cz * 97 + 4)});
			}
			else if (bedAt(centre, k_PlantTopUnderTide, k_PlantBottomUnderTide))
			{
				pockets.push_back({.cell = {cx, cz},
				                   .centre = centre,
				                   .radius = glm::mix(k_PlantPocketLeast, k_PlantPocketMost, size),
				                   .coral = false,
				                   .order = Hash(cx * 7 + 13, cz * 97 + 4)});
			}
		}
	}
	// The pockets fill in an order that the hash gives, so that the caps remove pockets from all of the map
	std::ranges::sort(pockets, {}, &Pocket::order);

	size_t coral = 0;
	size_t weed = 0;
	for (const auto& pocket : pockets)
	{
		const int cx = pocket.cell.x;
		const int cz = pocket.cell.y;
		// A roll from 0 to 1 for the item `item` of this pocket, with a salt for each value
		const auto roll = [cx, cz](int item, int salt) {
			return Hash(cx * 131 + item * 37 + salt, cz * 113 + item * 19 + salt * 7);
		};
		if (pocket.coral)
		{
			// Many coral close together: more of them near the centre, and the big ones in the middle
			const auto count = static_cast<int>(k_CoralPocketBase + pocket.radius * k_CoralPocketPerUnit);
			for (int i = 0; i < count && coral < mostCoral; ++i)
			{
				const float angle = roll(i, 1) * 6.2831853f;
				const float out = pocket.radius * std::pow(roll(i, 2), 0.7f);
				const glm::vec2 at = pocket.centre + out * glm::vec2(std::cos(angle), std::sin(angle));
				const auto depth = bedAt(at, k_CoralTopUnderTide, k_CoralBottomUnderTide);
				if (!depth)
				{
					continue;
				}
				const float underTide = *depth - lowestTide;
				const float middle = 1.0f - 0.5f * out / pocket.radius;
				plants.push_back({.kind = ReefPlant::Kind::Coral,
				                  .position = {at.x, -*depth, at.y},
				                  .scale = std::min((1.2f + 2.2f * roll(i, 3)) * middle + 0.6f, underTide - k_ReefClearance),
				                  .yaw = roll(i, 4) * 6.2831853f,
				                  .variant = std::min(std::floor(roll(i, 5) * 9.0f), 8.0f),
				                  .tint = roll(i, 6)});
				++coral;
			}
		}
		// The plants: a loose ring round each coral pocket, or a loose patch of their own where the coral cannot grow
		const float inner = pocket.coral ? pocket.radius : 0.0f;
		const float outer = pocket.coral ? pocket.radius + k_PlantRingWidth : pocket.radius;
		const auto count = static_cast<int>(k_PlantPocketBase + outer * k_PlantPocketPerUnit);
		for (int i = 0; i < count && weed < mostPlants; ++i)
		{
			const float angle = roll(i, 11) * 6.2831853f;
			const float out = std::sqrt(glm::mix(inner * inner, outer * outer, roll(i, 12)));
			const glm::vec2 at = pocket.centre + out * glm::vec2(std::cos(angle), std::sin(angle));
			const auto depth = bedAt(at, k_PlantTopUnderTide, k_PlantBottomUnderTide);
			if (!depth)
			{
				continue;
			}
			const float underTide = *depth - lowestTide;
			plants.push_back(
			    {.kind = ReefPlant::Kind::Seaweed,
			     .position = {at.x, -*depth, at.y},
			     .scale = std::min(1.5f + 3.0f * roll(i, 13) * std::min(underTide / 6.0f, 1.0f), underTide - k_ReefClearance),
			     .yaw = roll(i, 14) * 6.2831853f,
			     .variant = roll(i, 15),
			     .tint = roll(i, 16)});
			++weed;
		}
	}
	return plants;
}
