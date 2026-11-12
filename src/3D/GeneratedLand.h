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
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <LNDFile.h>
#include <landgen/Generator.h>

#include "3D/LandData.h"

namespace openblack::filesystem
{
class FileSystemInterface;
}

/// A land from the land generator (components/landgen), made into what the game loads. The generator gives the cells,
/// the countries and the surface of each material. The materials, the noise and the bump map come from the landscapes
/// of the story, so the ground looks as it does on those lands.
namespace openblack::generated_land
{
/// A generated land has no file. Its path names its seed, so that the game can make the land again.
constexpr std::string_view k_PathPrefix = "landgen:";
[[nodiscard]] std::string PathOf(uint64_t seed);
/// The seed of the path of a generated land, or nothing for another path
[[nodiscard]] std::optional<uint64_t> SeedOf(std::string_view path);

/// A material of a landscape of the story: the file and the index of the material in it
struct MaterialSource
{
	std::string_view landscape;
	uint32_t material;
};

/// The material of the story that each surface of the generator takes, at the index of the surface
[[nodiscard]] const std::array<MaterialSource, landgen::countries::k_SurfaceCount>& MaterialSources();

/// The landscape whose noise and bump maps a generated land takes
constexpr std::string_view k_NoiseLandscape = "Land1.lnd";

/// What the land takes from the landscapes of the story
struct StoryParts
{
	/// The material of each surface, at the index of the surface
	std::vector<lnd::LNDMaterial> surfaces;
	std::vector<uint8_t> noise;
	std::vector<uint8_t> bump;
};

/// Reads the materials, the noise and the bump maps from the landscapes in the game folder. Gives nothing when a
/// landscape is not there or does not read.
[[nodiscard]] std::optional<StoryParts> ReadStoryParts(filesystem::FileSystemInterface& files);

/// The land data of a generated land: a block for each block of the map that is not all open sea, in order along z then
/// x, and the land's countries and materials. surfaces holds the material of each surface.
[[nodiscard]] LandData Build(const landgen::GeneratedLand& land, const StoryParts& parts);

/// The settings of the generator for the land of a seed
[[nodiscard]] landgen::Settings SettingsFor(uint64_t seed);
} // namespace openblack::generated_land
