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
#include "landgen/Climate.h"
#include "landgen/Countries.h"
#include "landgen/Features.h"
#include "landgen/Grid.h"
#include "landgen/Hydrology.h"
#include "landgen/Lighting.h"
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

/// The ambient sound of the land of a biome
[[nodiscard]] Sound SoundOf(biomes::Biome biome);

struct Settings
{
	uint64_t seed {1};
	int side {k_MapCorners};
	/// The direction of the wind. Without it, the seed picks the direction.
	std::optional<climate::Wind> wind;
	terrain::BaseSettings base;
	terrain::HydraulicSettings hydraulic;
	terrain::ThermalSettings thermal;
	hydrology::LakeSettings lakes;
	hydrology::RiverSettings rivers;
	climate::Settings climate;
	biomes::Thresholds thresholds;
	features::Settings features;
	lighting::Settings lighting;
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
	/// The cell of each corner of the map, as a landscape keeps them: altitude, luminosity, country and water
	Grid<lnd::LNDCell> cells;
	Grid<biomes::Biome> biomes;
	/// The country of each biome, at the index of the biome
	std::vector<lnd::LNDCountry> countries;
	/// The surface of each material of the land, at the index that the countries use
	std::vector<countries::Surface> materials;
	std::vector<hydrology::River> rivers;
	size_t lakes {0};
	std::vector<features::Feature> features;
	StartArea start {};
	std::vector<StageTime> times;
};

/// Makes a land: the base heights, the erosion, the lakes and rivers, the start area, the climate, the biomes, the light,
/// the cells and countries, and the features
[[nodiscard]] GeneratedLand Generate(const Settings& settings);

/// The cell of each corner, from the altitudes, the luminosity and the biomes. A cell's water and sound come from the
/// four corners of the cell, as on the lands of the story.
[[nodiscard]] Grid<lnd::LNDCell> MakeCells(const Grid<uint8_t>& altitudes, const Grid<uint8_t>& luminosity,
                                           const Grid<biomes::Biome>& biomes, const Grid<uint8_t>& freshWater,
                                           const Grid<uint8_t>& runningWater);
} // namespace openblack::landgen
