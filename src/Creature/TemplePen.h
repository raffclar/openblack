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

#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// A creature's pen at its player's temple, and how the creature is shown smaller as it walks into it.
///
/// While its player's temple stands, a creature's home is the place in front of the temple that the temple's mesh marks
/// for it, taken again every turn. Near that place, and between the two walls that fan out from the temple's heart,
/// the creature is shown smaller, down to a newborn's size at the pen itself, so that it fits; walking out it grows back
/// the same way. Only the size it is shown at changes: what it has grown to is kept.
namespace openblack::temple_pen
{

/// The creature is shown at its own size this far from its home and farther, and at the pen's size this near and
/// nearer, in metres
inline constexpr float k_OuterRadius = 16.0f;
inline constexpr float k_InnerRadius = 14.0f;
/// The size it is shown at in the pen itself
inline constexpr float k_PenSize = 0.22f;

/// The pen's first wall is this far round from the temple's own turn, and the second wall a seventh of a turn further,
/// in radians
inline constexpr float k_FirstWallTurn = 3.83f;
inline constexpr float k_WallsApart = 0.8975979f;

/// A point's (x, z) as precisely as a map position keeps it
[[nodiscard]] glm::vec2 MapPlace(glm::vec3 point);

/// Whether a point (x, z) is on the inner side of both of the pen's walls, for a temple whose heart is at `heart` and
/// turned by `heartYAngle`. A point on a wall is inside
[[nodiscard]] bool BetweenWalls(glm::vec2 heart, float heartYAngle, glm::vec2 point);

/// The size a creature of its own size `size` is shown at, `distanceToHome` metres from its home: in reach of the pen
/// and between its walls, eased from its own size at the outer radius to the pen's size at the inner one; elsewhere its
/// own
[[nodiscard]] float ShownSize(float size, float distanceToHome, bool betweenWalls);

} // namespace openblack::temple_pen
