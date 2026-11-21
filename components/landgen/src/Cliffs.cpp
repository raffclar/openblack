/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "landgen/Cliffs.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <vector>

#include "landgen/Noise.h"
#include "landgen/Random.h"

using namespace openblack::landgen;

namespace
{
constexpr float k_MetresPerCorner = 10.0f;
constexpr float k_Diagonal = 1.41421356f;

/// A corner next to a corner, and the distance to it in corners
struct Near
{
	int dx;
	int dz;
	float distance;
};

// The pass along the map looks at the corners that it already went over: those before the corner. The pass back looks
// at the corners after it.
constexpr std::array<Near, 4> k_Before {{{-1, -1, k_Diagonal}, {-1, 0, 1.0f}, {-1, 1, k_Diagonal}, {0, -1, 1.0f}}};
constexpr std::array<Near, 4> k_After {{{1, 1, k_Diagonal}, {1, 0, 1.0f}, {1, -1, k_Diagonal}, {0, 1, 1.0f}}};

/// The highest value in the square of each corner, reach corners to each side
std::vector<float> HighestNear(const Grid<float>& grid, std::span<const float> values, int reach)
{
	const int side = grid.side;
	std::vector<float> along(values.size());
	std::vector<float> highest(values.size());
	for (int x = 0; x < side; ++x)
	{
		for (int z = 0; z < side; ++z)
		{
			float top = values[grid.Index(x, z)];
			for (int k = std::max(z - reach, 0); k <= std::min(z + reach, side - 1); ++k)
			{
				top = std::max(top, values[grid.Index(x, k)]);
			}
			along[grid.Index(x, z)] = top;
		}
	}
	for (int x = 0; x < side; ++x)
	{
		for (int z = 0; z < side; ++z)
		{
			float top = along[grid.Index(x, z)];
			for (int k = std::max(x - reach, 0); k <= std::min(x + reach, side - 1); ++k)
			{
				top = std::max(top, along[grid.Index(k, z)]);
			}
			highest[grid.Index(x, z)] = top;
		}
	}
	return highest;
}

/// Finds the summits that the limit would cut down: each corner that is the highest in its square and higher than a
/// corner next to it by more than the limit. The ground round each summit goes up to a cone at the limit of the summit,
/// so that the summit stays and its faces keep to that limit. The result has 1 for each summit and each corner of its
/// cone, which the limit does not cut down. A fixed corner does not go up.
std::vector<uint8_t> KeepSummits(const Grid<float>& grid, std::vector<float>& units, std::span<const float> limit,
                                 std::span<const uint8_t> sea, std::span<const uint8_t> fixed,
                                 const cliffs::FaceSettings& settings)
{
	const int side = grid.side;
	const int reach = std::max(settings.summitReach, 1);
	std::vector<uint8_t> summits(units.size(), 0);
	const auto highest = HighestNear(grid, units, reach);
	std::vector<uint32_t> found;
	for (int x = 0; x < side; ++x)
	{
		for (int z = 0; z < side; ++z)
		{
			const auto index = grid.Index(x, z);
			if (sea[index] != 0 || units[index] < highest[index])
			{
				continue;
			}
			bool higher = true;
			bool cut = false;
			for (const auto& near : k_Before)
			{
				for (const int sign : {1, -1})
				{
					const int nx = x + (sign * near.dx);
					const int nz = z + (sign * near.dz);
					if (!grid.Contains(nx, nz))
					{
						continue;
					}
					const float other = units[grid.Index(nx, nz)];
					higher = higher && other < units[index];
					cut = cut || units[index] > other + (limit[index] * near.distance);
				}
			}
			if (higher && cut)
			{
				summits[index] = 1;
				found.push_back(static_cast<uint32_t>(index));
			}
		}
	}
	const int coneReach = std::max(settings.coneReach, 1);
	for (const auto index : found)
	{
		const int x = grid.XOf(index);
		const int z = grid.ZOf(index);
		const float top = units[index];
		const float coneRise = limit[index];
		for (int nx = std::max(x - coneReach, 0); nx <= std::min(x + coneReach, side - 1); ++nx)
		{
			for (int nz = std::max(z - coneReach, 0); nz <= std::min(z + coneReach, side - 1); ++nz)
			{
				const auto other = grid.Index(nx, nz);
				if (sea[other] != 0 || (!fixed.empty() && fixed[other] != 0))
				{
					continue;
				}
				const auto dx = static_cast<float>(nx - x);
				const auto dz = static_cast<float>(nz - z);
				const float cone = top - (coneRise * std::sqrt((dx * dx) + (dz * dz)));
				if (cone > units[other])
				{
					units[other] = cone;
					summits[other] = 1;
				}
			}
		}
	}
	return summits;
}
} // namespace

