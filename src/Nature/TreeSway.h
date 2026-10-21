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

/// The sway of the trees and the ripe crops: a slow back and forth along the world's z axis, whatever the weather. The
/// wind does not change it.
namespace openblack::nature::tree_sway
{

/// The trees share this many sways
inline constexpr uint32_t k_Count = 16;
/// Each sway takes a new random speed after more than this many milliseconds of game time
inline constexpr uint32_t k_SpeedChangeMilliseconds = 2000;
/// The least and the most speed of a sway
inline constexpr float k_MinSpeed = 1.0f;
inline constexpr float k_MaxSpeed = 2.0f;

/// The phase of a sway after a frame: it moves on by the frame's game time times its speed, in the game's order
[[nodiscard]] float StepPhase(float phase, float speed, uint32_t frameMilliseconds);

/// How far a sway leans a tree along the world's z axis at a phase, per unit of the tree's height: at most 3%
[[nodiscard]] float Lean(float phase);

} // namespace openblack::nature::tree_sway
