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

#include "Particles/LiquidParticles.h"

/// The sparkles that show where the other players of a network game look. Each game turn, each other player gets one
/// mote at the place of its camera, in a tint of the player's colour. The hands of the other players are not drawn.
namespace openblack::player_sparkles
{

/// The number of tints that a sparkle takes at random. Four of them change the colour, the other three keep it.
inline constexpr int32_t k_Tints = 7;
/// The full size of a sparkle
inline constexpr float k_Size = 10.0f;
/// The alpha of a sparkle
inline constexpr uint32_t k_Alpha = 0x80u;

/// The colour of a sparkle, 0xAARRGGBB: each channel of the player's colour at half strength, then changed by the tint
/// (0 to 6), with the sparkle's alpha. Tint 0 goes to red, 1 to yellow, 2 to green, and 3 makes the colour darker.
[[nodiscard]] uint32_t Colour(uint32_t playerColour, uint32_t tint);

/// The kind of a sparkle from a random number of 0 or 1: a mote with its frames reversed for 0, else a mote
[[nodiscard]] liquid_particles::Kind KindOf(uint32_t roll);

} // namespace openblack::player_sparkles
