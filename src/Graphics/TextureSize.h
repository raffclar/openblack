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

#include "Texture2D.h"

namespace openblack::graphics
{

/// The size of a texture with no mipmaps, as the graphics library calculates it
struct TextureSize
{
	uint16_t width;
	uint16_t height;
	uint16_t layers;
	uint8_t bitsPerPixel;
	/// The bytes of all the layers. A compressed format keeps full blocks of texels.
	uint32_t storageSize;
};

/// Calculates the size of a texture with no mipmaps without the graphics library, so that the dedicated server can
/// describe a texture. The values are the same as the values of the graphics library.
[[nodiscard]] TextureSize CalculateTextureSize(uint16_t width, uint16_t height, uint16_t layers, TextureFormat format) noexcept;

} // namespace openblack::graphics
