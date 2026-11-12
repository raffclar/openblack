/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "landgen/Climate.h"

#include <algorithm>
#include <array>
#include <limits>

#include "landgen/Noise.h"
#include "landgen/Random.h"

using namespace openblack::landgen;
using namespace openblack::landgen::climate;

namespace
{
constexpr std::array<Step, 8> k_WindSteps {{{1, 0}, {1, -1}, {0, -1}, {-1, -1}, {-1, 0}, {-1, 1}, {0, 1}, {1, 1}}};

/// The mean of the values in a square of the given radius round each corner, along x then along z
Grid<float> Blur(const Grid<float>& values, int radius)
{
	Grid<float> along = values;
	Grid<float> result = values;
	const int side = values.side;
	for (int pass = 0; pass < 2; ++pass)
	{
		const auto& source = pass == 0 ? values : along;
		auto& target = pass == 0 ? along : result;
		for (int x = 0; x < side; ++x)
		{
			for (int z = 0; z < side; ++z)
			{
				float sum = 0.0f;
				int count = 0;
				for (int d = -radius; d <= radius; ++d)
				{
					const int sx = pass == 0 ? x + d : x;
					const int sz = pass == 0 ? z : z + d;
					if (source.Contains(sx, sz))
					{
						sum += source.At(sx, sz);
						++count;
					}
				}
				target.At(x, z) = sum / static_cast<float>(count);
			}
		}
	}
	return result;
}
} // namespace

Step climate::StepOf(Wind wind)
{
	return k_WindSteps[static_cast<size_t>(wind)];
}

Grid<float> climate::Temperature(const Grid<float>& heights, uint64_t seed, const Settings& settings)
{
	Grid<float> temperature(heights.side, 0.0f);
	const float last = static_cast<float>(std::max(heights.side - 1, 1));
	const auto variation = StageSeed(seed, 20);
	// A finer noise bends the lines between warm and cold land, so no biome ends on a straight line
	const auto edges = StageSeed(seed, 22);
	for (int x = 0; x < heights.side; ++x)
	{
		for (int z = 0; z < heights.side; ++z)
		{
			const float u = static_cast<float>(x) / last;
			const float v = static_cast<float>(z) / last;
			const float altitude = std::clamp(heights.At(x, z), 0.0f, 255.0f) / 255.0f;
			const float value = settings.northTemperature + ((settings.southTemperature - settings.northTemperature) * v) -
			                    (settings.heightCooling * altitude) +
			                    (settings.variation * noise::Fractal(variation, u * 4.0f, v * 4.0f, 3)) +
			                    (settings.edgeVariation * noise::Fractal(edges, u * 16.0f, v * 16.0f, 3));
			temperature.At(x, z) = std::clamp(value, 0.0f, 1.0f);
		}
	}
	return temperature;
}

Grid<float> climate::DistanceToWater(const Grid<uint8_t>& water)
{
	constexpr float k_Far = std::numeric_limits<float>::max() / 4.0f;
	constexpr float k_Diagonal = 1.41421356f;
	const int side = water.side;
	Grid<float> distance(side, k_Far);
	for (size_t i = 0; i < water.values.size(); ++i)
	{
		if (water.values[i] != 0)
		{
			distance.values[i] = 0.0f;
		}
	}
	const auto relax = [&distance](int x, int z, int nx, int nz, float step) {
		if (distance.Contains(nx, nz))
		{
			distance.At(x, z) = std::min(distance.At(x, z), distance.At(nx, nz) + step);
		}
	};
	// Two passes of a chamfer: from the low corner forwards, then from the high corner back
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
	return distance;
}

Grid<float> climate::Moisture(const Grid<float>& heights, const Grid<uint8_t>& water, uint64_t seed, const Settings& settings)
{
	const int side = heights.side;
	const auto step = StepOf(settings.wind);
	Grid<float> carried(side, 1.0f);
	Grid<float> rain(side, 0.0f);

	// The air moves with the wind: each corner takes its air from the corner upwind, which comes first in this order
	for (int i = 0; i < side; ++i)
	{
		const int x = step.dx >= 0 ? i : side - 1 - i;
		for (int j = 0; j < side; ++j)
		{
			const int z = step.dz >= 0 ? j : side - 1 - j;
			const int ux = x - step.dx;
			const int uz = z - step.dz;
			float air = heights.Contains(ux, uz) ? carried.At(ux, uz) : 1.0f;
			if (water.At(x, z) != 0)
			{
				// The air takes up moisture over the water
				air = std::min(1.0f, air + 0.05f);
				rain.At(x, z) = air * settings.flatRain;
			}
			else
			{
				const float climb = heights.Contains(ux, uz) ? std::max(0.0f, heights.At(x, z) - heights.At(ux, uz)) : 0.0f;
				const float part = std::min(settings.flatRain + (settings.climbRain * climb), 0.5f);
				rain.At(x, z) = air * part;
				air -= air * part;
			}
			carried.At(x, z) = air;
		}
	}
	rain = Blur(rain, 6);

	const auto distance = DistanceToWater(water);
	const auto variation = StageSeed(seed, 21);
	const float last = static_cast<float>(std::max(side - 1, 1));
	Grid<float> moisture(side, 0.0f);
	for (int x = 0; x < side; ++x)
	{
		for (int z = 0; z < side; ++z)
		{
			const float r = rain.At(x, z);
			const float fromRain = r / (r + settings.flatRain);
			const float nearWater = 1.0f - noise::SmoothStep(0.0f, settings.waterReach * 3.0f, distance.At(x, z));
			const float u = static_cast<float>(x) / last;
			const float v = static_cast<float>(z) / last;
			const float value = (0.65f * fromRain) + (0.35f * nearWater) +
			                    (settings.variation * noise::Fractal(variation, u * 5.0f, v * 5.0f, 3));
			moisture.At(x, z) = std::clamp(value, 0.0f, 1.0f);
		}
	}
	return moisture;
}
