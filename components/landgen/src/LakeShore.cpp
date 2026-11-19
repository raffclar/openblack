/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "landgen/LakeShore.h"

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
