/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "landgen/LakeShore.h"

#include <cmath>

#include <algorithm>
#include <limits>

#include "landgen/Terrain.h"

using namespace openblack::landgen;
using namespace openblack::landgen::lakeshore;

namespace
{
constexpr float k_Far = std::numeric_limits<float>::max() / 4.0f;
constexpr float k_Diagonal = 1.41421356f;
/// The sea has its shore steps on this many corners from the water
constexpr auto k_ShoreRings = static_cast<float>(terrain::k_ShoreSteps.size());
/// The countries of the coast have wet sand up to this altitude
constexpr float k_WetSandTop = 8.0f;
} // namespace

Shores lakeshore::Find(const Ground& ground, std::span<const lakes::Lake> lakes, const biomes::Thresholds& thresholds)
{
	const int side = ground.altitudes.side;
	Shores shores {.shore = Grid<Shore>(side, Shore::Land), .distance = Grid<float>(side, k_Far)};
	if (lakes.empty())
	{
		return shores;
	}
	// The level of the nearest lake at each corner. The water of a lake is each corner of it at or under its level.
	Grid<float> level(side, 0.0f);
	for (const auto& lake : lakes)
	{
		for (const auto corner : lake.corners)
		{
			shores.shore.values[corner] = Shore::Bed;
			if (ground.altitudes.values[corner] <= lake.level)
			{
				shores.distance.values[corner] = 0.0f;
				level.values[corner] = lake.level;
			}
		}
	}
	// Two passes of a chamfer, as for the distance to the sea. Each corner also takes the level of the lake that gives
	// its distance.
	const auto relax = [&](int x, int z, int nx, int nz, float step) {
		if (!shores.distance.Contains(nx, nz))
		{
			return;
		}
		const float through = shores.distance.At(nx, nz) + step;
		if (through < shores.distance.At(x, z))
		{
			shores.distance.At(x, z) = through;
			level.At(x, z) = level.At(nx, nz);
		}
	};
	for (int x = 0; x < side; ++x)
	{
		for (int z = 0; z < side; ++z)
		{
			relax(x, z, x - 1, z, 1.0f);
			relax(x, z, x, z - 1, 1.0f);
			relax(x, z, x - 1, z - 1, k_Diagonal);
			relax(x, z, x - 1, z + 1, k_Diagonal);
		}
	}
	for (int x = side - 1; x >= 0; --x)
	{
		for (int z = side - 1; z >= 0; --z)
		{
			relax(x, z, x + 1, z, 1.0f);
			relax(x, z, x, z + 1, 1.0f);
			relax(x, z, x + 1, z + 1, k_Diagonal);
			relax(x, z, x + 1, z - 1, k_Diagonal);
		}
	}

	// The sea beach goes up to the beach altitude, and the game draws the sea at the sea level of the thresholds. A lake
	// beach goes up the same height over the level of its lake.
	const float highest = thresholds.beachAltitude - thresholds.seaLevel;
	for (size_t i = 0; i < shores.shore.values.size(); ++i)
	{
		if (shores.shore.values[i] != Shore::Land)
		{
			continue;
		}
		const float distance = shores.distance.values[i];
		const float altitude = ground.altitudes.values[i];
		const float above = altitude - level.values[i];
		if (distance <= ground.beachWidths.values[i] && above >= 0.0f && above < highest &&
		    ground.slopes.values[i] < thresholds.beachSlope && ground.temperature.values[i] > thresholds.beachTemperature &&
		    ground.temperature.values[i] >= thresholds.nordicTemperature && altitude < thresholds.alpineAltitude)
		{
			shores.shore.values[i] = Shore::Beach;
		}
		else if (distance <= k_ShoreRings && above >= 0.0f && above < k_WetSandTop - thresholds.seaLevel)
		{
			shores.shore.values[i] = Shore::Bed;
		}
	}
	return shores;
}

namespace
{
/// The height of the shore steps of the sea over the sea level at a distance from the water, but never under it
float StepOver(float distance)
{
	const auto& steps = terrain::k_ShoreSteps;
	float step = steps.back();
	if (distance <= 1.0f)
	{
		step = steps.front();
	}
	else if (distance < k_ShoreRings)
	{
		const auto below = static_cast<size_t>(distance);
		const float along = distance - static_cast<float>(below);
		step = steps.at(below - 1) + ((steps.at(below) - steps.at(below - 1)) * along);
	}
	return std::max(step - terrain::k_SeaLevel, 0.0f);
}

/// The distance in corners from the water of one lake, in a box round the lake. The box holds each corner that the
/// rule can lower.
struct LakeDistance
{
	int x0;
	int z0;
	int width;
	int depth;
	std::vector<float> distance;

