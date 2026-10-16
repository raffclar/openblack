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
#include <span>

#include "RawImage.h"

/// Windows bitmap files (.bmp), which players can make with any paint program
namespace openblack::rawimage
{
/// The image in a bitmap file, top row first. The decoder reads the old and the new headers, 1, 4, 8, 16, 24 and 32 bits
/// a texel, colour masks, and run length compression of 4 and 8 bits a texel. Texels that a compressed image skips are
/// black. There is no image when the file is damaged or of a kind the decoder cannot read.
[[nodiscard]] std::optional<RgbImage> DecodeBitmap(std::span<const uint8_t> bytes);
} // namespace openblack::rawimage
