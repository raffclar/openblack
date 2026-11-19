/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <optional>
#include <string_view>

#include "landgen/Climate.h"
#include "landgen/Terrain.h"

/// The types of land that the seed picks: the size and the shape of the land before the generator makes its terrain.
/// Each land of the story is one island of 1.1 to 3.9 square kilometres. The other types are larger or have a
/// different shape, as the user asked for generated lands.
namespace openblack::landgen::landtypes
{
enum class Type : uint8_t
{
	/// One island of the size of a land of the story
	StoryIsland,
	/// One large island that fills most of the map
	BigIsland,
	/// A high plateau with terraces that rises from a narrow coast
	Highlands,
	/// Mountain ranges with valleys between them
	Valleys,
	/// A group of small islands. The seed picks it only when the settings let it.
	Archipelago,
};
constexpr size_t k_TypeCount = 5;

/// The name of the type for the editor and the reports
[[nodiscard]] std::string_view NameOf(Type type);

struct Settings
{
	/// The type of the land. Without it, the seed picks the type.
	std::optional<Type> type;
	/// The seed picks an archipelago only when this is true
	bool archipelago {false};
	/// How often the seed picks each type, in the order of Type
	std::array<uint32_t, k_TypeCount> weights {4, 2, 2, 2, 2};
};

/// The type of the land: the type of the settings, or a type that the seed picks with the weights
[[nodiscard]] Type Pick(uint64_t seed, const Settings& settings);

/// The fewest and the most islands of an archipelago
constexpr int k_FewestIslands = 3;
constexpr int k_MostIslands = 5;

/// The height in metres of the highest altitude on the lands of the story
constexpr float k_StoryTopHeight = 170.85f;

/// Changes the settings of the shape and the climate for the type. The changes start from the settings that they get,
/// so that the other choices of the land keep their effect. A big island changes nothing. A smaller island has a lower
/// top height (in metres), so that its mountains are as steep as the mountains of the big island.
void Apply(Type type, uint64_t seed, terrain::BaseSettings& base, terrain::MassifSettings& massifs, climate::Settings& climate,
           float& topHeight);
} // namespace openblack::landgen::landtypes