	[[nodiscard]] float At(int x, int z) const { return distance[static_cast<size_t>((x - x0) * depth + (z - z0))]; }
};

LakeDistance DistanceOf(const Grid<float>& heights, const lakes::Lake& lake, int reach)
{
	const int side = heights.side;
	int lowX = side;
	int lowZ = side;
	int highX = -1;
	int highZ = -1;
	for (const auto corner : lake.corners)
	{
		const int x = static_cast<int>(corner) / side;
		const int z = static_cast<int>(corner) % side;
		lowX = std::min(lowX, x);
		lowZ = std::min(lowZ, z);
		highX = std::max(highX, x);
		highZ = std::max(highZ, z);
	}
	LakeDistance box {.x0 = std::max(lowX - reach, 0), .z0 = std::max(lowZ - reach, 0), .width = 0, .depth = 0, .distance = {}};
	box.width = std::min(highX + reach, side - 1) - box.x0 + 1;
	box.depth = std::min(highZ + reach, side - 1) - box.z0 + 1;
	box.distance.assign(static_cast<size_t>(box.width * box.depth), k_Far);
	const auto local = [&box](int x, int z) -> float& { return box.distance[static_cast<size_t>(x * box.depth + z)]; };
	for (const auto corner : lake.corners)
	{
		if (heights.values[corner] <= lake.level)
		{
			local((static_cast<int>(corner) / side) - box.x0, (static_cast<int>(corner) % side) - box.z0) = 0.0f;
		}
	}
	// Two passes of a chamfer, as for the distance to the sea
	const auto relax = [&](int x, int z, int nx, int nz, float step) {
		if (nx < 0 || nz < 0 || nx >= box.width || nz >= box.depth)
		{
			return;
		}
		local(x, z) = std::min(local(x, z), local(nx, nz) + step);
	};
	for (int x = 0; x < box.width; ++x)
	{
		for (int z = 0; z < box.depth; ++z)
		{
			relax(x, z, x - 1, z, 1.0f);
			relax(x, z, x, z - 1, 1.0f);
			relax(x, z, x - 1, z - 1, k_Diagonal);
			relax(x, z, x - 1, z + 1, k_Diagonal);
		}
	}
	for (int x = box.width - 1; x >= 0; --x)
	{
		for (int z = box.depth - 1; z >= 0; --z)
		{
			relax(x, z, x + 1, z, 1.0f);
			relax(x, z, x, z + 1, 1.0f);
			relax(x, z, x + 1, z + 1, k_Diagonal);
			relax(x, z, x + 1, z - 1, k_Diagonal);
		}
	}
	return box;
}
} // namespace

std::vector<uint32_t> lakeshore::ShoreSteps(Grid<float>& heights, std::span<const lakes::Lake> lakes,
                                            std::span<const lakes::Keep> keep, const StepSettings& settings)
{
	std::vector<uint32_t> lowered;
	if (lakes.empty() || heights.values.empty() || settings.rise <= 0.0f)
	{
		return lowered;
	}
	const int side = heights.side;
	const float highest = std::ranges::max(heights.values);
	// The corners of the lakes and of the places to keep do not change. The highest that each corner can be, from the nearest
	// lake, and the lowest, from the shores of all the lakes near it
	Grid<float> ceiling(side, k_Far);
	Grid<float> bottom(side, -k_Far);
	Grid<uint8_t> fixed(side, 0);
	for (const auto& lake : lakes)
	{
		for (const auto corner : lake.corners)
		{
			fixed.values[corner] = 1;
		}
	}
	for (const auto& place : keep)
	{
		const auto reach = static_cast<int>(std::ceil(place.radius));
		const auto cx = static_cast<int>(std::floor(place.x + 0.5f));
		const auto cz = static_cast<int>(std::floor(place.z + 0.5f));
		for (int x = cx - reach; x <= cx + reach; ++x)
		{
			for (int z = cz - reach; z <= cz + reach; ++z)
			{
				const auto dx = static_cast<float>(x) - place.x;
				const auto dz = static_cast<float>(z) - place.z;
				if (fixed.Contains(x, z) && (dx * dx) + (dz * dz) <= place.radius * place.radius)
				{
					fixed.At(x, z) = 1;
				}
			}
		}
	}
	for (const auto& lake : lakes)
	{
		if (lake.corners.empty())
		{
			continue;
		}
		const float shore = lake.level + settings.freeboard;
		const int reach = static_cast<int>(k_ShoreRings +
		                                   std::ceil(std::max(highest - shore - StepOver(k_ShoreRings), 0.0f) / settings.rise));
		const auto box = DistanceOf(heights, lake, reach);
		for (int x = box.x0; x < box.x0 + box.width; ++x)
		{
			for (int z = box.z0; z < box.z0 + box.depth; ++z)
			{
				const float distance = box.At(x, z);
				if (distance >= k_Far)
				{
					continue;
				}
				const float steps = distance <= k_ShoreRings
				                        ? StepOver(distance)
				                        : StepOver(k_ShoreRings) + (settings.rise * (distance - k_ShoreRings));
				ceiling.At(x, z) = std::min(ceiling.At(x, z), shore + steps);
				if (distance <= k_ShoreRings)
				{
					bottom.At(x, z) = std::max(bottom.At(x, z), shore);
				}
			}
		}
	}
	for (size_t i = 0; i < heights.values.size(); ++i)
	{
		const float top = std::max(ceiling.values[i], bottom.values[i]);
		if (fixed.values[i] == 0 && heights.values[i] > top)
		{
			heights.values[i] = top;
			lowered.push_back(static_cast<uint32_t>(i));
		}
	}
	return lowered;
}
