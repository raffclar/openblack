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
#include <string_view>
#include <vector>

/// The low resolution textures of the lowest detail levels, Minimum and Low. The game then loads most of its pictures
/// at half their size along each side: 128 texels in place of 256. A picture from a file takes every second texel of
/// each second row, and its alpha does the same. A few pictures keep their full size: the atmosphere, the buttons of
/// the front end and the pages that hold the letters of the fonts.
namespace openblack::low_resolution_textures
{

/// Texels along a side of a picture that the game loads at half its size
inline constexpr uint16_t k_FullSide = 256;
inline constexpr uint16_t k_HalfSide = 128;

/// Whether the game loads the picture of this file at half its size at the lowest detail levels. stem is the name of the
/// file with no folder and no extension, in any case. The alpha file of a picture (its name with an "a" after it) goes
/// with its picture.
[[nodiscard]] bool HalvedAtLowDetail(std::string_view stem);

/// A picture of side by side texels, one or three bytes a texel, at half its size: the texel at the start of each two
/// by two square. Nothing for a picture of another size.
[[nodiscard]] std::optional<std::vector<uint8_t>> Halve(std::span<const uint8_t> picture, uint16_t side = k_FullSide);

} // namespace openblack::low_resolution_textures
