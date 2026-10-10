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
#include <array>
#include <span>

#include "BinkDecoder.h"

/// The colours the game's Bink library gives a decoded picture: YUV 4:2:0 to RGB with its own integer tables.
///
/// - Chroma is not interpolated: each U/V sample covers its 2x2 block of pixels.
/// - Four 16.16 tables, each truncated towards zero, are added to a floored luma, and the sums clamped to 0..255:
///     y' = max(0, (76309 * (Y - 16)) >> 16)        (Y below 16 gives 0; Y above 235 isn't clamped)
///     R = y' + trunc( 104597 * (V - 128) / 65536)
///     G = y' + trunc( -25675 * (U - 128) / 65536) + trunc(-53279 * (V - 128) / 65536)
///     B = y' + trunc( 132202 * (U - 128) / 65536)
///   These are the usual limited-range BT.601 factors except blue's, which is one more than the textbook 132201.
namespace openblack::bink
{

namespace detail
{
/// trunc(k * (i - 128) / 65536), rounding towards zero
[[nodiscard]] constexpr std::array<int16_t, 256> MakeChromaTable(int32_t k) noexcept
{
	std::array<int16_t, 256> table {};
	for (int32_t i = 0; i < 256; ++i)
	{
		const int32_t p = k * (i - 128);
		table[static_cast<size_t>(i)] = static_cast<int16_t>(p < 0 ? -((-p) >> 16) : (p >> 16));
	}
	return table;
}

[[nodiscard]] constexpr std::array<int16_t, 256> MakeLumaTable() noexcept
{
	std::array<int16_t, 256> table {};
	for (int32_t i = 0; i < 256; ++i)
	{
		table[static_cast<size_t>(i)] = static_cast<int16_t>(std::max(0, (76309 * (i - 16)) >> 16));
	}
	return table;
}
} // namespace detail

inline constexpr auto k_LumaTable = detail::MakeLumaTable();
inline constexpr auto k_RedFromV = detail::MakeChromaTable(104597);
inline constexpr auto k_GreenFromU = detail::MakeChromaTable(-25675);
inline constexpr auto k_GreenFromV = detail::MakeChromaTable(-53279);
inline constexpr auto k_BlueFromU = detail::MakeChromaTable(132202);

struct Rgb8
{
	uint8_t r {0};
	uint8_t g {0};
	uint8_t b {0};
};

[[nodiscard]] constexpr uint8_t ClampToByte(int32_t value) noexcept
{
	return static_cast<uint8_t>(std::clamp(value, 0, 255));
}

/// One pixel's colour from its luma and its block's chroma
[[nodiscard]] constexpr Rgb8 ToRgb(uint8_t y, uint8_t u, uint8_t v) noexcept
{
	const int32_t luma = k_LumaTable[y];
	return {
	    .r = ClampToByte(luma + k_RedFromV[v]),
	    .g = ClampToByte(luma + k_GreenFromU[u] + k_GreenFromV[v]),
	    .b = ClampToByte(luma + k_BlueFromU[u]),
	};
}

/// The pixel order of a converted picture
enum class PixelLayout : uint8_t
{
	Rgba8, ///< R, G, B, then 255
	Bgrx8, ///< B, G, R, then 0: what the library writes to a 32-bit surface
};

/// Converts a whole picture to width * height * 4 bytes. Nothing when `out` is too small
void ConvertPicture(const Picture& picture, PixelLayout layout, std::span<uint8_t> out) noexcept;

/// The tables as one 256 x 2 lookup for a shader: row 0 by Y or V (luma, red from V, green from V, 0), row 1 by U
/// (green from U, blue from U, 0, 0), each entry four signed values
[[nodiscard]] std::array<std::array<int16_t, 4>, 512> MakeShaderLookup() noexcept;

} // namespace openblack::bink
