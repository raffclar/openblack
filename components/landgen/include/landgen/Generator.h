/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>
#include <string_view>
#include <vector>

#include <LNDFile.h>

#include "landgen/Biomes.h"
#include "landgen/Cliffs.h"
#include "landgen/Climate.h"
#include "landgen/Countries.h"
#include "landgen/Cursed.h"
#include "landgen/Features.h"
#include "landgen/Formations.h"
#include "landgen/Grid.h"
#include "landgen/HeightCurve.h"
#include "landgen/Hydrology.h"
#include "landgen/Lakes.h"
#include "landgen/LandTypes.h"
#include "landgen/Lighting.h"
#include "landgen/Paths.h"
#include "landgen/Players.h"
#include "landgen/RoadBed.h"
#include "landgen/Settlement.h"
#include "landgen/Streams.h"
#include "landgen/Swamps.h"
#include "landgen/Terrain.h"

/// The generator of lands: from a seed and its settings, a whole land that the game loads as it loads a landscape of the
/// story. The same seed and settings give the same land on every platform.
namespace openblack::landgen
{
/// The corners along each side of the map of the game: 32 blocks of 16 cells
constexpr int k_MapCorners = 513;

/// The ambient sounds of the land, with the game's numbers. A cell keeps its sound in bits 2 to 5 of its flags.
enum class Sound : uint8_t
{
	Sea = 1,
	StillFreshWater = 2,
	Coastal = 3,
	Jungle = 4,
	Arctic = 5,
	Desert = 6,
	Countryside = 7,
	Swamp = 8,
	RunningWater = 9,
};
/// The flag of a cell of open water, which shows the sea rather than land
constexpr uint8_t k_OpenWaterFlag = 0x02;
/// The country of the corners under a path, after the countries of the biomes
constexpr uint8_t k_PathCountry = static_cast<uint8_t>(biomes::k_BiomeCount);
/// The countries of the edges of the lakes above the sea, after the country of the paths: the bed, then the beach
constexpr uint8_t k_LakeBedCountry = k_PathCountry + 1;
constexpr uint8_t k_LakeBeachCountry = k_PathCountry + 2;

/// The ambient sound of the land of a biome
[[nodiscard]] Sound SoundOf(biomes::Biome biome);

struct Settings
{
	uint64_t seed {1};
	int side {k_MapCorners};
	/// The type of the land, which changes the shape settings below before the generator uses them
	landtypes::Settings landType;
	/// The direction of the wind. Without it, the seed picks the direction.
	std::optional<climate::Wind> wind;
	/// The height in metres of the highest altitude, and the altitude above which each altitude unit adds more height.
	/// The lands of the story have 170.85 m at the highest altitude. Snowdon, the highest mountain of Wales, is 1085 m.
	float topHeight {1250.0f};
	float heightKnee {73.471f};
	terrain::BaseSettings base;
	terrain::HydraulicSettings hydraulic;
	terrain::ThermalSettings thermal;
	terrain::TerraceSettings terraces;
	terrain::MassifSettings massifs;
	terrain::RollSettings roll;
	/// The steepest faces of the cliffs
	cliffs::FaceSettings faces;
	terrain::ShelfSettings shelf;
	hydrology::LakeSettings lakes;
	hydrology::RiverSettings rivers;
	/// The streams that the land keeps from its rivers, and the trees on their banks
	streams::Settings streams;
	streams::BankSettings banks;
	/// The lakes above the sea, and the round hollows on the hills that hold some of them
	lakes::Settings highLakes;
	lakes::DigSettings hillHollows;
	climate::Settings climate;
	biomes::Thresholds thresholds;
	features::Settings features;
	features::ForestSettings forests;
	cursed::Settings cursed;
	swamps::Settings swamps;
	formations::Settings formations;
	lighting::Settings lighting;
	settlement::Settings settlement;
	settlement::SiteSettings sites;
	paths::Settings paths;
	/// The bed of the paths, cut and filled into the land as the bed of a built road
	roads::Settings roads;
	settlement::TownMix mix;
	settlement::FarmSettings farms;
	/// The players of the land and the start areas of the other players
	players::Settings players;
};

/// A flat place for the player's temple and first town, in corners
struct StartArea
{
	float x;
	float z;
	float radius;
};

struct StartSettings
{
	/// The flat ground round the middle, and the ground beyond it that slopes back to the land
	float flatRadius {9.0f};
	float blendRadius {15.0f};
	/// The start is between these altitudes and between these distances from the sea, in corners
	float lowest {10.0f};
	float highest {70.0f};
	float nearestSea {15.0f};
	float farthestSea {90.0f};
	/// The start keeps this far from a river, in corners
	float riverDistance {10.0f};
};

/// The flattest place that meets the settings, near the middle of the map from north to south, where the climate is
/// mild. Without such a place, the middle of the map.
[[nodiscard]] StartArea FindStartArea(const Grid<float>& heights, const Grid<float>& seaDistance,
                                      const Grid<float>& riverDistance, const StartSettings& settings);

/// Flattens the start area to the mean height of its middle, and blends the ground round it into the land
void FlattenStartArea(Grid<float>& heights, const StartArea& start, const StartSettings& settings);

/// The time that a stage of the generator took
struct StageTime
{
	std::string_view stage;
	double milliseconds;
};

/// A land from the generator: what a landscape holds, and the things on it
struct GeneratedLand
{
	/// The type of the land, from the settings or from the seed
	landtypes::Type type {landtypes::Type::BigIsland};
	/// The cell of each corner of the map, as a landscape keeps them: altitude, luminosity, country and water
	Grid<lnd::LNDCell> cells;
	Grid<biomes::Biome> biomes;
	/// The country of each biome, at the index of the biome
	std::vector<lnd::LNDCountry> countries;
	/// The surface of each material of the land, at the index that the countries use
	std::vector<countries::Surface> materials;
	std::vector<hydrology::River> rivers;
	size_t lakes {0};
	/// The lakes above the sea, each at its own level in altitude units
	std::vector<lakes::Lake> highLakes;
	std::vector<features::Feature> features;
	/// The forests that hold the trees of the features
	std::vector<features::Forest> forests;
	/// The cursed places
	std::vector<cursed::Patch> cursedPatches;
	/// The sheets of green mist over the swamps
	std::vector<swamps::Sheet> swampSheets;
	/// The fairy rings and the groups of lone pillars
	formations::Result formations;
	/// The puffs of mist and smoke over the cursed places, then the puffs of the green mist over the swamps
	std::vector<cursed::Mist> mists;
	StartArea start {};
	/// The player's temple and first town on the start area, and the neutral towns. The paths between the towns start
	/// from the centres of these towns.
	settlement::Plan settlement;
	/// The paths between the temple and the towns, in the order that the generator laid them, and the lanterns along
	/// some of them
	std::vector<paths::Path> paths;
	std::vector<paths::Lantern> lanterns;
	/// The start areas of players two and up, and the temple and first town of each of them. The plans have no neutral
	/// towns.
	std::vector<StartArea> otherStarts;
	std::vector<settlement::Plan> otherPlayers;
	std::vector<StageTime> times;
	/// The height units of each altitude, and the height of each corner in units of 0.67 m. The altitude of a cell is its
	/// height in units on a land of the story, and the rounded altitude of the curve on a generated land.
	HeightCurve heightCurve;
	Grid<uint16_t> heightUnits;
};

/// Makes a land: the type of the land, the base heights, the erosion, the lakes and rivers, the start area and the places of
/// the towns, the climate, the biomes, the light, the cells and countries, the temple and town of the start, the neutral
/// towns, the paths between them and their lanterns, the features, the cursed places, the mist of the swamps, the fairy
/// rings and lone pillars, and the forests
[[nodiscard]] GeneratedLand Generate(const Settings& settings);

/// The cell of each corner, from the altitudes, the luminosity and the biomes. A cell's water and sound come from the
/// four corners of the cell, as on the lands of the story. A cell under a lake above the sea (lakeLevels, below 0 where no
/// lake is) has water, and a cell on its shore is on the coast, both with the sound of still fresh water. As on the lands
/// of the story, the cells by a stream have the sound of their biome, not the sound of running water.
[[nodiscard]] Grid<lnd::LNDCell> MakeCells(const Grid<uint8_t>& altitudes, const Grid<uint8_t>& luminosity,
                                           const Grid<biomes::Biome>& biomes, const Grid<uint8_t>& freshWater,
                                           const Grid<float>* lakeLevels = nullptr);
} // namespace openblack::landgen
