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

#include <algorithm>
#include <span>

/// How many bits a channel of a texture keeps. Black & White keeps the textures it loads with an alpha beside them (the
/// sea's among them) in 16 bits a texel, four bits each for red, green, blue and alpha: only the top four bits of each
/// of the files' 8-bit values, without rounding or dithering. The graphics card reads four bits n back as n fifteenths.
namespace openblack::graphics::texel_depth
{

enum class Depth : uint8_t
{
	/// Eight bits a channel, as in the files
	Full,
	/// Four bits a channel, as the game keeps textures with an alpha
	FourBits,
};

/// An 8-bit channel kept in four bits and read back as 8 bits: n fifteenths of 255 is n * 17
[[nodiscard]] constexpr uint8_t FourBits(uint8_t value)
{
	const auto top = static_cast<uint8_t>(value >> 4u);
	return static_cast<uint8_t>((top << 4u) | top);
}

/// Keeps every 8-bit channel of the texels at the depth given
inline void Reduce(std::span<uint8_t> channels, Depth depth)
{
	if (depth == Depth::FourBits)
	{
		std::ranges::transform(channels, channels.begin(), FourBits);
	}
}

} // namespace openblack::graphics::texel_depth
