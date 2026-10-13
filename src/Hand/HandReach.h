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

#include <optional>
#include <span>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"

/// What the hand reaches for when the Action button is pressed with nothing under the cursor. Every frame the game finds
/// the land behind the hand's middle (the middle of the box round its posed bones) as the camera sees it. Pressed with no
/// object picked under the cursor, the hand takes the thing standing nearest that point across the ground, if it is
/// within 5 metres of it and no further from it than the land under the cursor is. When the land behind the hand is
/// water, a fish farm whose fish show under the cursor's point is taken first: the interface never picks a farm itself.
namespace openblack::hand_reach
{

/// How far from the land behind the hand a thing may stand and still be reached, in metres
constexpr float k_Reach = 5.0f;

/// The hand's middle: the middle of the box round the places of its posed bones, the first (the root) left out. None
/// with fewer than two bones.
[[nodiscard]] std::optional<glm::vec3> Middle(std::span<const glm::vec3> bonePlaces);

/// The map cells searched about a point, from the first to the last on each axis, inclusive: those the reach takes in
/// across x and z, the cells counted the game's way through 16.16 map positions
struct Cells
{
	glm::ivec2 first;
	glm::ivec2 last;
};
[[nodiscard]] Cells CellsAround(const map_coords::MapCoords& point);

/// Of the things met in the cells' search, in its order, the one the hand takes: the nearest to the point behind the
/// hand, strictly within the reach (the first met keeps a tie), provided it is no further from that point than the land
/// under the cursor is. None otherwise.
[[nodiscard]] std::optional<size_t> Nearest(const map_coords::MapCoords& behindHand, const map_coords::MapCoords& cursor,
                                            std::span<const map_coords::MapCoords> things);

} // namespace openblack::hand_reach
