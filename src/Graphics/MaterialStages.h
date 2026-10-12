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

/// How each primitive of a model takes the environment map and a sliding texture, as its material's flags say. The
/// game draws an environment-mapped model one primitive at a time, and each primitive's flags choose how the map, in
/// a second texture stage, is combined with the colour the first stage made; most primitives take none of it. A model
/// whose skin slides across it slides it only on the primitives whose flags let it, and never while it is drawn with an
/// environment map: the creature spells' phials, which are, keep their liquid on its first frame however their skin is
/// stepped.
namespace openblack::graphics::material_stages
{

/// How the environment map combines with what the texture and the light made
enum class EnvironmentStage : uint8_t
{
	Off,        ///< not drawn with the map at all
	Map,        ///< the map's colour alone
	Kept,       ///< drawn with the map, which leaves the colour as it was
	Modulated,  ///< the colour times the map's
	Modulated2, ///< the colour times the map's, doubled
	Added,      ///< the map's colour added to it, each channel at most white
};

/// The stage chosen by the three highest bits of the material's flags
inline constexpr std::array<EnvironmentStage, 8> k_EnvironmentStages = {
    EnvironmentStage::Off,        EnvironmentStage::Kept, EnvironmentStage::Added, EnvironmentStage::Modulated,
    EnvironmentStage::Modulated2, EnvironmentStage::Map,  EnvironmentStage::Off,   EnvironmentStage::Off,
};

/// A material whose flags have this bit keeps its texture still while the model's skin slides
inline constexpr uint8_t k_KeepsTextureStill = 0x10;

/// How a primitive of an environment-mapped model takes the map, by its material's flags
[[nodiscard]] constexpr EnvironmentStage EnvironmentStageOf(uint8_t materialFlags)
{
	return k_EnvironmentStages.at(materialFlags >> 5u);
}

/// Whether a primitive's texture slides with its model's skin: never on a model drawn with an environment map,
/// otherwise unless the primitive's material keeps it still
[[nodiscard]] constexpr bool TextureSlides(uint8_t materialFlags, bool environmentMapped)
{
	return !environmentMapped && (materialFlags & k_KeepsTextureStill) == 0;
}

} // namespace openblack::graphics::material_stages
