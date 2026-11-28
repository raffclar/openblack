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

#include "landgen/LakeShore.h"
#include "landgen/Noise.h"
#include "landgen/Random.h"
#include "landgen/Stages.h"

using namespace openblack::landgen;
using biomes::Biome;

namespace
{
constexpr uint8_t k_SeaLevelAltitude = 3;
/// A corner at this altitude or lower holds water, and a corner one unit higher is on the coast line
constexpr int k_WaterAltitude = 1;
constexpr int k_CoastAltitude = 2;
/// The distance along each axis in metres from the middle of a fish farm to a tree or a rock that the fish farm removes
constexpr float k_FarmClearance = 7.5f;
/// How far the noise that bends the rivers moves the land for the water, up or down, in altitude units
constexpr float k_RiverBend = 3.0f;
constexpr float k_MetresPerCorner = 10.0f;
/// No path is drawn and no lantern stands this near the temple, a building or a field, in corners
constexpr float k_TempleClearRadius = 5.0f;
constexpr float k_BuildingClearRadius = 1.5f;
constexpr float k_FieldClearRadius = 1.2f;
/// The bed of a path and its banks keep this many altitude units above the sea, and above a lake that is this many
/// corners away or nearer
constexpr float k_ShoreRise = 1.5f;
constexpr float k_LakeShoreReach = 3.0f;

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

/// The height units that the game draws at each corner, from the heights through the curve
Grid<float> MakeDrawnUnits(const Grid<float>& heights, const HeightCurve& curve)
{
	Grid<float> units(heights.side, 0.0f);
	for (size_t i = 0; i < units.values.size(); ++i)
	{
		units.values[i] = curve.UnitsOf(std::clamp(heights.values[i], 0.0f, terrain::k_HighestAltitude));
	}
	return units;
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
                                                            const Grid<float>* lakeLevels)
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
			else if (low <= k_SeaLevelAltitude)
			{
				// As on the lands of the story, the altitude of the cell's own corner gives its water and its coast
				if (a00 <= k_WaterAltitude)
				{
					cell.properties.hasWater = 1;
				}
				else if (a00 <= k_CoastAltitude)
				{
					cell.properties.coastLine = 1;
				}
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
			else
			{
				sound = static_cast<uint8_t>(SoundOf(biomes.At(x, z)));
			}
			cell.flags = static_cast<uint8_t>(cell.flags | (sound << 2u));
		}
	}
	return cells;
}

namespace
{
/// Gives the cursed places their dark ground: two countries after the countries of the land, the dark scrub of the edge
/// and the burnt rock of the middle. The water and the banks of the rivers keep their ground.
void ScorchCursedGround(GeneratedLand& land, const Grid<uint8_t>& keepClear, uint64_t seed, const cursed::Settings& settings)
{
	// A cell holds the country in four bits
	constexpr size_t k_MostCountries = 16;
	const auto recipes = countries::CursedRecipes();
	if (land.cursedPatches.empty() || land.countries.size() + recipes.size() > k_MostCountries)
	{
		return;
	}
	const auto edgeCountry = static_cast<uint8_t>(land.countries.size());
	for (const auto& recipe : recipes)
	{
		land.countries.push_back(countries::MakeCountry(recipe, land.materials));
	}
	const int side = land.cells.side;
	for (const auto& patch : land.cursedPatches)
	{
		const int lowX = std::max(0, static_cast<int>(std::floor((patch.x - patch.radius) / k_MetresPerCorner)));
		const int highX = std::min(side - 1, static_cast<int>(std::ceil((patch.x + patch.radius) / k_MetresPerCorner)));
		const int lowZ = std::max(0, static_cast<int>(std::floor((patch.z - patch.radius) / k_MetresPerCorner)));
		const int highZ = std::min(side - 1, static_cast<int>(std::ceil((patch.z + patch.radius) / k_MetresPerCorner)));
		for (int x = lowX; x <= highX; ++x)
		{
			for (int z = lowZ; z <= highZ; ++z)
			{
				if (keepClear.At(x, z) != 0)
				{
					continue;
				}
				const auto scorch = cursed::ScorchAt(patch, static_cast<float>(x) * k_MetresPerCorner,
				                                     static_cast<float>(z) * k_MetresPerCorner, seed, settings);
				if (scorch != cursed::Scorch::None)
				{
					land.cells.At(x, z).properties.country =
					    static_cast<uint8_t>(edgeCountry + (scorch == cursed::Scorch::Core ? 1u : 0u)) & 0xFu;
				}
			}
		}
	}
}
} // namespace

