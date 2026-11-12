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
#include <span>
#include <vector>

#include <LNDFile.h>

#include "landgen/Biomes.h"

/// The countries of the landscape: for each biome, the ground that the land has at each altitude. The game paints the
/// land with them as it paints the lands of the story, so the snow on the mountains comes from the altitude.
namespace openblack::landgen::countries
{
/// A kind of ground. The game gives each one a material of the landscapes of the story.
enum class Surface : uint8_t
{
	WetSand,
	Sand,
	DesertSand,
	Grass,
	LushGrass,
	DryGrass,
	Earth,
	Mud,
	WetGrass,
	JungleFloor,
	Heather,
	Scrub,
	Gravel,
	Rock,
	DarkRock,
	Snow,
	SnowyTops,
	Ice,

	_Count
};
constexpr auto k_SurfaceCount = static_cast<size_t>(Surface::_Count);

/// The ground of a country from an altitude up to the next band
struct Band
{
	uint8_t altitude;
	Surface surface;
};

/// The bands of a country, from the sea bed up, and the altitudes over which a band blends into the next one. The
/// first band starts at altitude 0.
struct Recipe
{
	std::span<const Band> bands;
	uint8_t blend;
};

/// The recipe of the country of each biome
[[nodiscard]] Recipe RecipeOf(biomes::Biome biome);

/// The country table of a recipe. Each entry names the materials by their index in materials, which holds a surface for
/// each material of the land. An entry that blends two materials gives the first one the coefficient out of 255.
[[nodiscard]] lnd::LNDCountry MakeCountry(const Recipe& recipe, std::span<const Surface> materials);

/// The coefficient of the upper material at an altitude inside a blend that starts at start and is length altitudes
/// long: from about 1/length at the start to 255 at the end, rounded to the nearest, half up
[[nodiscard]] constexpr uint32_t BlendCoefficient(int altitude, int start, int length)
{
	return static_cast<uint32_t>(((((altitude - start) + 1) * 510) + length) / (2 * length));
}

/// The surfaces that the countries of all the biomes use, in the order of the surfaces
[[nodiscard]] std::vector<Surface> UsedSurfaces();
} // namespace openblack::landgen::countries
