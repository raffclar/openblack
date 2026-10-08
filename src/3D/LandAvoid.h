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

#include <filesystem>
#include <functional>
#include <optional>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace openblack
{
class LandIslandInterface;

/// The creature's walkable mask (512 x 512 bytes, [z][x], one per landscape cell): built once per landscape, when it
/// opens, by a validation pass and a flood fill. The creature may stand where it is 0 or 6 and at least 7.1 from the
/// centre of any other nearby cell (IsPosValid):
/// it wades into the sea up to where the four corners of the cells are at altitude 0, and never swims.
///
/// The creature's locomotion reads it (not called from the game yet); OPENBLACK_DUMP_LAND_AVOID writes it as a picture.
namespace land_avoid
{

// The byte values of the mask, as At() returns them
constexpr uint8_t k_Land = 0;  ///< land reached by the flood
constexpr uint8_t k_Avoid = 1; ///< too steep or deep sea (all 4 corners at altitude 0) next to the reached land
/// land the flood did not reach, and the too steep / deep cells away from the reached land
constexpr uint8_t k_Unreachable = 2;
/// reached land whose cell has the water bit (or no block): water the creature can walk in
constexpr uint8_t k_Water = 6;

/// The creature's radius for most checks (giving it an object, the movement, the script's PosValidForCreature)
constexpr float k_CreatureRadius = 7.1f;
/// The creature's radius for turning
constexpr float k_CreatureTurningRadius = 7.05f;

/// The validation and the flood for the island's landscape (the original's 512 x 512 becomes
/// GetCellsPerSide() squared, so BWLandEditor maps get their whole grid)
void Validate(const LandIslandInterface& island);
/// Drops the mask (no island)
void Clear();

/// The mask at [z][x]; k_Unreachable off the map or before Validate
[[nodiscard]] uint8_t At(int32_t x, int32_t z);
/// The cell of `position` ((int)(x * 0.1), (int)(z * 0.1)) is in the map and 0 or 6, and no cell of the
/// 3 x 3 around it (in the map) that is not 0 or 6 has its centre (10 i + 5, 10 j + 5) closer than `radius` in xz
[[nodiscard]] bool IsPosValid(glm::vec3 position, float radius = k_CreatureRadius);

/// Where a creature of `radius` can stand nearest `point` (x, z) by `valid`: the point itself, else the first of 16
/// directions (from +x, turning towards +z) at 0.5, 1, 1.5 ... up to `maxDistance` away; nothing within it
[[nodiscard]] std::optional<glm::vec2> NearestValid(glm::vec2 point, float radius, float maxDistance,
                                                    const std::function<bool(glm::vec3, float)>& valid);
/// NearestValid by IsPosValid
[[nodiscard]] std::optional<glm::vec2> NearestValid(glm::vec2 point, float radius, float maxDistance);

/// A picture of the mask, one pixel per cell (x right, z down): 0 green, 6 blue, 1 red, 2 grey (any other value
/// magenta). False if there is no mask or the file cannot be written.
bool DumpPng(const std::filesystem::path& path);
/// Test hook, after Validate: OPENBLACK_DUMP_LAND_AVOID=1 (land_avoid.png in the working directory) or =<file.png>
void DumpIfRequested();

} // namespace land_avoid
} // namespace openblack
