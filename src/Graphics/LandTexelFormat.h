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

#include <bit>

#include "Graphics/Texture2D.h"

namespace openblack::graphics
{

/// True for a format that every back end can read in a shader as colour data without an extension. The web back ends
/// set the limit: WebGPU with only its core features, and WebGL 2. A 16-bit integer format (RG16) is not in this set.
[[nodiscard]] constexpr bool SampledOnEveryBackEnd(TextureFormat format)
{
	switch (format)
	{
	case TextureFormat::R8:
	case TextureFormat::RG8:
	case TextureFormat::RGBA8:
	case TextureFormat::BGRA8:
	case TextureFormat::R16F:
	case TextureFormat::RG16F:
	case TextureFormat::RGBA16F:
		return true;
	default:
		return false;
	}
}

/// The format of the land textures with two 16-bit values in each texel: the height map of the island and the corners
/// that the renderer draws. Each 16-bit value fills two 8-bit channels, with the low byte first. This is the memory
/// order of a pair of uint16_t values, thus the game gives the same data as for RG16. The shaders join the two bytes.
inline constexpr TextureFormat k_LandTwoValueFormat = TextureFormat::RGBA8;

static_assert(SampledOnEveryBackEnd(k_LandTwoValueFormat), "the web back ends must read the land textures");
static_assert(std::endian::native == std::endian::little, "the land textures keep the low byte of each value first");

/// The value of one 16-bit land value from its two channels as a shader reads them (0 to 1), as the land shaders join
/// them. The tests use it to show that each value comes back exactly.
[[nodiscard]] constexpr float JoinLandTexelBytes(float low, float high)
{
	const auto round = [](float channel) { return static_cast<float>(static_cast<int32_t>((channel * 255.0f) + 0.5f)); };
	return round(low) + (256.0f * round(high));
}

} // namespace openblack::graphics
