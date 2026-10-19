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

#include <array>

/// How the game reads the skins of an L3D model. Most models keep 4 bits for each of alpha, red, green and blue. A model
/// with the flag 0x10000 (only the logo effect's sky sphere, D_SKY.l3d) keeps 5 bits for each of red, green and blue,
/// and no alpha: the game makes its skins opaque. For the device's opaque 16-bit format the game keeps the 5-bit
/// colours as they are, as it takes the 5-bit format when the device has it.
namespace openblack::l3d_skin
{

inline constexpr uint32_t k_OpaqueSkinsFlag = 0x10000U;

[[nodiscard]] constexpr bool HasOpaqueSkins(uint32_t meshFlags) noexcept
{
	return (meshFlags & k_OpaqueSkinsFlag) != 0;
}

/// A texel of an opaque skin with its alpha bit set, for a texture of 5 bits for each colour and 1 bit of alpha
[[nodiscard]] constexpr uint16_t OpaqueTexel(uint16_t texel) noexcept
{
	return static_cast<uint16_t>(texel | 0x8000U);
}

/// A texel of an opaque skin as red, green, blue and alpha bytes, each colour the nearest byte to its 5-bit share
[[nodiscard]] constexpr std::array<uint8_t, 4> OpaqueTexelBytes(uint16_t texel) noexcept
{
	constexpr uint32_t k_Five = 31;
	const auto expand = [](uint32_t value) { return static_cast<uint8_t>(((value * 255U) + (k_Five / 2)) / k_Five); };
	return {expand((texel >> 10U) & k_Five), expand((texel >> 5U) & k_Five), expand(texel & k_Five), 0xFF};
}

} // namespace openblack::l3d_skin
