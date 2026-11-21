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

#include "Graphics/Texture2D.h"

/// The target that keeps the blended dome of the sky. The game keeps the dome in a texture of 5 bits for each channel,
/// and the card filters these levels exactly. A target of 8 bits for each channel cannot keep a level of 31 exactly. Thus
/// the filtered sky was one step off in some pixels. A target of 10 bits for each channel keeps each level near enough
/// that the filtered sky has the same 8-bit value as an exact 5-bit texture.
namespace openblack::graphics::sky_dome_target
{

/// The bits of a channel of a 5-bit picture
inline constexpr uint32_t k_PictureBits = 5;

/// The format of the dome's target. Use 10 bits for each channel when the device can draw into this format, else 8 bits.
[[nodiscard]] constexpr TextureFormat Format(bool tenBitsDraw) noexcept
{
	return tenBitsDraw ? TextureFormat::RGB10A2 : TextureFormat::RGBA8;
}

/// The bits of a colour channel of the dome's target format
[[nodiscard]] constexpr uint32_t ChannelBits(TextureFormat format) noexcept
{
	return format == TextureFormat::RGB10A2 ? 10U : 8U;
}

/// The value that a channel of `bits` keeps for a 5-bit level, of 2 to the power `bits` minus 1. The device rounds to the
/// nearest value.
[[nodiscard]] constexpr uint32_t StoredLevel(uint32_t level, uint32_t bits) noexcept
{
	constexpr uint32_t k_Top = (1U << k_PictureBits) - 1U;
	const uint32_t top = (1U << bits) - 1U;
	return ((level * top) + (k_Top / 2U)) / k_Top;
}

} // namespace openblack::graphics::sky_dome_target
