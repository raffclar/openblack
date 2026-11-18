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

#include "3D/GeneratedLandChoices.h"
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
/// The settings of the generator for the choices
[[nodiscard]] landgen::Settings SettingsOf(const Choices& choices);

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

/// The name of a tribe of the plan, as a land script gives it
[[nodiscard]] std::string_view TribeName(landgen::settlement::Tribe tribe);

/// The lines of a land script that make the temple and the towns of a generated land, with their people and fields, as
/// a land of the story makes them. The town of the start has the number 0 and belongs to player one. The neutral towns
/// have the numbers after it. The lines use only kinds of the game.
[[nodiscard]] std::string SettlementScript(const landgen::settlement::Plan& plan);

/// The settings of the generator for the land of a seed, with each other choice at its first value
[[nodiscard]] landgen::Settings SettingsFor(uint64_t seed);
} // namespace openblack::generated_land
