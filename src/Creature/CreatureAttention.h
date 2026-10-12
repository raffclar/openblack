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

#include <glm/vec3.hpp>

/// How much attention a god pays its creature: every game turn the creature lives, it notes whether its player's camera
/// is looking at it, and the share of its life it was looked at is the attention its mind scroll tells of. Everything
/// here is pure.
namespace openblack::creature_attention
{

/// The camera is looking at a creature closer to it than this
inline constexpr float k_LookingRange = 400.0f;
/// and within this angle of the way it looks: two thirds of seventy degrees
inline constexpr float k_LookingAngle = 1.2217305f * 0.6666667f;

/// Whether a camera at `origin` looking towards `focus` is looking at a creature standing at `creature`: the creature
/// is nearer than the range and within the angle of the way the camera looks. A creature right at the camera, or a
/// camera looking nowhere, is not looked at.
[[nodiscard]] bool IsLookingAt(const glm::vec3& origin, const glm::vec3& focus, const glm::vec3& creature);

/// The share of the turns a creature has lived that its player looked at it, 0 before it has lived a turn
[[nodiscard]] double Share(uint32_t turnsLookedAt, uint32_t turnsLived);

/// The five words for how much attention, as a number from 0 (the least) to 4: a fifth of the share each, the most
/// for all from four fifths up (and, as the game counts, for a share below nothing)
[[nodiscard]] int32_t HowMuch(double share);

} // namespace openblack::creature_attention
