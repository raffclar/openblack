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
#include <utility>

#include "landgen/Noise.h"
#include "landgen/Random.h"

using namespace openblack::landgen;
using biomes::Biome;

namespace
{
constexpr uint8_t k_SeaLevelAltitude = 3;
/// How far the noise that bends the rivers moves the land for the water, up or down, in altitude units
constexpr float k_RiverBend = 3.0f;
constexpr float k_MetresPerCorner = 10.0f;
/// No path is drawn and no lantern stands this near the temple, a building or a field, in corners
constexpr float k_TempleClearRadius = 5.0f;
constexpr float k_BuildingClearRadius = 1.5f;
constexpr float k_FieldClearRadius = 1.2f;

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
                                                            const Grid<uint8_t>& runningWater, const Grid<float>* lakeLevels)
{
	const int side = altitudes.side;
	Grid<openblack::lnd::LNDCell> cells(side, openblack::lnd::LNDCell {});
	const auto altitude = [&](int x, int z) {
		return static_cast<int>(altitudes.At(std::min(x, side - 1), std::min(z, side - 1)));
	};
	// The level of the lake at a corner, or below 0 where no lake is
	const auto lakeLevel = [&](int x, int z) {
		return lakeLevels != nullptr ? lakeLevels->At(std::min(x, side - 1), std::min(z, side - 1)) : -1.0f;
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
			else if (const float lake =
			             std::max({lakeLevel(x, z), lakeLevel(x + 1, z), lakeLevel(x, z + 1), lakeLevel(x + 1, z + 1)});
			         lake >= 0.0f && static_cast<float>(low) <= lake)
			{
				// A lake above the sea: its water and its shore are as the sea's, with the sound of still fresh water
				if (static_cast<float>(high) <= lake)
				{
					cell.properties.hasWater = 1;
				}
				else
				{
					cell.properties.coastLine = 1;
				}
				sound = static_cast<uint8_t>(Sound::StillFreshWater);
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
	land.heightCurve = HeightCurve::WithTop(settings.heightKnee, settings.topHeight);
	const int side = settings.side;
	const auto seed = settings.seed;

	// The shape of the land
	auto heights = terrain::BaseHeights(side, seed, settings.base);
	clock.Done("base heights");
	terrain::ErodeHydraulic(heights, seed, settings.hydraulic);
	clock.Done("hydraulic erosion");
	terrain::ErodeThermal(heights, settings.thermal);
	clock.Done("thermal erosion");
	// The terraces and cliffs come after the erosion, which would wear the cliffs down. Away from the massifs the edge
	// noise lifts the ground by a limited height, so that no high tower stands on the plains.
	const auto peaks = terrain::PlaceMassifs(heights, seed, settings.massifs);
	const auto massifs = terrain::MassifHeights(side, peaks, seed, settings.massifs);
	std::vector<float> highestEdge(massifs.values.size());
	for (size_t i = 0; i < highestEdge.size(); ++i)
	{
		highestEdge[i] =
		    massifs.values[i] > 0.0f ? terrain::k_HighestAltitude : heights.values[i] + settings.massifs.lowlandRise;
	}
	auto cliffs = terrain::Terrace(heights, seed, settings.terraces, highestEdge);
	clock.Done("terraces");
	// The high land is on the massifs only
	terrain::LowerHighGround(heights, settings.massifs.lowlandKnee, settings.massifs.lowlandCeiling);
	terrain::RaiseMassifs(heights, massifs, cliffs);
	clock.Done("massifs");
	terrain::RollGround(heights, seed, settings.roll);
	clock.Done("rolling ground");

	// The water: lakes first, then the rivers on the land that the lakes leave
	const auto firstDrainage = hydrology::Drain(heights, terrain::k_SeaLevel);
	auto basins = hydrology::FormLakes(heights, firstDrainage, settings.lakes);
	land.lakes = basins.seaLakes.size();
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
	terrain::ErodeThermal(heights,
	                      {.passes = 4,
	                       .plainTalus = settings.thermal.plainTalus,
	                       .mountainTalus = settings.thermal.mountainTalus,
	                       .rate = settings.thermal.rate},
	                      cliffs.values);
	terrain::ShoreShelf(heights, seed, settings.shelf);
	clock.Done("shore");

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
	const auto startSea = climate::DistanceToWater(sea);
	land.start = FindStartArea(heights, startSea, riverDistance, startSettings);
	FlattenStartArea(heights, land.start, startSettings);
	clock.Done("start area");

	// The lakes on the hills: round hollows dug into high flat ground, away from the sea, the rivers and the start
	const auto awayFromHollows =
	    Mask(side, [&](size_t i) { return heights.values[i] <= terrain::k_SeaLevel || riverCorners.values[i] != 0; });
	lakes::DigHollows(heights, awayFromHollows, {{.x = land.start.x, .z = land.start.z, .radius = startSettings.blendRadius}},
	                  StageSeed(seed, 35), settings.hillHollows);
	clock.Done("hill hollows");

	// The neutral towns stand on flat ground, as the towns of the story do, so the ground of each town becomes flat
	const auto firstTown = settlement::TownOfStart(land.start.x, land.start.z, seed, settings.settlement);
	const std::array startArea {settlement::Area {.x = land.start.x, .z = land.start.z, .radius = startSettings.blendRadius}};
	const auto sites =
	    settlement::FindSites(heights, land.heightCurve, startSea, riverDistance, firstTown, startArea, seed, settings.sites);
	const StartSettings townGround {.flatRadius = settings.sites.flatRadius, .blendRadius = settings.sites.blendRadius};
	for (const auto& site : sites)
	{
		FlattenStartArea(heights, {.x = site.x, .z = site.z, .radius = townGround.flatRadius}, townGround);
	}
	clock.Done("town places");

	// The altitude byte of each corner is its height rounded. The height units come from the height before the rounding,
	// through the curve, so that the high ground has no steps of many units.
	Grid<uint8_t> altitudes(side, 0);
	land.heightUnits = Grid<uint16_t>(side, 0);
	for (size_t i = 0; i < heights.values.size(); ++i)
	{
		const float height = std::clamp(heights.values[i], 0.0f, terrain::k_HighestAltitude);
		const auto altitude = static_cast<uint8_t>(std::floor(height + 0.5f));
		altitudes.values[i] = altitude;
		land.heightUnits.values[i] =
		    height <= land.heightCurve.knee ? uint16_t {altitude} : land.heightCurve.WholeUnitsOf(height);
	}
	// The lakes above the sea, from the altitudes that the game draws: each basin holds water at its own level
	Grid<float> drawn(side, 0.0f);
	for (size_t i = 0; i < drawn.values.size(); ++i)
	{
		drawn.values[i] = static_cast<float>(altitudes.values[i]);
	}
	land.highLakes = lakes::Find(drawn, static_cast<float>(k_SeaLevelAltitude), settings.highLakes);
	Grid<float> lakeLevels(side, -1.0f);
	for (size_t i = 0; i < drawn.values.size(); ++i)
	{
		// A joined lake lowers the bed of the higher lake
		if (drawn.values[i] != static_cast<float>(altitudes.values[i]))
		{
			altitudes.values[i] = static_cast<uint8_t>(std::clamp(drawn.values[i], 0.0f, terrain::k_HighestAltitude));
			heights.values[i] = drawn.values[i];
		}
	}
	for (const auto& lake : land.highLakes)
	{
		for (const auto corner : lake.corners)
		{
			lakeLevels.values[corner] = lake.level;
		}
	}
	clock.Done("high lakes");

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
			        .wet = basins.wet.values[index] != 0 || lakeLevels.values[index] >= 0.0f,
			        .blight = noise::Fractal(blightSeed, u * 6.0f, v * 6.0f, 3),
			    },
			    settings.thresholds);
		}
	}
	land.biomes = biomes::Smooth(biomes, 2);
	clock.Done("biomes");

	// What the landscape holds
	const auto luminosity = lighting::Luminosity(land.heightUnits, settings.lighting);
	clock.Done("light");
	Grid<uint8_t> freshWater(side, 0);
	for (const auto& lake : basins.seaLakes)
	{
		for (const auto corner : lake)
		{
			freshWater.values[corner] = 1;
		}
	}
	const auto runningWater = Mask(side, [&riverDistance](size_t i) { return riverDistance.values[i] <= 2.0f; });
	land.cells = MakeCells(altitudes, luminosity, land.biomes, freshWater, runningWater, &lakeLevels);
	land.materials = countries::UsedSurfaces();
	for (size_t biome = 0; biome < biomes::k_BiomeCount; ++biome)
	{
		land.countries.push_back(countries::MakeCountry(countries::RecipeOf(static_cast<Biome>(biome)), land.materials));
	}
	land.countries.push_back(countries::MakeCountry(countries::PathRecipe(), land.materials));
	clock.Done("cells and countries");

	// Nothing grows in the water, on the rivers or on their banks
	const auto keepClear = Mask(side, [&](size_t i) {
		return altitudes.values[i] <= k_SeaLevelAltitude + 1 || riverDistance.values[i] <= 1.5f || lakeLevels.values[i] >= 0.0f;
	});
	const auto rivers = Mask(side, [&riverDistance](size_t i) { return riverDistance.values[i] <= 1.5f; });

	// The temple and the town on the start area, and the neutral towns
	land.settlement = settlement::Make(land.start.x, land.start.z, keepClear, seed, settings.settlement);
	const auto tribes = settlement::TribesOf(land.settlement.town.tribe, seed, settings.sites);
	const auto townTribes = settlement::TribesOfTowns(tribes, sites.size(), seed);
	for (size_t i = 0; i < sites.size(); ++i)
	{
		land.settlement.neutral.push_back(
		    settlement::MakeTown(sites[i], townTribes[i], keepClear, seed, static_cast<uint32_t>(i), settings.settlement));
	}
	clock.Done("towns");

	// The paths from the temple to the town of the start, and on from town to town. They are drawn on the ground with the
	// country of the paths, and no tree or rock stands on them or on their lanterns.
	const auto pathWater =
	    Mask(side, [&](size_t i) { return altitudes.values[i] <= k_SeaLevelAltitude + 1 || lakeLevels.values[i] >= 0.0f; });
	std::vector<paths::Place> places {
	    {.x = land.settlement.citadelX / k_MetresPerCorner, .z = land.settlement.citadelZ / k_MetresPerCorner},
	    {.x = land.settlement.town.x / k_MetresPerCorner, .z = land.settlement.town.z / k_MetresPerCorner}};
	std::vector<paths::Clear> pathClear {{.x = places.front().x, .z = places.front().z, .radius = k_TempleClearRadius}};
	const auto clearTown = [&pathClear](const settlement::Town& town) {
		for (const auto& building : town.buildings)
		{
			pathClear.push_back(
			    {.x = building.x / k_MetresPerCorner, .z = building.z / k_MetresPerCorner, .radius = k_BuildingClearRadius});
		}
		for (const auto& field : town.fields)
		{
			pathClear.push_back(
			    {.x = field.x / k_MetresPerCorner, .z = field.z / k_MetresPerCorner, .radius = k_FieldClearRadius});
		}
	};
	clearTown(land.settlement.town);
	for (const auto& town : land.settlement.neutral)
	{
		places.push_back({.x = town.x / k_MetresPerCorner, .z = town.z / k_MetresPerCorner});
		clearTown(town);
	}
	const paths::Ground pathGround {.heights = heights, .curve = land.heightCurve, .water = pathWater, .rivers = rivers};
	land.paths = paths::Lay(pathGround, places, paths::ChainLinks(places), seed, settings.paths);
	const auto drawnPaths = paths::Draw(land.paths, pathWater, pathClear);
	land.lanterns = paths::PlaceLanterns(land.paths, places, pathWater, pathClear, seed, settings.paths);
	auto growClear = keepClear;
	for (size_t i = 0; i < drawnPaths.values.size(); ++i)
	{
		auto& cell = land.cells.values[i];
		if (drawnPaths.values[i] != 0)
		{
			growClear.values[i] = 1;
			if (cell.properties.hasWater == 0 && cell.properties.coastLine == 0)
			{
				cell.properties.country = k_PathCountry;
			}
		}
	}
	for (const auto& lantern : land.lanterns)
	{
		const int x = static_cast<int>(std::floor((lantern.x / k_MetresPerCorner) + 0.5f));
		const int z = static_cast<int>(std::floor((lantern.z / k_MetresPerCorner) + 0.5f));
		if (growClear.Contains(x, z))
		{
			growClear.At(x, z) = 1;
		}
	}
	clock.Done("paths");
	// Nothing grows on the ground of the towns
	std::vector<features::Clearing> clearings {
	    features::Clearing {.x = land.start.x, .z = land.start.z, .radius = startSettings.blendRadius}};
	std::vector<features::Clearing> towns;
	for (const auto& site : sites)
	{
		towns.push_back({.x = site.x, .z = site.z, .radius = settings.sites.blendRadius});
	}
	clearings.insert(clearings.end(), towns.begin(), towns.end());
	land.features = features::Place({.biomes = land.biomes,
	                                 .heights = heights,
	                                 .seaDistance = seaDistance,
	                                 .keepClear = growClear,
	                                 .clearings = clearings,
	                                 .townX = land.start.x,
	                                 .townZ = land.start.z,
	                                 .rivers = &rivers,
	                                 .towns = towns},
	                                seed, settings.features);
	clock.Done("features");
	auto cursed = cursed::Place({.biomes = land.biomes,
	                             .heights = heights,
	                             .seaDistance = seaDistance,
	                             .keepClear = growClear,
	                             .clearings = clearings,
	                             .townX = land.start.x,
	                             .townZ = land.start.z,
	                             .rivers = &rivers,
	                             .towns = towns},
	                            land.features, seed, settings.cursed);
	land.cursedPatches = std::move(cursed.patches);
	land.mists = std::move(cursed.mists);
	clock.Done("cursed places");
	auto swampMist = swamps::Place({.biomes = land.biomes,
	                                .heights = heights,
	                                .seaDistance = seaDistance,
	                                .keepClear = keepClear,
	                                .clearings = clearings,
	                                .townX = land.start.x,
	                                .townZ = land.start.z,
	                                .rivers = &rivers,
	                                .towns = towns},
	                               seed, settings.swamps);
	land.swampSheets = std::move(swampMist.sheets);
	land.mists.insert(land.mists.end(), swampMist.mists.begin(), swampMist.mists.end());
	clock.Done("swamp mist");

	// The forests, nearest the town of the start first
	land.forests = features::GroupForests(land.features, land.settlement.town.x, land.settlement.town.z, settings.forests);
	clock.Done("forests");
	return land;
}