float cliffs::SteepestRiseAt(uint64_t seed, float u, float v, const FaceSettings& settings)
{
	const float wave = noise::Fractal(StageSeed(seed, 100), u * settings.frequency, v * settings.frequency, 2);
	return settings.steepestLow + ((settings.steepestHigh - settings.steepestLow) * noise::SmoothStep(-0.5f, 0.5f, wave));
}

void cliffs::LimitFaces(Grid<float>& heights, const HeightCurve& curve, float seaLevel, uint64_t seed,
                        const FaceSettings& settings, std::span<const uint8_t> fixed)
{
	const int side = heights.side;
	if (side < 2)
	{
		return;
	}
	const auto last = static_cast<float>(side - 1);
	// The work is in height units, so that a rise is the same on the low land and on the mountains
	std::vector<float> units(heights.values.size());
	std::vector<float> limit(heights.values.size());
	std::vector<uint8_t> sea(heights.values.size(), 0);

	for (int x = 0; x < side; ++x)
	{
		for (int z = 0; z < side; ++z)
		{
			const auto index = heights.Index(x, z);
			const float height = heights.values[index];
			sea[index] = height <= seaLevel ? 1 : 0;
			units[index] = curve.UnitsOf(height);
			const float rise = SteepestRiseAt(seed, static_cast<float>(x) / last, static_cast<float>(z) / last, settings);
			limit[index] = rise * k_MetresPerCorner / k_MetresPerUnit;
		}
	}
	const auto start = units;
	const float seaUnits = curve.UnitsOf(seaLevel);
	const auto summits = KeepSummits(heights, units, limit, sea, fixed, settings);
	const auto relax = [&](int x, int z, const auto& nears) {
		const auto index = heights.Index(x, z);
		if (sea[index] != 0 || summits[index] != 0)
		{
			return false;
		}
		float lowest = units[index];
		for (const auto& near : nears)
		{
			if (heights.Contains(x + near.dx, z + near.dz))
			{
				lowest = std::min(lowest, units[heights.Index(x + near.dx, z + near.dz)] + (limit[index] * near.distance));
			}
		}
		// A corner of the land stays above the sea
		lowest = std::max(lowest, seaUnits + 0.01f);
		if (lowest >= units[index] - 0.001f)
		{
			return false;
		}
		units[index] = lowest;
		return true;
	};
	for (int pass = 0; pass < settings.passes; ++pass)
	{
		bool changed = false;
		for (int x = 0; x < side; ++x)
		{
			for (int z = 0; z < side; ++z)
			{
				changed = relax(x, z, k_Before) || changed;
			}
		}
		for (int x = side - 1; x >= 0; --x)
		{
			for (int z = side - 1; z >= 0; --z)
			{
				changed = relax(x, z, k_After) || changed;
			}
		}
		if (!changed)
		{
			break;
		}
	}
	for (size_t i = 0; i < heights.values.size(); ++i)
	{
		if (units[i] != start[i])
		{
			heights.values[i] = curve.AltitudeOf(units[i]);
		}
	}
}
