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

#include <optional>

/// The rules of the ring of light that a particle rule stands round each of its atoms, as the singing stones' heal does:
/// a ring of light on the land round the place where the atom was made, in the colour of the effect's player, as bright
/// as the atom is opaque
namespace openblack::particles::object_light_sheet
{

/// The rule makes this many atoms, unless its file gives another number
inline constexpr int k_DefaultAtoms = 1;
/// The radius of the ring when the rule has no value for it
inline constexpr float k_DefaultRadius = 20.0f;
/// The height of the ring's light
inline constexpr float k_Height = 10.0f;
/// The top of the ring is spread out from its middle by this
inline constexpr float k_Spread = 1.1f;
/// The strength that the ring's light has at all times
inline constexpr float k_Strength = 1.0f;
/// The colour of an atom whose effect has no player, 0xAARRGGBB
inline constexpr uint32_t k_NoPlayerArgb = 0xFF777777u;
/// The colour that an atom which the rule did not make has, 0xAARRGGBB
inline constexpr uint32_t k_OtherAtomArgb = 0xFFFFFFFFu;

/// The colour that the rule gives to the atoms it makes, 0xAARRGGBB: the effect's player's colour (0xRRGGBB), opaque, or
/// grey when the effect has no player
[[nodiscard]] constexpr uint32_t AtomArgb(std::optional<uint32_t> playerRgb)
{
	return playerRgb.has_value() ? (0xFF000000u | (*playerRgb & 0xFFFFFFu)) : k_NoPlayerArgb;
}

/// The colour of an atom's ring, 0xAARRGGBB: each byte of the atom's colour times the atom's alpha over 256, rounded
/// down
[[nodiscard]] constexpr uint32_t RingArgb(uint32_t argb, uint8_t alpha)
{
	uint32_t result = 0;
	for (uint32_t shift = 0; shift < 32; shift += 8)
	{
		const uint32_t channel = (argb >> shift) & 0xFFu;
		result |= ((channel * alpha) >> 8u) << shift;
	}
	return result;
}

} // namespace openblack::particles::object_light_sheet
