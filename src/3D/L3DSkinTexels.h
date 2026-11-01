/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <span>
#include <vector>

#include "3D/L3DSkinFormat.h"
#include "3D/LowResolutionTextures.h"

/// How the texels of an L3D model's skin go into a texture, by the model's flags. Every loader of L3D skins uses these
/// rules, so that each model with opaque skins looks the same in each place.
namespace openblack::l3d_skin
{

/// The layout of a skin's texture
enum class Layout : uint8_t
{
	/// 4 bits for each of alpha, red, green and blue, as the file keeps them
	Argb4444,
	/// 5 bits for each of red, green and blue, with the top bit set as the alpha
	Argb1555,
	/// One byte for each of red, green, blue and alpha, for a device without the 16-bit opaque format
	Rgba8,
};

/// The layout of the skins of a model with these flags, on a device that has the 16-bit format with 1 bit of alpha or
/// not
[[nodiscard]] constexpr Layout LayoutOf(uint32_t meshFlags, bool deviceHas1555) noexcept
{
	if (!HasOpaqueSkins(meshFlags))
	{
		return Layout::Argb4444;
	}
	return deviceHas1555 ? Layout::Argb1555 : Layout::Rgba8;
}

/// The bytes of each texel in a texture of this layout
[[nodiscard]] constexpr uint32_t BytesPerTexel(Layout layout) noexcept
{
	return layout == Layout::Rgba8 ? 4 : 2;
}

/// The texels of a skin in a texture of this layout, as bytes
[[nodiscard]] inline std::vector<uint8_t> TexelsIn(Layout layout, std::span<const uint16_t> texels)
{
	std::vector<uint8_t> bytes(texels.size() * BytesPerTexel(layout));
	switch (layout)
	{
	case Layout::Argb4444:
		std::ranges::copy(std::as_bytes(texels), reinterpret_cast<std::byte*>(bytes.data()));
		break;
	case Layout::Argb1555:
		for (size_t i = 0; i < texels.size(); ++i)
		{
			const auto texel = OpaqueTexel(texels[i]);
			bytes[i * 2] = static_cast<uint8_t>(texel & 0xFFU);
			bytes[(i * 2) + 1] = static_cast<uint8_t>(texel >> 8U);
		}
		break;
	case Layout::Rgba8:
		for (size_t i = 0; i < texels.size(); ++i)
		{
			std::ranges::copy(OpaqueTexelBytes(texels[i]), bytes.begin() + static_cast<std::ptrdiff_t>(i * 4));
		}
		break;
	}
	return bytes;
}

/// Texels of 4-bit channels that are all solid, with the alpha in the top 4 bits, as the masks of solid texels read
/// them for an opaque skin
[[nodiscard]] inline std::vector<uint16_t> SolidAlphaTexels(size_t count)
{
	return std::vector<uint16_t>(count, static_cast<uint16_t>(0xF000U));
}

/// Texels along each side of an L3D model's skin as the game loads it: 256, or 128 at the lowest detail levels
[[nodiscard]] constexpr uint16_t LoadedSide(bool lowResolution) noexcept
{
	return lowResolution ? low_resolution_textures::k_HalfSide : low_resolution_textures::k_FullSide;
}

/// The texels of an L3D model's skin as the game loads them into its texture. At the lowest detail levels the game keeps
/// each skin at half its size along each side: each texel is the mean of a two by two square of the file's texels, each
/// channel rounded down. An opaque skin takes the mean of its 5-bit colours and stays opaque. A skin of another size
/// stays as it is.
[[nodiscard]] inline std::vector<uint16_t> LoadedTexels(std::span<const uint16_t> texels, uint32_t meshFlags,
                                                        bool lowResolution)
{
	if (lowResolution)
	{
		auto half = HasOpaqueSkins(meshFlags) ? low_resolution_textures::HalveByMean555Opaque(texels)
		                                      : low_resolution_textures::HalveByMean4444(texels);
		if (half.has_value())
		{
			return *std::move(half);
		}
	}
	return {texels.begin(), texels.end()};
}

/// Texels along each side of a square skin of count texels
[[nodiscard]] inline uint16_t SideOf(size_t count) noexcept
{
	return count == static_cast<size_t>(low_resolution_textures::k_HalfSide) * low_resolution_textures::k_HalfSide
	           ? low_resolution_textures::k_HalfSide
	           : low_resolution_textures::k_FullSide;
}

} // namespace openblack::l3d_skin
