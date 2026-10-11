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

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "Enums.h"

/// The columns of light that stand over every village centre a player owns and over each player's temple. Both are
/// the same tall half tube of light, turned about the upright to face the camera and added over the scene in their
/// owner's colour: the village centres' faint and steady, the temple's beating with its owner's heart.
namespace openblack::graphics::light_column
{

/// How strongly a village centre's column is added, of 255
inline constexpr uint8_t k_TownAlpha = 20;
/// The temple's column beats between these strengths, of 255
inline constexpr float k_TempleAlphaLow = 55.0f;
/// The column keeps its model's width and is stretched this much upright. The game also gives the column's object a
/// size of 3, but its model is drawn by the turn and stretch alone, so that size changes nothing on screen.
inline constexpr float k_UprightStretch = 3.0f;
/// A temple being destroyed keeps its column for this long into its destruction
inline constexpr float k_DestructionSeconds = 21.8f;

/// A column shows only for a player's things, and only once that player's influence border shows: from the first time
/// their temple stands wholly built on the land, so not over a temple still going up for a player who has had none
/// built
[[nodiscard]] bool Shown(PlayerNames owner, bool borderShown);

/// The temple's column shows until this far into its destruction
[[nodiscard]] bool TempleShown(bool destroying, float destructionClock);

/// The temple's strength for its owner's heartbeat pulse (0 between beats, 1 at the top of a beat): 55 to 110 of 255,
/// rounded down
[[nodiscard]] uint8_t TempleAlpha(float pulse);

/// The column's colour: the owner's colour (0xRRGGBB, any alpha above it ignored) at a strength of 255
[[nodiscard]] glm::vec4 Tint(uint32_t colour, uint8_t alpha);

/// Where the column's model is drawn: standing at a point, turned about the upright so that it faces the eye, as wide
/// as its model and stretched three times upright
[[nodiscard]] glm::mat4 Model(const glm::vec3& position, const glm::vec3& eye);

} // namespace openblack::graphics::light_column
