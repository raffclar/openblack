/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "landgen/Biomes.h"

#include <array>
#include <utility>

using namespace openblack::landgen;
using namespace openblack::landgen::biomes;

Biome biomes::Classify(const Conditions& c, const Thresholds& t)
{
	if (c.altitude <= t.seaLevel)
	{
		return Biome::Sea;
	}
	if (c.altitude >= t.alpineAltitude)
	{
		return Biome::Alpine;
	}
	if (c.temperature < t.nordicTemperature)
	{
		return Biome::NordicWastes;
	}
	if (c.seaDistance <= c.beachWidth && c.altitude < t.beachAltitude && c.slope < t.beachSlope &&
	    c.temperature > t.beachTemperature)
	{
		return Biome::Beach;
	}
	if (c.wet || (c.moisture > t.swampMoisture && c.altitude < t.swampAltitude && c.slope < t.swampSlope))
	{
		return Biome::Swamp;
	}
	if (c.temperature > t.jungleTemperature && c.moisture > t.jungleMoisture)
	{
		return Biome::DenseJungle;
	}
	if (c.temperature > t.desertTemperature && c.moisture < t.desertMoisture)
	{
		return Biome::Desert;
	}
	if (c.blight > t.deadBlight && c.moisture < t.deadMoisture)
	{
		return Biome::DeadWoods;
	}
	if (c.moisture > t.forestMoisture)
	{
		return Biome::TemperateForest;
	}
	return Biome::Grassland;
}

Grid<Biome> biomes::Smooth(const Grid<Biome>& biomes, int passes)
{
	Grid<Biome> current = biomes;
	Grid<Biome> next = biomes;
	for (int pass = 0; pass < passes; ++pass)
	{
		for (int x = 0; x < current.side; ++x)
		{
			for (int z = 0; z < current.side; ++z)
			{
				// The sea follows the land's altitude, so it stays as it is
				const auto own = current.At(x, z);
				if (own == Biome::Sea)
				{
					next.At(x, z) = own;
					continue;
				}
				std::array<int, k_BiomeCount> counts {};
				for (int dx = -1; dx <= 1; ++dx)
				{
					for (int dz = -1; dz <= 1; ++dz)
					{
						if (current.Contains(x + dx, z + dz))
						{
							const auto biome = current.At(x + dx, z + dz);
							if (biome != Biome::Sea)
							{
								++counts[static_cast<size_t>(biome)];
							}
						}
					}
				}
				// The corner keeps its own biome when no other biome has more corners
				auto best = own;
				for (size_t i = 0; i < k_BiomeCount; ++i)
				{
					if (counts[i] > counts[static_cast<size_t>(best)])
					{
						best = static_cast<Biome>(i);
					}
				}
				// The mountains keep their snow: the altitude decides the alpine biome
				next.At(x, z) = (own == Biome::Alpine || best == Biome::Alpine) ? own : best;
			}
		}
		std::swap(current, next);
	}
	return current;
}
