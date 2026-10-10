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

#include <glm/vec2.hpp>

// How a creature fizzing out of sight is drawn (carried home, made invisible, teleported, faded by a script). The fizz,
// 0 to 1, is worked as a byte, the fraction dropped. The body is drawn in two passes: first its depth alone, wherever
// the static scrolling over its skin has an alpha no more than 5 under that byte, then the body itself blended at what
// is left of its alpha, only where that depth was laid. So the creature thins out to the background, in specks that
// grow as the static thins, and at a byte of 255 nothing of it is drawn at all.
namespace openblack::creature_fizz_look
{

/// How far it has fizzed, as a byte: the fizz times 255, the fraction dropped
[[nodiscard]] uint8_t Level(float fizz);

/// Whether any of the body is drawn: all of it isn't once the level reaches 255
[[nodiscard]] bool Drawn(float fizz);

/// Whether the body is drawn through the static: fizzed at all, and not wholly out of sight
[[nodiscard]] bool Fizzing(float fizz);

/// The least alpha, of 255, the static must have where the body shows: 5 under the level, never under 0
[[nodiscard]] uint8_t StaticThreshold(uint8_t level);

/// The alpha, of 1, the body is blended at where it shows: what the level leaves of a whole byte
[[nodiscard]] float BodyAlpha(uint8_t level);

/// The static slides across the skin a tenth of its width and a fifth of its height every second of game time it is
/// drawn, wrapping round. Each creature's own static starts unmoved.
[[nodiscard]] glm::vec2 Scroll(glm::vec2 scroll, float seconds);

/// Whether the creature's hair is drawn: only until it has fizzed or frozen a fifth of the way, and never through the
/// static
[[nodiscard]] bool HairShown(float fizz, float freeze);

/// Whether the creature's eyes are drawn through the static: a frozen eye takes the freeze's look instead
[[nodiscard]] bool EyesFizz(float fizz, float freeze);

/// Whether the creature's eyelids are drawn white rather than in the colour of the skin under them: when it is wholly
/// frozen
[[nodiscard]] bool EyelidsWhite(float freeze);

/// Whether the creature is drawn reflected in the sea: only until it has fizzed a fifth of the way out, and then whole,
/// without the static
[[nodiscard]] bool ReflectionShown(float fizz);

} // namespace openblack::creature_fizz_look
