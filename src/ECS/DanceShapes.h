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

#include <glm/vec2.hpp>

/// The shapes a dance's groups stand in: 256 points round each, 65535 to the group's radius. The game works them out
/// as it starts, with every float operation rounded to a float, and reads the rest from its data folder.
namespace openblack::ecs::dance_shapes
{

inline constexpr std::size_t k_PointsPerShape = 256;
inline constexpr std::size_t k_ShapeCount = 46;
/// The shapes from this one on are read from files in the game's data folder, named after them
inline constexpr std::size_t k_FirstFileShape = 13;

using Shape = std::array<glm::ivec2, k_PointsPerShape>;

/// A shape the game works out: the circle, the wavy circle, the spiral, the wave, the square, the two lines, the point,
/// the octagon and the line out from the centre. The triangle, the heart and the random shape, which no dance uses, and
/// the shapes read from files, are all at the centre.
[[nodiscard]] Shape Build(std::size_t index);

/// The name of a shape read from a file, its file being data\<name>.dan; empty for one the game works out
[[nodiscard]] const char* FileName(std::size_t index);

} // namespace openblack::ecs::dance_shapes
