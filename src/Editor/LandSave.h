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

#include <span>
#include <string>
#include <string_view>

#include <glm/vec2.hpp>

#include "3D/LandData.h"

/// What the editor saves of a land: the land's cells into the data of a landscape file, and the lines of a land script
/// for what it placed and drew, as plain rules
namespace openblack::editor::land_save
{

/// Copies the cells of the blocks of the land as it is now into the data of its landscape file. Each block goes to the
/// block of the data at the same place on the map. Gives the count of the blocks that it copied.
size_t CopyCells(LandData& land, std::span<const lnd::LNDBlock> blocks);

/// A tree as the land script makes it again
struct TreeLine
{
	/// The number of its forest, -1 for none
	int32_t forest {-1};
	glm::ivec2 place {0, 0};
	int32_t type {0};
	bool nonScenic {false};
	/// Its turn about the up axis, in radians, as the script gives it
	float yAngle {0.0f};
	float scale {1.0f};
	float maxScale {1.0f};
};

/// The line that makes a tree again. The game writes its turn, its size now and its largest size.
[[nodiscard]] std::string TreeText(const TreeLine& tree);

/// A feature as the land script makes it again
struct FeatureLine
{
	glm::ivec2 place {0, 0};
	/// The name of its row of the features' table
	std::string name;
	/// Its turn about the up axis, in radians, as the script gives it
	float yAngle {0.0f};
	float scale {1.0f};
};

/// The line that makes a feature again. The game writes its turn and its size each times 1000, rounded to a float and
/// cut to a whole number, and a last number that is 0 for the features that a land script makes.
[[nodiscard]] std::string FeatureText(const FeatureLine& feature);

/// The lines that make a stream again: the stream with its number, then each of its points in order
[[nodiscard]] std::string StreamText(int32_t id, std::span<const glm::ivec2> places);

/// The lines that make a footpath again: a new footpath, which becomes the script's footpath 0, then each of its nodes
/// in order
[[nodiscard]] std::string FootpathText(std::span<const glm::ivec2> places);

/// The name of a saved file: the land's name with the extension of the file
[[nodiscard]] std::string FileName(std::string_view landName, std::string_view extension);

} // namespace openblack::editor::land_save