GeneratedLand openblack::landgen::Generate(const Settings& chosen)
{
	GeneratedLand land;
	StageClock clock(land.times);
	// The type of the land changes the settings of its shape and climate
	land.type = landtypes::Pick(chosen.seed, chosen.landType);
	auto settings = chosen;
	landtypes::Apply(land.type, settings.seed, settings.base, settings.massifs, settings.climate, settings.topHeight);
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
	const auto bendSeed = StageSeed(seed, Stage::RiverBends);
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
	// The streams start at springs on the hills and on the rivers, as many for the area as the lands of the story have
	const auto traced = hydrology::TraceRivers(drainage, flow, settings.rivers);
	const auto streamWater = Mask(side, [&heights](size_t i) { return heights.values[i] <= terrain::k_SeaLevel; });
	const auto landCorners = static_cast<size_t>(std::ranges::count(streamWater.values, uint8_t {0}));
	land.rivers = streams::Choose(traced, drainage.receivers, heights, streamWater, landCorners, seed, settings.streams);
	hydrology::CarveRivers(heights, land.rivers, flow, settings.rivers);
	clock.Done("rivers");
	terrain::ErodeThermal(heights,
	                      {.passes = 4,
	                       .plainTalus = settings.thermal.plainTalus,
	                       .mountainTalus = settings.thermal.mountainTalus,
	                       .rate = settings.thermal.rate},
	                      cliffs.values);
	Grid<uint8_t> riverCorners(side, 0);
	for (const auto& river : land.rivers)
	{
		for (const auto corner : river.corners)
		{
			riverCorners.values[corner] = 1;
		}
	}
	const auto riverDistance = climate::DistanceToWater(riverCorners);
	const auto shoreWidths = terrain::ShoreShelf(heights, &riverDistance, seed, settings.shelf);
	clock.Done("shore");

	const auto sea = Mask(side, [&heights](size_t i) { return heights.values[i] <= terrain::k_SeaLevel; });
	const StartSettings startSettings;
	const auto startSea = climate::DistanceToWater(sea);
	land.start = FindStartArea(heights, startSea, riverDistance, startSettings);
	FlattenStartArea(heights, land.start, startSettings);
	// The other players of a land for more than one player. The town of each player is on its start area.
	const auto playerCount = std::min(settings.players.count, players::k_MostPlayers);
	std::vector<uint64_t> playerSeeds {seed};
	std::vector<players::Start> townOffsets;
	for (uint32_t player = 0; player < playerCount; ++player)
	{
		if (player > 0)
		{
			playerSeeds.push_back(players::SeedOfPlayer(seed, player));
		}
		const auto town = settlement::TownOfStart(0.0f, 0.0f, playerSeeds.back(), settings.settlement);
		townOffsets.push_back({.x = town.x, .z = town.z});
	}
	for (const auto& other :
	     players::FindStarts(heights, startSea, riverDistance, {.x = land.start.x, .z = land.start.z}, townOffsets,
	                         {.lowest = startSettings.lowest,
	                         .highest = startSettings.highest,
	                         .nearestSea = startSettings.nearestSea,
	                         .farthestSea = startSettings.farthestSea,
	                         .riverDistance = startSettings.riverDistance,
	                         .flatRadius = startSettings.flatRadius},
	                         settings.players))
	{
		land.otherStarts.push_back({.x = other.x, .z = other.z, .radius = startSettings.flatRadius});
		FlattenStartArea(heights, land.otherStarts.back(), startSettings);
	}
	// Each start area of a player, player one first
	std::vector<StartArea> starts {land.start};
	starts.insert(starts.end(), land.otherStarts.begin(), land.otherStarts.end());
	clock.Done("start area");

	// The lakes on the hills: round hollows dug into high flat ground, away from the sea, the rivers and the start
	const auto awayFromHollows =
	    Mask(side, [&](size_t i) { return heights.values[i] <= terrain::k_SeaLevel || riverCorners.values[i] != 0; });
	std::vector<lakes::Keep> keepStarts;
	keepStarts.reserve(starts.size());
	for (const auto& start : starts)
	{
		keepStarts.push_back({.x = start.x, .z = start.z, .radius = startSettings.blendRadius});
	}
	lakes::DigHollows(heights, awayFromHollows, keepStarts, StageSeed(seed, Stage::HillHollows), settings.hillHollows);
	clock.Done("hill hollows");

	// The neutral towns stand on flat ground, as the towns of the story do, so the ground of each town becomes flat
	std::vector<settlement::Site> firstTowns;
	std::vector<settlement::Area> startAreas;
	for (size_t player = 0; player < starts.size(); ++player)
	{
		const auto& start = starts[player];
		firstTowns.push_back(settlement::TownOfStart(start.x, start.z, playerSeeds[player], settings.settlement));
		startAreas.push_back({.x = start.x, .z = start.z, .radius = startSettings.blendRadius});
	}
	// The lakes that the ground holds now. No town goes into one of them.
	Grid<float> roundedNow(side, 0.0f);
	for (size_t i = 0; i < heights.values.size(); ++i)
	{
		roundedNow.values[i] = std::floor(std::clamp(heights.values[i], 0.0f, terrain::k_HighestAltitude) + 0.5f);
	}
	Grid<uint8_t> lakesNow(side, 0);
	for (const auto& lake : lakes::Find(roundedNow, static_cast<float>(k_SeaLevelAltitude), settings.highLakes))
	{
		for (const auto corner : lake.corners)
		{
			lakesNow.values[corner] = 1;
		}
	}
	const auto sites = settlement::FindSites(heights, land.heightCurve, startSea, riverDistance, firstTowns, startAreas, seed,
	                                         settings.sites, &lakesNow);
	const StartSettings townGround {.flatRadius = settings.sites.flatRadius, .blendRadius = settings.sites.blendRadius};
	for (const auto& site : sites)
	{
		FlattenStartArea(heights, {.x = site.x, .z = site.z, .radius = townGround.flatRadius}, townGround);
	}
	clock.Done("town places");

	// Few faces of the cliffs of the story are near vertical. This stage comes after all the stages that shape the
	// ground, so that no wall of a massif, a terrace, a river or a hollow is steeper than the limit.
	cliffs::LimitFaces(heights, land.heightCurve, terrain::k_SeaLevel, seed, settings.faces, riverCorners.values);
	clock.Done("cliff faces");

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
	// No lake covers the flat ground of a start or a town, so that each town stays dry and its paths reach it
	std::vector<lakes::Keep> dryGround;
	dryGround.reserve(starts.size());
	for (const auto& start : starts)
	{
		dryGround.push_back({.x = start.x, .z = start.z, .radius = startSettings.flatRadius});
	}
	for (const auto& town : firstTowns)
	{
		dryGround.push_back({.x = town.x, .z = town.z, .radius = settings.sites.flatRadius});
	}
	for (const auto& site : sites)
	{
		dryGround.push_back({.x = site.x, .z = site.z, .radius = settings.sites.flatRadius});
	}
	land.highLakes = lakes::Find(drawn, static_cast<float>(k_SeaLevelAltitude), settings.highLakes, dryGround);
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
	// Each lake gets the shore steps of the sea, so that a steep rim has a flat shore for the sand. The flat ground of
	// the start areas and of the towns does not change.
	std::vector<lakes::Keep> flatGround = keepStarts;
	for (const auto& site : sites)
	{
		flatGround.push_back({.x = site.x, .z = site.z, .radius = settings.sites.blendRadius});
	}
	const auto stepped = lakeshore::ShoreSteps(drawn, land.highLakes, flatGround,
	                                           {.freeboard = settings.highLakes.freeboard, .rise = settings.shelf.cliffRise});
	for (const auto corner : stepped)
	{
		const float height = std::clamp(drawn.values[corner], 0.0f, terrain::k_HighestAltitude);
		const auto altitude = static_cast<uint8_t>(std::floor(height + 0.5f));
		heights.values[corner] = height;
		altitudes.values[corner] = altitude;
		land.heightUnits.values[corner] =
		    height <= land.heightCurve.knee ? uint16_t {altitude} : land.heightCurve.WholeUnitsOf(height);
		drawn.values[corner] = static_cast<float>(altitude);
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
	climateSettings.wind =
	    settings.wind.value_or(static_cast<climate::Wind>(Random(StageSeed(seed, Stage::Wind)).NextBelow(8)));
	const auto seaNow = Mask(side, [&altitudes](size_t i) { return altitudes.values[i] <= k_SeaLevelAltitude; });
	const auto water = Mask(side, [&](size_t i) { return seaNow.values[i] != 0 || riverCorners.values[i] != 0; });
	const auto temperature = climate::Temperature(heights, seed, climateSettings);
	// The height units that the game draws, which the rules of the trees use. The mountains and the cold of the biomes
	// come from them, so that the alpine and nordic stands start on ground where their kinds of tree grow.
	const auto drawnUnits = MakeDrawnUnits(heights, land.heightCurve);
	const auto drawnTemperature = climate::Temperature(drawnUnits, seed, climateSettings);
	const auto moisture = climate::Moisture(heights, water, seed, climateSettings);
	const auto seaDistance = climate::DistanceToWater(seaNow);
	const auto slopes = terrain::Slopes(heights);
	clock.Done("climate");

	Grid<Biome> biomes(side, Biome::Sea);
	const auto beachSeed = StageSeed(seed, Stage::Beaches);
	const auto blightSeed = StageSeed(seed, Stage::Blight);
	Grid<float> beachWidths(side, 0.0f);
	for (int x = 0; x < side; ++x)
	{
		for (int z = 0; z < side; ++z)
		{
			const float u = static_cast<float>(x) / last;
			const float v = static_cast<float>(z) / last;
			const auto index = heights.Index(x, z);
			beachWidths.values[index] =
			    std::max(4.0f + (8.0f * noise::SmoothStep(-0.4f, 0.4f, noise::Fractal(beachSeed, u * 10.0f, v * 10.0f, 2))),
			             shoreWidths.values[index]);
			biomes.At(x, z) = biomes::Classify(
			    {
			        .altitude = static_cast<float>(altitudes.values[index]),
			        .drawnAltitude = drawnUnits.values[index],
			        .temperature = temperature.values[index],
			        .drawnTemperature = drawnTemperature.values[index],
			        .moisture = moisture.values[index],
			        .slope = slopes.values[index],
			        .seaDistance = seaDistance.values[index],
			        .beachWidth = beachWidths.values[index],
			        .wet = basins.wet.values[index] != 0 || lakeLevels.values[index] >= 0.0f,
			        .blight = noise::Fractal(blightSeed, u * 6.0f, v * 6.0f, 3),
			    },
			    settings.thresholds);
		}
	}
	// The lakes above the sea get the same beaches as the sea, with the heights from the level of each lake
	const lakeshore::Shores lakeShores =
	    lakeshore::Find({.altitudes = drawn, .slopes = slopes, .temperature = temperature, .beachWidths = beachWidths},
	                    land.highLakes, settings.thresholds);
	for (size_t i = 0; i < biomes.values.size(); ++i)
	{
		if (lakeShores.shore.values[i] == lakeshore::Shore::Beach && lakeShores.distance.values[i] < seaDistance.values[i])
		{
			biomes.values[i] = Biome::Beach;
		}
	}
	land.biomes = biomes::Smooth(biomes, 2);
	clock.Done("biomes");

	// Nothing grows in the water, on the rivers or on their banks
	const auto keepClear = Mask(side, [&](size_t i) {
		return altitudes.values[i] <= k_SeaLevelAltitude + 1 || riverDistance.values[i] <= 1.5f || lakeLevels.values[i] >= 0.0f;
	});
	const auto rivers = Mask(side, [&riverDistance](size_t i) { return riverDistance.values[i] <= 1.5f; });

	// The temple and the town on the start area, and the neutral towns
	land.settlement = settlement::Make(land.start.x, land.start.z, keepClear, seed, settings.settlement);
	for (size_t player = 1; player < starts.size(); ++player)
	{
		land.otherPlayers.push_back(
		    settlement::Make(starts[player].x, starts[player].z, keepClear, playerSeeds[player], settings.settlement));
	}
	const auto tribes = settlement::TribesOf(land.settlement.town.tribe, seed, settings.sites);
	const auto townTribes = settlement::TribesOfTowns(tribes, sites.size(), seed);
	// The heights above the sea in metres, for the fish farms on the shore
	const float seaMetres = land.heightCurve.MetresOf(terrain::k_SeaLevel);
	Grid<float> aboveSea(side, 0.0f);
	for (size_t i = 0; i < heights.values.size(); ++i)
	{
		aboveSea.values[i] = land.heightCurve.MetresOf(heights.values[i]) - seaMetres;
	}
	// A fish farm stands on dry ground of the shore, also on the lowest ground just above the sea, but not on a river or a
	// lake
	const auto farmClear = Mask(side, [&](size_t i) {
		return altitudes.values[i] <= k_SeaLevelAltitude || riverDistance.values[i] <= 1.5f || lakeLevels.values[i] >= 0.0f;
	});
	std::vector<settlement::Field> fishFarms;
	for (size_t i = 0; i < sites.size(); ++i)
	{
		const auto number = static_cast<uint32_t>(i);
		auto town = settlement::MakeTown(sites[i], townTribes[i], keepClear, seed, number, settings.settlement, settings.mix);
		town.fishFarms = settlement::FishFarms(town, aboveSea, seaDistance, farmClear, fishFarms, seed, number, settings.farms);
		fishFarms.insert(fishFarms.end(), town.fishFarms.begin(), town.fishFarms.end());
		land.settlement.neutral.push_back(std::move(town));
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
	// The town of each other player joins the nearest place before it, and its temple then joins the nearest place, its town
	for (const auto& other : land.otherPlayers)
	{
		places.push_back({.x = other.town.x / k_MetresPerCorner, .z = other.town.z / k_MetresPerCorner});
		places.push_back({.x = other.citadelX / k_MetresPerCorner, .z = other.citadelZ / k_MetresPerCorner});
		pathClear.push_back({.x = places.back().x, .z = places.back().z, .radius = k_TempleClearRadius});
		clearTown(other.town);
	}
	const paths::Ground pathGround {.heights = heights, .curve = land.heightCurve, .water = pathWater, .rivers = rivers};
	land.paths = paths::Lay(pathGround, places, paths::ChainLinks(places), seed, settings.paths);
	land.lanterns = paths::PlaceLanterns(land.paths, places, pathWater, pathClear, seed, settings.paths);
	clock.Done("paths");

	// The bed of each path, cut and filled into the land as a built road, with a bank on each side. The bed and the banks
	// do not change the water, the rivers, the buildings, the fields or the temple, and do not cut a shore.
	Grid<uint8_t> roadFixed(side, 0);
	Grid<float> roadMetres(side, 0.0f);
	Grid<float> roadLowest(side, land.heightCurve.MetresOf(static_cast<float>(k_SeaLevelAltitude) + k_ShoreRise));
	for (size_t i = 0; i < heights.values.size(); ++i)
	{
		roadMetres.values[i] = land.heightCurve.MetresOf(heights.values[i]);
		roadFixed.values[i] = pathWater.values[i] != 0 || riverDistance.values[i] <= 2.0f ? roads::k_Fixed : 0;
	}
	for (const auto& circle : pathClear)
	{
		const int cx = static_cast<int>(std::floor(circle.x));
		const int cz = static_cast<int>(std::floor(circle.z));
		const int reach = static_cast<int>(std::ceil(circle.radius)) + 1;
		for (int x = cx - reach; x <= cx + reach; ++x)
		{
			for (int z = cz - reach; z <= cz + reach; ++z)
			{
				const float dx = static_cast<float>(x) - circle.x;
				const float dz = static_cast<float>(z) - circle.z;
				if (roadFixed.Contains(x, z) && roadFixed.At(x, z) == 0 &&
				    (dx * dx) + (dz * dz) < circle.radius * circle.radius)
				{
					roadFixed.At(x, z) = roads::k_Ring;
				}
			}
		}
	}
	for (const auto& lake : land.highLakes)
	{
		for (const auto corner : lake.corners)
		{
			// The corners near a lake keep above the level of the lake
			const int lx = heights.XOf(corner);
			const int lz = heights.ZOf(corner);
			const int reach = static_cast<int>(k_LakeShoreReach);
			for (int x = std::max(0, lx - reach); x <= std::min(side - 1, lx + reach); ++x)
			{
				for (int z = std::max(0, lz - reach); z <= std::min(side - 1, lz + reach); ++z)
				{
					auto& lowest = roadLowest.At(x, z);
					lowest = std::max(lowest, land.heightCurve.MetresOf(lake.level + k_ShoreRise));
				}
			}
		}
	}
	const roads::Ground roadGround {.metres = roadMetres, .fixed = roadFixed, .lowest = roadLowest};
	const auto graded = settings.roads.grade
	                        ? roads::Grade(roads::BedOf(land.paths, roadGround, settings.roads), roadGround, settings.roads)
	                        : roadMetres;
	for (size_t i = 0; i < graded.values.size(); ++i)
	{
		if (graded.values[i] == roadMetres.values[i])
		{
			continue;
		}
		const float height =
		    std::clamp(land.heightCurve.AltitudeOf(graded.values[i] / k_MetresPerUnit), 0.0f, terrain::k_HighestAltitude);
		heights.values[i] = height;
		const auto altitude = static_cast<uint8_t>(std::floor(height + 0.5f));
		altitudes.values[i] = altitude;
		land.heightUnits.values[i] =
		    height <= land.heightCurve.knee ? uint16_t {altitude} : land.heightCurve.WholeUnitsOf(height);
	}
	// The paths are drawn on the new ground. A corner that a cliff divides from the path, such as the top of a cliff
	// next to a path at its foot, keeps its country.
	const auto drawnPaths = paths::Draw(land.paths, pathWater, pathClear, graded, settings.roads.cliffSlope);
	clock.Done("road beds");

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
	land.cells = MakeCells(altitudes, luminosity, land.biomes, freshWater, &lakeLevels);
	land.materials = countries::UsedSurfaces();
	for (size_t biome = 0; biome < biomes::k_BiomeCount; ++biome)
	{
		land.countries.push_back(countries::MakeCountry(countries::RecipeOf(static_cast<Biome>(biome)), land.materials));
	}
	land.countries.push_back(countries::MakeCountry(countries::PathRecipe(), land.materials));
	// The edges of the lakes above the sea: wet sand on the bed and the shore, and sand on the beaches that the lakes are
	// nearer than the sea. These countries have one surface at all altitudes, because a lake can be at any height.
	for (const auto& recipe : countries::LakeShoreRecipes())
	{
		land.countries.push_back(countries::MakeCountry(recipe, land.materials));
	}
	for (size_t i = 0; i < land.cells.values.size(); ++i)
	{
		auto& properties = land.cells.values[i].properties;
		if (lakeShores.shore.values[i] == lakeshore::Shore::Bed)
		{
			properties.country = k_LakeBedCountry;
		}
		else if (land.biomes.values[i] == Biome::Beach && lakeShores.distance.values[i] < seaDistance.values[i])
		{
			properties.country = k_LakeBeachCountry;
		}
	}
	clock.Done("cells and countries");
	// No tree or rock stands on the wet sand of a lake, as on the wet sand of the sea shore
	const auto lakeBeds = Mask(side, [&lakeShores](size_t i) { return lakeShores.shore.values[i] == lakeshore::Shore::Bed; });
	auto growClear = keepClear;
	for (size_t i = 0; i < growClear.values.size(); ++i)
	{
		if (lakeBeds.values[i] != 0)
		{
			growClear.values[i] = 1;
		}
	}
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
	clock.Done("path country");
	// Nothing grows on the ground of the towns
	std::vector<features::Clearing> clearings;
	clearings.reserve(starts.size());
	for (const auto& start : starts)
	{
		clearings.push_back({.x = start.x, .z = start.z, .radius = startSettings.blendRadius});
	}
	// The town of each other player also keeps clear ground round it, so that a big forest does not stand in it
	for (size_t player = 1; player < firstTowns.size(); ++player)
	{
		clearings.push_back({.x = firstTowns[player].x, .z = firstTowns[player].z, .radius = startSettings.blendRadius});
	}
	std::vector<features::Clearing> towns;
	towns.reserve(sites.size());
	for (const auto& site : sites)
	{
		towns.push_back({.x = site.x, .z = site.z, .radius = settings.sites.blendRadius});
	}
	clearings.insert(clearings.end(), towns.begin(), towns.end());
	// The trees grow near the start of each other player as they grow near the start of player one
	for (const auto& other : land.otherStarts)
	{
		towns.push_back({.x = other.x, .z = other.z, .radius = startSettings.blendRadius});
	}
	land.features = features::Place({.biomes = land.biomes,
	                                 .heights = heights,
	                                 .seaDistance = seaDistance,
	                                 .keepClear = growClear,
	                                 .clearings = clearings,
	                                 .townX = land.start.x,
	                                 .townZ = land.start.z,
	                                 .rivers = &rivers,
	                                 .towns = towns,
	                                 .curve = land.heightCurve},
	                                seed, settings.features);
	// The trees on the banks of the streams, which stand nearer the water than the other trees. No bank tree stands on a
	// path, a lantern or the wet sand of a lake.
	const auto bankClear = Mask(side, [&](size_t i) {
		const bool onPath = drawnPaths.values[i] != 0 || (growClear.values[i] != 0 && keepClear.values[i] == 0);
		return altitudes.values[i] <= k_SeaLevelAltitude + 1 || lakeLevels.values[i] >= 0.0f || lakeBeds.values[i] != 0 ||
		       onPath;
	});
	const auto bankTrees = streams::PlaceBankTrees({.biomes = land.biomes,
	                                                .heights = heights,
	                                                .seaDistance = seaDistance,
	                                                .keepClear = bankClear,
	                                                .clearings = clearings,
	                                                .townX = land.start.x,
	                                                .townZ = land.start.z,
	                                                .rivers = &rivers,
	                                                .towns = towns,
	                                                .curve = land.heightCurve},
	                                               land.rivers, seed, settings.banks);
	land.features.insert(land.features.end(), bankTrees.begin(), bankTrees.end());
	// Each player gets woods near its town that are almost as large as the woods of the other players
	if (!land.otherStarts.empty())
	{
		std::vector<players::Start> startTowns;
		startTowns.reserve(firstTowns.size());
		for (const auto& town : firstTowns)
		{
			startTowns.push_back({.x = town.x, .z = town.z});
		}
		players::EvenTheWoods(land.features,
		                      {.biomes = land.biomes,
		                       .heights = heights,
		                       .seaDistance = seaDistance,
		                       .keepClear = growClear,
		                       .clearings = clearings,
		                       .townX = land.start.x,
		                       .townZ = land.start.z,
		                       .rivers = &rivers,
		                       .towns = towns,
		                       .curve = land.heightCurve},
		                      startTowns, settings.players);
	}
	clock.Done("features");
	auto cursed = cursed::Place({.biomes = land.biomes,
	                             .heights = heights,
	                             .seaDistance = seaDistance,
	                             .keepClear = growClear,
	                             .clearings = clearings,
	                             .townX = land.start.x,
	                             .townZ = land.start.z,
	                             .rivers = &rivers,
	                             .towns = towns,
	                             .curve = land.heightCurve},
	                            land.features, seed, settings.cursed);
	land.cursedPatches = std::move(cursed.patches);
	land.mists = std::move(cursed.mists);
	ScorchCursedGround(land, keepClear, seed, settings.cursed);
	clock.Done("cursed places");
	auto swampMist = swamps::Place({.biomes = land.biomes,
	                                .heights = heights,
	                                .seaDistance = seaDistance,
	                                .keepClear = keepClear,
	                                .clearings = clearings,
	                                .townX = land.start.x,
	                                .townZ = land.start.z,
	                                .rivers = &rivers,
	                                .towns = towns,
	                                .curve = land.heightCurve},
	                               seed, settings.swamps);
	land.swampSheets = std::move(swampMist.sheets);
	land.mists.insert(land.mists.end(), swampMist.mists.begin(), swampMist.mists.end());
	clock.Done("swamp mist");
	// The fairy rings and the lone pillars keep out of the cursed places
	std::vector<formations::Circle> cursedCircles;
	cursedCircles.reserve(land.cursedPatches.size());
	for (const auto& patch : land.cursedPatches)
	{
		cursedCircles.push_back({.x = patch.x, .z = patch.z, .radius = patch.radius});
	}
	land.formations = formations::Place({.biomes = land.biomes,
	                                     .heights = heights,
	                                     .seaDistance = seaDistance,
	                                     .keepClear = growClear,
	                                     .clearings = clearings,
	                                     .townX = land.start.x,
	                                     .townZ = land.start.z,
	                                     .rivers = &rivers,
	                                     .towns = towns,
	                                     .curve = land.heightCurve},
	                                    land.features, cursedCircles, seed, settings.formations);
	clock.Done("fairy rings and pillars");

	// No tree or rock stands on a fish farm
	std::erase_if(land.features, [&fishFarms](const features::Feature& feature) {
		return std::ranges::any_of(fishFarms, [&feature](const settlement::Field& farm) {
			return std::fabs(feature.x - farm.x) < k_FarmClearance && std::fabs(feature.z - farm.z) < k_FarmClearance;
		});
	});
	// The forests, nearest the town of the start first
	land.forests = features::GroupForests(land.features, land.settlement.town.x, land.settlement.town.z, settings.forests);
	clock.Done("forests");
	return land;
}
