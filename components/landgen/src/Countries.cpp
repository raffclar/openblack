/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "landgen/Countries.h"

#include <algorithm>
#include <iterator>

using namespace openblack::landgen;
using namespace openblack::landgen::countries;
using biomes::Biome;

namespace
{
// The sea bed and the coast: wet sand under the water, then dry sand
constexpr std::array<Band, 2> k_Sea {{{0, Surface::WetSand}, {8, Surface::Sand}}};
constexpr std::array<Band, 4> k_Beach {{{0, Surface::WetSand}, {6, Surface::Sand}, {34, Surface::Grass}, {120, Surface::Rock}}};
constexpr std::array<Band, 4> k_Grassland {
    {{0, Surface::WetSand}, {8, Surface::Grass}, {115, Surface::Rock}, {185, Surface::Snow}}};
constexpr std::array<Band, 5> k_TemperateForest {
    {{0, Surface::WetSand}, {8, Surface::LushGrass}, {95, Surface::Heather}, {130, Surface::Rock}, {185, Surface::Snow}}};
constexpr std::array<Band, 4> k_DenseJungle {
    {{0, Surface::WetSand}, {8, Surface::JungleFloor}, {125, Surface::Rock}, {200, Surface::Snow}}};
constexpr std::array<Band, 4> k_Desert {
    {{0, Surface::Sand}, {10, Surface::DesertSand}, {115, Surface::Rock}, {205, Surface::Snow}}};
constexpr std::array<Band, 4> k_Swamp {{{0, Surface::Mud}, {8, Surface::WetGrass}, {120, Surface::Rock}, {185, Surface::Snow}}};
constexpr std::array<Band, 5> k_DeadWoods {
    {{0, Surface::WetSand}, {8, Surface::DryGrass}, {55, Surface::Scrub}, {125, Surface::DarkRock}, {195, Surface::Snow}}};
// The nordic wastes are under snow down to near the sea, with bare rock and ice
constexpr std::array<Band, 4> k_NordicWastes {
    {{0, Surface::Gravel}, {10, Surface::Rock}, {24, Surface::Snow}, {160, Surface::Ice}}};
// The mountains: grass on the lowest slopes, rock, then snow and the snowy tops
constexpr std::array<Band, 4> k_Alpine {
    {{0, Surface::Grass}, {125, Surface::Rock}, {175, Surface::Snow}, {225, Surface::SnowyTops}}};

constexpr uint8_t k_Blend = 24;

uint32_t IndexOf(Surface surface, std::span<const Surface> materials)
{
	const auto it = std::ranges::find(materials, surface);
	return it == materials.end() ? 0u : static_cast<uint32_t>(std::distance(materials.begin(), it));
}
} // namespace

Recipe countries::RecipeOf(Biome biome)
{
	switch (biome)
	{
	case Biome::Sea:
		return {.bands = k_Sea, .blend = k_Blend};
	case Biome::Beach:
		return {.bands = k_Beach, .blend = k_Blend};
	case Biome::Grassland:
		return {.bands = k_Grassland, .blend = k_Blend};
	case Biome::TemperateForest:
		return {.bands = k_TemperateForest, .blend = k_Blend};
	case Biome::DenseJungle:
		return {.bands = k_DenseJungle, .blend = k_Blend};
	case Biome::Desert:
		return {.bands = k_Desert, .blend = k_Blend};
	case Biome::Swamp:
		return {.bands = k_Swamp, .blend = k_Blend};
	case Biome::DeadWoods:
		return {.bands = k_DeadWoods, .blend = k_Blend};
	case Biome::NordicWastes:
		return {.bands = k_NordicWastes, .blend = k_Blend};
	case Biome::Alpine:
	case Biome::_Count:
		break;
	}
	return {.bands = k_Alpine, .blend = k_Blend};
}

openblack::lnd::LNDCountry countries::MakeCountry(const Recipe& recipe, std::span<const Surface> materials)
{
	lnd::LNDCountry country {};
	country.type = 0;
	size_t band = 0;
	for (int altitude = 0; altitude < static_cast<int>(country.materials.size()); ++altitude)
	{
		while (band + 1 < recipe.bands.size() && altitude >= recipe.bands[band + 1].altitude)
		{
			++band;
		}
		const auto here = IndexOf(recipe.bands[band].surface, materials);
		auto& entry = country.materials.at(static_cast<size_t>(altitude));
		entry = {.indices = {here, here}, .coefficient = 255};
		if (band + 1 < recipe.bands.size())
		{
			// The blend into the next band ends just below the altitude where that band starts
			const int next = recipe.bands[band + 1].altitude;
			const int length = std::min<int>(recipe.blend, next - recipe.bands[band].altitude);
			const int start = next - length;
			if (length > 0 && altitude >= start)
			{
				const auto above = IndexOf(recipe.bands[band + 1].surface, materials);
				entry = {.indices = {above, here}, .coefficient = BlendCoefficient(altitude, start, length)};
			}
		}
	}
	return country;
}

std::vector<Surface> countries::UsedSurfaces()
{
	std::array<bool, k_SurfaceCount> used {};
	for (size_t biome = 0; biome < biomes::k_BiomeCount; ++biome)
	{
		for (const auto& band : RecipeOf(static_cast<Biome>(biome)).bands)
		{
			used.at(static_cast<size_t>(band.surface)) = true;
		}
	}
	std::vector<Surface> surfaces;
	for (size_t surface = 0; surface < k_SurfaceCount; ++surface)
	{
		if (used.at(surface))
		{
			surfaces.push_back(static_cast<Surface>(surface));
		}
	}
	return surfaces;
}
