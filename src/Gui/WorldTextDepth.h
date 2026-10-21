/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <glm/mat4x4.hpp>

/// The depth at which the game draws a flat box of words over a point in the world, such as a villager's label.
///
/// The box, its pointer and its words are on the screen, but the game tests them against the depth of the world: a
/// hill or a house in front of the box hides the part behind it. The game does not put the box at the depth of its
/// point. It moves the box towards the camera: 5 nearer when the camera looks level, and as far as two times the near
/// plane when the camera looks straight down. The game writes no depth for the box.
namespace openblack::gui::world_text
{

/// The box is this much nearer to the camera than its point when the camera looks level
inline constexpr float k_LevelPull = 5.0f;

/// The depth in front of the camera at which the game draws a box over a point at a depth. The pitch is the camera's
/// pitch from its focus, 0 when it looks level, and its sign does not change the result.
[[nodiscard]] float DrawnDepth(float pointDepth, float cameraPitch, float nearClip);

/// The game draws words in the world at least half a unit beyond the near plane
inline constexpr float k_WordsBeyondNear = 0.5f;

/// The depth at which the game draws words that it asks for at a depth: never nearer than half a unit beyond the near
/// plane
[[nodiscard]] float WordsDepth(float depth, float nearClip);

/// The value that a projection gives the depth buffer for a depth in front of the camera
[[nodiscard]] float BufferDepth(const glm::mat4& projection, float depth);

} // namespace openblack::gui::world_text
