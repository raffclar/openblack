/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "landgen/Generator.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <chrono>
#include <optional>

#include "landgen/Noise.h"
#include "landgen/Random.h"

using namespace openblack::landgen;
using biomes::Biome;

namespace
{
constexpr uint8_t k_SeaLevelAltitude = 3;
/// How far the noise that bends the rivers moves the land for the water, up or down, in altitude units
constexpr float k_RiverBend = 3.0f;

class StageClock
{
public:
	explicit StageClock(std::vector<StageTime>& times)
	    : _times(times)
	    , _start(std::chrono::steady_clock::now())
	{
	}
	void Done(std::string_view stage)
	{
		const auto now = std::chrono::steady_clock::now();
		_times.push_back({.stage = stage, .milliseconds = std::chrono::duration<double, std::milli>(now - _start).count()});
		_start = now;
	}

private:
	std::vector<StageTime>& _times;
	std::chrono::steady_clock::time_point _start;
};

Grid<uint8_t> Mask(int side, auto&& test)
{
	Grid<uint8_t> mask(side, 0);
	for (size_t i = 0; i < mask.values.size(); ++i)
	{
		mask.values[i] = test(i) ? 1 : 0;
	}
	return mask;
}
} // namespace

Sound openblack::landgen::SoundOf(Biome biome)
{
	switch (biome)
	{
	case Biome::Sea:
		return Sound::Sea;
	case Biome::Beach:
		return Sound::Coastal;
	case Biome::DenseJungle:
		return Sound::Jungle;
	case Biome::Desert:
		return Sound::Desert;
	case Biome::Swamp:
		return Sound::Swamp;
	case Biome::NordicWastes:
	case Biome::Alpine:
		return Sound::Arctic;
	case Biome::Grassland:
	case Biome::TemperateForest:
	case Biome::DeadWoods:
	case Biome::_Count:
		break;
	}
	return Sound::Countryside;
}

StartArea openblack::landgen::FindStartArea(const Grid<float>& heights, const Grid<float>& seaDistance,
                                            const Grid<float>& riverDistance, const StartSettings& settings)
{
	const int side = heights.side;
	const float last = static_cast<float>(std::max(side - 1, 1));
	const auto reach = static_cast<int>(settings.flatRadius);
	std::optional<StartArea> best;
	float bestScore = 0.0f;
	for (int x = reach; x < side - reach; x += 4)
	{
		for (int z = reach; z < side - reach; z += 4)
		{
			const float height = heights.At(x, z);
			if (height < settings.lowest || height > settings.highest || seaDistance.At(x, z) < settings.nearestSea ||
			    seaDistance.At(x, z) > settings.farthestSea || riverDistance.At(x, z) < settings.riverDistance)
			{
				continue;
			}
			float rough = 0.0f;
			for (int dx = -reach; dx <= reach; dx += 3)
			{
				for (int dz = -reach; dz <= reach; dz += 3)
				{
					rough = std::max(rough, std::fabs(heights.At(x + dx, z + dz) - height));
				}
			}
			// A mild climate: near the middle from north to south
			const float v = static_cast<float>(z) / last;
			const float score = rough + (std::fabs(v - 0.55f) * 40.0f);
			if (!best.has_value() || score < bestScore)
			{
				best = StartArea {.x = static_cast<float>(x), .z = static_cast<float>(z), .radius = settings.flatRadius};
				bestScore = score;
			}
		}
	}
	return best.value_or(StartArea {.x = last * 0.5f, .z = last * 0.5f, .radius = settings.flatRadius});
}

void openblack::landgen::FlattenStartArea(Grid<float>& heights, const StartArea& start, const StartSettings& settings)
{
	const auto cx = static_cast<int>(start.x);
	const auto cz = static_cast<int>(start.z);
	const auto inner = static_cast<int>(settings.flatRadius);
	const auto outer = static_cast<int>(settings.blendRadius);
	float sum = 0.0f;
	int count = 0;
	for (int dx = -inner; dx <= inner; ++dx)
	{
		for (int dz = -inner; dz <= inner; ++dz)
		{
			if (heights.Contains(cx + dx, cz + dz) && (dx * dx) + (dz * dz) <= inner * inner)
			{
				sum += heights.At(cx + dx, cz + dz);
				++count;
			}
		}
	}
	if (count == 0)
	{
		return;
	}
	const float level = sum / static_cast<float>(count);
	for (int dx = -outer; dx <= outer; ++dx)
	{
		for (int dz = -outer; dz <= outer; ++dz)
		{
			if (!heights.Contains(cx + dx, cz + dz))
			{
				continue;
			}
			const float distance = std::sqrt(static_cast<float>((dx * dx) + (dz * dz)));
			const float weight = noise::SmoothStep(settings.blendRadius, settings.flatRadius, distance);
			auto& height = heights.At(cx + dx, cz + dz);
			height += (level - height) * weight;
		}
	}
}

