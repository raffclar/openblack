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

#include <array>
#include <string_view>

#include "landgen/Grid.h"

/// The kinds of land (biomes): what grows on the land and what its ground is, from its climate and its shape
namespace openblack::landgen::biomes
{
/// Each biome is a country of the landscape, at the same index
enum class Biome : uint8_t
{
	/// The sea, the lakes and the shallows
	Sea,
	/// Wide bands of sand on the gentle slopes of a warm coast
	Beach,
	Grassland,
	TemperateForest,
	/// Palms and bushes, tightly packed, on hot and wet land
	DenseJungle,
	/// Hot and dry land behind the mountains
	Desert,
	/// Wet, low and flat land, and the filled basins
	Swamp,
	/// Burnt and dead trees on dry ground
	DeadWoods,
	/// Cold land of snow and rock with a few conifers
	NordicWastes,
	/// The high mountains: rock, then snow at the tops
	Alpine,

	_Count
};
constexpr auto k_BiomeCount = static_cast<size_t>(Biome::_Count);

constexpr std::array<std::string_view, k_BiomeCount> k_BiomeNames {"Sea",           "Beach",  "Grassland", "Temperate forest",
                                                                   "Dense jungle",  "Desert", "Swamp",     "Dead woods",
                                                                   "Nordic wastes", "Alpine"};

struct Thresholds
{
	/// The highest altitude that the game draws at the sea level
	float seaLevel {3.0f};
	/// The land at and above this altitude is alpine
	float alpineAltitude {135.0f};
	/// The land below this temperature is the nordic wastes
	float nordicTemperature {0.24f};
	/// A beach is lower than this altitude, gentler than this slope and warmer than this temperature
	float beachAltitude {16.0f};
	float beachSlope {4.0f};
	float beachTemperature {0.35f};
	/// Swamps are wetter, lower and gentler than this
	float swampMoisture {0.8f};
	float swampAltitude {40.0f};
	float swampSlope {3.0f};
	/// The jungle is warmer and wetter than this
	float jungleTemperature {0.66f};
	float jungleMoisture {0.55f};
	/// The desert is warmer and drier than this
	float desertTemperature {0.6f};
	float desertMoisture {0.33f};
	/// The dead woods are where the blight noise is above this, on land drier than the moisture
	float deadBlight {0.32f};
	float deadMoisture {0.62f};
	/// A forest is wetter than this
	float forestMoisture {0.5f};
};

/// What the land is like at one corner
struct Conditions
{
	float altitude;
	float temperature;
	float moisture;
	/// The largest difference of altitude to the next corner
	float slope;
	/// The distance to the sea in corners, and the width of the beach there in corners
	float seaDistance;
	float beachWidth;
	/// A filled basin
	bool wet;
	/// A noise from about -1 to 1 that marks where the woods died
	float blight;
};

/// The biome of a corner, by the first rule that the conditions meet: the sea, the mountains, the cold, the coast, the
/// wet land, the hot land and then the temperate land
[[nodiscard]] Biome Classify(const Conditions& conditions, const Thresholds& thresholds);

/// Gives each corner the biome that most of the corners round it have, so that small patches go
[[nodiscard]] Grid<Biome> Smooth(const Grid<Biome>& biomes, int passes);
} // namespace openblack::landgen::biomes
