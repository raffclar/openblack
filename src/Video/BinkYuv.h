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

#include "Graphics/Rgb16.h"

/// The Bink 1.0w library's YUV 4:2:0 -> RGB copy (the conversion lives inside the library, not in the game, which only
/// picks the surface). Recovered from golden frames (32-bit surface dumps of the original) against the decoded planes of
/// the same frames: the model below reproduces every pixel of all 55 golden frames of the five videos (449 685 distinct
/// (Y, U, V) triples, 0 mismatches), and with BinkDecoder every frame of the five videos:
///
/// - chroma: nearest, one U/V sample per 2x2 block (no interpolation);
/// - four separately truncated 16.16 tables added to a floored luma, then clamped to 0..255:
///     y' = max(0, (76309 * (Y - 16)) >> 16)          (76309 = 255/219 in 16.16; Y < 16 -> 0, Y > 235 not clamped)
///     R = clamp(y' + trunc( 104597 * (V - 128) / 65536))
///     G = clamp(y' + trunc(-25675 * (U - 128) / 65536) + trunc(-53279 * (V - 128) / 65536))
///     B = clamp(y' + trunc( 132202 * (U - 128) / 65536))
///   "trunc" rounds toward zero (a table of ints built with a C cast), the luma floors. Rv / Gu / Gv are the classic
///   BT.601 limited-range constants; Bu is NOT the classic 132201 (that one fails U = 70: RAD gives -117, it gives -116).
/// - (approximate) the data pins each constant to an interval (Y 76305..76309, Rv 104579..104605, Bu 132202..132221,
///   Gu 25674..25683, Gv 53248..53302); inside them the tables only differ at chroma values the game's videos never or
///   almost never use: Rv at V = 29 / 227, Bu at U = 13 / 243, Gv at V = 5 / 21 / 37 / 219 / 235 / 251. In the five
///   files only V = 219 occurs (10 chroma samples in all of pre_intro.bik), where the G could be 1 lower.
namespace openblack::video::bink_yuv
{

namespace detail
{
/// trunc(k * c / 65536) toward zero, for c = index - 128
constexpr std::array<int16_t, 256> MakeChromaTable(int32_t k) noexcept
{
	std::array<int16_t, 256> t {};
	for (int32_t i = 0; i < 256; ++i)
	{
		const int32_t p = k * (i - 128);
		t[static_cast<size_t>(i)] = static_cast<int16_t>(p < 0 ? -((-p) >> 16) : (p >> 16));
	}
	return t;
}

constexpr std::array<int16_t, 256> MakeLumaTable() noexcept
{
	std::array<int16_t, 256> t {};
	for (int32_t i = 0; i < 256; ++i)
	{
		t[static_cast<size_t>(i)] = static_cast<int16_t>(std::max(0, (76309 * (i - 16)) >> 16));
	}
	return t;
}
} // namespace detail

inline constexpr auto k_Luma = detail::MakeLumaTable();
inline constexpr auto k_RedFromV = detail::MakeChromaTable(104597);
inline constexpr auto k_GreenFromU = detail::MakeChromaTable(-25675);
/// (approximate) the one corner the game's videos reach: V = 219 (10 chroma samples in all of pre_intro.bik, none in
/// the golden frames) is -73 with 53248..53293 (53279 here) and -74 with 53294..53302, both inside the interval the
/// data allows
inline constexpr auto k_GreenFromV = detail::MakeChromaTable(-53279);
inline constexpr auto k_BlueFromU = detail::MakeChromaTable(132202);

[[nodiscard]] constexpr uint8_t Clamp8(int32_t v) noexcept
{
	return static_cast<uint8_t>(std::clamp(v, 0, 255));
}

struct Rgb8
{
	uint8_t r;
	uint8_t g;
	uint8_t b;
};

[[nodiscard]] constexpr Rgb8 ToRgb(uint8_t y, uint8_t u, uint8_t v) noexcept
{
	const int32_t l = k_Luma[y];
	return {Clamp8(l + k_RedFromV[v]), Clamp8(l + k_GreenFromU[u] + k_GreenFromV[v]), Clamp8(l + k_BlueFromU[u])};
}

/// The planes of one decoded frame, YUV 4:2:0 (an alpha plane would be ignored: the game's videos have none)
struct Planes
{
	const uint8_t* y;
	const uint8_t* u;
	const uint8_t* v;
	ptrdiff_t yPitch;
	ptrdiff_t uPitch;
	ptrdiff_t vPitch;
	uint32_t width;
	uint32_t height;
};

/// The library's colours at 32-bit surface precision into width * height RGBA8 (alpha 0xFF): what
/// IVideoDecoder::DecodeNext gives the player, which cuts it to 16 bits itself (graphics::rgb16::Quantize, v >> 3 per
/// channel for 555: the golden .bin are exactly rgb32 >> 3)
inline void CopyToRgba8(const Planes& p, std::span<uint8_t> rgba) noexcept
{
	if (rgba.size() < static_cast<size_t>(p.width) * p.height * 4)
	{
		return;
	}
	for (uint32_t row = 0; row < p.height; ++row)
	{
		const uint8_t* ys = p.y + static_cast<ptrdiff_t>(row) * p.yPitch;
		const uint8_t* us = p.u + static_cast<ptrdiff_t>(row / 2) * p.uPitch;
		const uint8_t* vs = p.v + static_cast<ptrdiff_t>(row / 2) * p.vPitch;
		uint8_t* out = rgba.data() + static_cast<size_t>(row) * p.width * 4;
		for (uint32_t x = 0; x < p.width; ++x)
		{
			const auto c = ToRgb(ys[x], us[x / 2], vs[x / 2]);
			out[x * 4 + 0] = c.r;
			out[x * 4 + 1] = c.g;
			out[x * 4 + 2] = c.b;
			out[x * 4 + 3] = 0xFF;
		}
	}
}

/// CopyToRgba8, then the player's two passes through the 16-bit framebuffer, in one: each pixel's colour packed to a
/// 16-bit texel of `format` (graphics::rgb16::Quantize) and that texel expanded again to RGBA8 (graphics::rgb16::Expand),
/// what the renderer samples. The same values as the three passes; nothing when a buffer is too small
inline void CopyToRgb16AndRgba8(const Planes& p, graphics::rgb16::Format format, std::span<uint16_t> texels,
                                std::span<uint8_t> rgba) noexcept
{
	const size_t pixels = static_cast<size_t>(p.width) * p.height;
	if (texels.size() < pixels || rgba.size() < pixels * 4)
	{
		return;
	}
	for (uint32_t row = 0; row < p.height; ++row)
	{
		const uint8_t* ys = p.y + static_cast<ptrdiff_t>(row) * p.yPitch;
		const uint8_t* us = p.u + static_cast<ptrdiff_t>(row / 2) * p.uPitch;
		const uint8_t* vs = p.v + static_cast<ptrdiff_t>(row / 2) * p.vPitch;
		uint16_t* texel = texels.data() + static_cast<size_t>(row) * p.width;
		uint8_t* out = rgba.data() + static_cast<size_t>(row) * p.width * 4;
		for (uint32_t x = 0; x < p.width; ++x)
		{
			const auto c = ToRgb(ys[x], us[x / 2], vs[x / 2]);
			texel[x] = graphics::rgb16::Pack(format, c.r, c.g, c.b);
			const auto sampled = graphics::rgb16::Unpack(format, texel[x]);
			out[x * 4 + 0] = sampled[0];
			out[x * 4 + 1] = sampled[1];
			out[x * 4 + 2] = sampled[2];
			out[x * 4 + 3] = sampled[3];
		}
	}
}

} // namespace openblack::video::bink_yuv
