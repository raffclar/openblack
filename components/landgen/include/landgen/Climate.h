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

#include "landgen/Grid.h"

/// The climate of the land: how warm and how wet each corner is, from 0 to 1
namespace openblack::landgen::climate
{
/// The eight directions that the wind can blow from
enum class Wind : uint8_t
{
	West,
	SouthWest,
	South,
	SouthEast,
	East,
	NorthEast,
	North,
	NorthWest,
};

/// The step on the map that the wind makes, from where it blows towards where it goes. North is towards the low z.
[[nodiscard]] Step StepOf(Wind wind);

struct Settings
{
	/// The temperature at the north edge and at the south edge of the map, before the height cools it
	float northTemperature {0.22f};
	float southTemperature {0.95f};
	/// How much the highest altitude cools the land
	float heightCooling {0.75f};
	Wind wind {Wind::West};
	/// The part of the moisture in the air that falls as rain for each altitude unit that the air climbs, and on flat
	/// ground
	float climbRain {0.02f};
	float flatRain {0.004f};
	/// How far from water the land stays wet, in corners
	float waterReach {25.0f};
	/// How much the warp of the noise changes the temperature and the moisture, up or down
	float variation {0.08f};
	/// How much a finer noise moves the temperature, so the edges of the biomes are not straight
	float edgeVariation {0.06f};
};

/// The temperature of each corner: the south is warm, the north is cold, and the mountains are cold
[[nodiscard]] Grid<float> Temperature(const Grid<float>& heights, uint64_t seed, const Settings& settings);

/// The distance in corners from each corner to the nearest water corner, along the axes and the diagonals
[[nodiscard]] Grid<float> DistanceToWater(const Grid<uint8_t>& water);

/// The moisture of each corner. The wind takes moisture from the sea and lets it fall as rain as it goes, most of all
/// where it climbs the mountains, so that the land behind the mountains is dry (the rain shadow). Land near water is
/// also wet.
[[nodiscard]] Grid<float> Moisture(const Grid<float>& heights, const Grid<uint8_t>& water, uint64_t seed,
                                   const Settings& settings);
} // namespace openblack::landgen::climate