Grid<openblack::lnd::LNDCell> openblack::landgen::MakeCells(const Grid<uint8_t>& altitudes, const Grid<uint8_t>& luminosity,
                                                            const Grid<Biome>& biomes, const Grid<uint8_t>& freshWater,
                                                            const Grid<uint8_t>& runningWater)
{
	const int side = altitudes.side;
	Grid<openblack::lnd::LNDCell> cells(side, openblack::lnd::LNDCell {});
	const auto altitude = [&](int x, int z) {
		return static_cast<int>(altitudes.At(std::min(x, side - 1), std::min(z, side - 1)));
	};
	for (int x = 0; x < side; ++x)
	{
		for (int z = 0; z < side; ++z)
		{
			auto& cell = cells.At(x, z);
			cell.altitude = altitudes.At(x, z);
			cell.luminosity = luminosity.At(x, z);
			cell.properties.country = static_cast<uint8_t>(biomes.At(x, z)) & 0xFu;

			const int a00 = altitude(x, z);
			const int a10 = altitude(x + 1, z);
			const int a01 = altitude(x, z + 1);
			const int a11 = altitude(x + 1, z + 1);
			const int low = std::min({a00, a10, a01, a11});
			const int high = std::max({a00, a10, a01, a11});
			const bool fresh = freshWater.At(x, z) != 0;
			const auto water = fresh ? Sound::StillFreshWater : Sound::Sea;
			// The cells are cut along their diagonals in a checkerboard. Each block then has the same triangles, and the
			// renderer keeps few lists of triangles for a land of many blocks.
			cell.properties.split = ((x + z) & 1) != 0 ? 1 : 0;
			uint8_t sound = 0;
			if (high == 0)
			{
				cell.properties.hasWater = 1;
				sound = static_cast<uint8_t>(water);
				cell.flags = k_OpenWaterFlag;
			}
			else if (high <= k_SeaLevelAltitude)
			{
				cell.properties.hasWater = 1;
				sound = static_cast<uint8_t>(fresh ? Sound::StillFreshWater : Sound::Coastal);
			}
			else if (low <= k_SeaLevelAltitude)
			{
				cell.properties.coastLine = 1;
				sound = static_cast<uint8_t>(fresh ? Sound::StillFreshWater : Sound::Coastal);
			}
			else if (runningWater.At(x, z) != 0)
			{
				sound = static_cast<uint8_t>(Sound::RunningWater);
			}
			else
			{
				sound = static_cast<uint8_t>(SoundOf(biomes.At(x, z)));
			}
			cell.flags = static_cast<uint8_t>(cell.flags | (sound << 2u));
		}
	}
	return cells;
}

