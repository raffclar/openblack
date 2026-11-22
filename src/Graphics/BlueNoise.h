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

#include <span>
#include <vector>

/// A square of blue noise that tiles: neighbour texels have values that are far apart, and the noise has no large
/// patterns. The renderer adds it to a colour before the screen cuts the colour to 8 bits. Thus a smooth gradient does
/// not show bands.
namespace openblack::graphics::blue_noise
{

/// The side of the square that the renderer uses
inline constexpr uint32_t k_Side = 64;

/// The rank of each texel of a square of the side, row after row: each value from 0 to side * side - 1 is in the square
/// one time. The ranks come from the void and cluster method, on a square that wraps at its edges. The result is the
/// same at each call.
[[nodiscard]] std::vector<uint16_t> Ranks(uint32_t side);

/// The ranks as bytes for a texture of one channel: rank * 256 / (side * side)
[[nodiscard]] std::vector<uint8_t> Bytes(std::span<const uint16_t> ranks);

} // namespace openblack::graphics::blue_noise