GeneratedLand openblack::landgen::Generate(const Settings& settings)
{
	GeneratedLand land;
	StageClock clock(land.times);
	const int side = settings.side;
	const auto seed = settings.seed;

	// The shape of the land
	auto heights = terrain::BaseHeights(side, seed, settings.base);
	clock.Done("base heights");
	terrain::ErodeHydraulic(heights, seed, settings.hydraulic);
	clock.Done("hydraulic erosion");
	terrain::ErodeThermal(heights, settings.thermal);
	clock.Done("thermal erosion");

	// The water: lakes first, then the rivers on the land that the lakes leave
	const auto firstDrainage = hydrology::Drain(heights, terrain::k_SeaLevel);
	auto lakes = hydrology::FormLakes(heights, firstDrainage, settings.lakes);
	land.lakes = lakes.seaLakes.size();
	clock.Done("lakes");
	// The rivers follow a little noise over the land, so that they bend as rivers do on gentle slopes
	Grid<float> bent = heights;
	const auto bendSeed = StageSeed(seed, 33);
	const float last = static_cast<float>(std::max(side - 1, 1));
	for (int x = 0; x < side; ++x)
	{
		for (int z = 0; z < side; ++z)
		{
			auto& height = bent.At(x, z);
			if (height > terrain::k_SeaLevel)
			{
				height += k_RiverBend * noise::Fractal(bendSeed, static_cast<float>(x) / last * 48.0f,
				                                       static_cast<float>(z) / last * 48.0f, 2);
				height = std::max(height, terrain::k_SeaLevel + 0.01f);
			}
		}
	}
	const auto drainage = hydrology::Drain(bent, terrain::k_SeaLevel);
	const auto flow = hydrology::Accumulate(drainage, Grid<float>(side, 1.0f));
	land.rivers = hydrology::TraceRivers(drainage, flow, settings.rivers);
	hydrology::CarveRivers(heights, land.rivers, flow, settings.rivers);
	clock.Done("rivers");
	terrain::ErodeThermal(heights, {.passes = 4,
	                                .plainTalus = settings.thermal.plainTalus,
	                                .mountainTalus = settings.thermal.mountainTalus,
	                                .rate = settings.thermal.rate});

	Grid<uint8_t> riverCorners(side, 0);
	for (const auto& river : land.rivers)
	{
		for (const auto corner : river.corners)
		{
			riverCorners.values[corner] = 1;
		}
	}
	const auto riverDistance = climate::DistanceToWater(riverCorners);
	const auto sea = Mask(side, [&heights](size_t i) { return heights.values[i] <= terrain::k_SeaLevel; });
	const StartSettings startSettings;
	land.start = FindStartArea(heights, climate::DistanceToWater(sea), riverDistance, startSettings);
	FlattenStartArea(heights, land.start, startSettings);
	clock.Done("start area");

	Grid<uint8_t> altitudes(side, 0);
	for (size_t i = 0; i < heights.values.size(); ++i)
	{
		altitudes.values[i] =
		    static_cast<uint8_t>(std::clamp(std::floor(heights.values[i] + 0.5f), 0.0f, terrain::k_HighestAltitude));
	}

	// The climate and the biomes
	auto climateSettings = settings.climate;
	climateSettings.wind = settings.wind.value_or(static_cast<climate::Wind>(Random(StageSeed(seed, 30)).NextBelow(8)));
	const auto seaNow = Mask(side, [&altitudes](size_t i) { return altitudes.values[i] <= k_SeaLevelAltitude; });
	const auto water = Mask(side, [&](size_t i) { return seaNow.values[i] != 0 || riverCorners.values[i] != 0; });
	const auto temperature = climate::Temperature(heights, seed, climateSettings);
	const auto moisture = climate::Moisture(heights, water, seed, climateSettings);
	const auto seaDistance = climate::DistanceToWater(seaNow);
	const auto slopes = terrain::Slopes(heights);
	clock.Done("climate");

	Grid<Biome> biomes(side, Biome::Sea);
	const auto beachSeed = StageSeed(seed, 31);
	const auto blightSeed = StageSeed(seed, 32);
	for (int x = 0; x < side; ++x)
	{
		for (int z = 0; z < side; ++z)
		{
			const float u = static_cast<float>(x) / last;
			const float v = static_cast<float>(z) / last;
			const auto index = heights.Index(x, z);
			biomes.At(x, z) = biomes::Classify(
			    {
			        .altitude = static_cast<float>(altitudes.values[index]),
			        .temperature = temperature.values[index],
			        .moisture = moisture.values[index],
			        .slope = slopes.values[index],
			        .seaDistance = seaDistance.values[index],
			        .beachWidth =
			            4.0f + (8.0f * noise::SmoothStep(-0.4f, 0.4f, noise::Fractal(beachSeed, u * 10.0f, v * 10.0f, 2))),
			        .wet = lakes.wet.values[index] != 0,
			        .blight = noise::Fractal(blightSeed, u * 6.0f, v * 6.0f, 3),
			    },
			    settings.thresholds);
		}
	}
	land.biomes = biomes::Smooth(biomes, 2);
	clock.Done("biomes");

	// What the landscape holds
	const auto luminosity = lighting::Luminosity(altitudes, settings.lighting);
	clock.Done("light");
	Grid<uint8_t> freshWater(side, 0);
	for (const auto& lake : lakes.seaLakes)
	{
		for (const auto corner : lake)
		{
			freshWater.values[corner] = 1;
		}
	}
	const auto runningWater = Mask(side, [&riverDistance](size_t i) { return riverDistance.values[i] <= 2.0f; });
	land.cells = MakeCells(altitudes, luminosity, land.biomes, freshWater, runningWater);
	land.materials = countries::UsedSurfaces();
	for (size_t biome = 0; biome < biomes::k_BiomeCount; ++biome)
	{
		land.countries.push_back(countries::MakeCountry(countries::RecipeOf(static_cast<Biome>(biome)), land.materials));
	}
	clock.Done("cells and countries");

	// Nothing grows in the water, on the rivers or on their banks
	const auto keepClear =
	    Mask(side, [&](size_t i) { return altitudes.values[i] <= k_SeaLevelAltitude + 1 || riverDistance.values[i] <= 1.5f; });
	const std::array clearings {features::Clearing {.x = land.start.x, .z = land.start.z, .radius = startSettings.blendRadius}};
	land.features = features::Place(land.biomes, heights, keepClear, clearings, seed, settings.features);
	clock.Done("features");
	return land;
}
