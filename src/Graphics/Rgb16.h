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

/// The original's 16-bit opaque surfaces, RGB 555 and RGB 565, CPU only. The sibling of Argb4444.h (the 4444 cut).
///
/// - Which of the two: 565 only when the 16-bit texture format the engine chooses has the green mask 0x7E0, that is
///   when the device has no X1R5G5B5; else 555. On the hardware the game targets it is 555: the canonical path
///   (checked against binkw32.dll by the video oracle).
/// - The video frames: each Bink frame is copied to the player's 16-bit framebuffer with BinkCopyToBuffer flags 10
///   (BINKSURFACE565) or 9 (BINKSURFACE555); the textures are described with the masks 0x7C00 / 0x3E0 / 0x1F (555) or
///   0xF800 / 0x7E0 / 0x1F (565).
/// - The .raw textures without the alpha flag are cut to 16 bits on the same switch: 555 (sun.raw goes through it) or
///   the loader's own 565, which keeps only 5 bits of green (PackRaw565).
/// - D3D7 expands each field back to 8 bits when it samples: bit replication (inferred: done by the driver, not by
///   the game and not measurable with the video oracle, whose PNGs use this same replication; the oracle only checks
///   the 16-bit values: 555 = rgb32 >> 3 and 565 = R >> 3, G >> 2, B >> 3 on every pixel).
namespace openblack::graphics::rgb16
{

/// The 16-bit opaque layout (BinkCopyToBuffer flags 9 / 10)
enum class Format : uint8_t
{
	Rgb555, ///< X1R5G5B5, BINKSURFACE555 (9): the canonical path
	Rgb565, ///< R5G6B5, BINKSURFACE565 (10)
};

/// The top 5 bits of an 8-bit value: v >> 3. The .raw cut takes B >> 3 and R & 0xF8, G & 0xF8 before shifting them
/// into place. None rounds.
[[nodiscard]] constexpr uint8_t Quantize5(uint8_t v) noexcept
{
	return static_cast<uint8_t>(v >> 3);
}

/// The top 6 bits of an 8-bit value: v >> 2, the green of BINKSURFACE565 (the conversion is inside binkw32.dll; checked
/// with the video oracle: frame 0 of the five films with flag 10 = their BINKSURFACE32 frame with G >> 2 on every
/// pixel. The .raw loader's own 565 branch keeps 5 bits instead, PackRaw565)
[[nodiscard]] constexpr uint8_t Quantize6(uint8_t v) noexcept
{
	return static_cast<uint8_t>(v >> 2);
}

/// A 5-bit field back to 8 bits as D3D7 samples it: (n << 3) | (n >> 2), 31 -> 255 (inferred: done by the driver)
[[nodiscard]] constexpr uint8_t Expand5(uint8_t n) noexcept
{
	const auto f = static_cast<uint32_t>(n & 0x1Fu);
	return static_cast<uint8_t>(f << 3 | f >> 2);
}

/// A 6-bit field back to 8 bits: (n << 2) | (n >> 4), 63 -> 255 (inferred: done by the driver)
[[nodiscard]] constexpr uint8_t Expand6(uint8_t n) noexcept
{
	const auto f = static_cast<uint32_t>(n & 0x3Fu);
	return static_cast<uint8_t>(f << 2 | f >> 4);
}

/// What the original samples for an 8-bit value through a 5-bit field: Expand5(Quantize5(v))
[[nodiscard]] constexpr uint8_t Cut5(uint8_t v) noexcept
{
	return Expand5(Quantize5(v));
}

/// The same through a 6-bit field: Expand6(Quantize6(v))
[[nodiscard]] constexpr uint8_t Cut6(uint8_t v) noexcept
{
	return Expand6(Quantize6(v));
}

/// The 555 texel of the .raw cut: ((R & 0xF8) << 7) | ((G & 0xF8) << 2) | (B >> 3); bit 15 is 0. BINKSURFACE555
/// writes the same layout (the masks 0x7C00 / 0x3E0 / 0x1F)
[[nodiscard]] constexpr uint16_t Pack555(uint8_t r, uint8_t g, uint8_t b) noexcept
{
	return static_cast<uint16_t>(Quantize5(r) << 10 | Quantize5(g) << 5 | Quantize5(b));
}

/// The R5G6B5 texel of BINKSURFACE565: (R >> 3) << 11 | (G >> 2) << 5 | B >> 3 (the masks 0xF800 / 0x7E0 / 0x1F; the
/// 6-bit green checked with the video oracle, Quantize6)
[[nodiscard]] constexpr uint16_t Pack565(uint8_t r, uint8_t g, uint8_t b) noexcept
{
	return static_cast<uint16_t>(Quantize5(r) << 11 | Quantize6(g) << 5 | Quantize5(b));
}

/// The R5G6B5 texel of the .raw cut's own 565 branch: ((R & 0xF8) << 8) | ((G & 0xF8) << 3) | (B >> 3): the green is
/// G & 0xF8, so the lowest bit of the 6-bit field is always 0. Only for the .raw textures, never for the video
[[nodiscard]] constexpr uint16_t PackRaw565(uint8_t r, uint8_t g, uint8_t b) noexcept
{
	return static_cast<uint16_t>(Quantize5(r) << 11 | Quantize5(g) << 6 | Quantize5(b));
}

/// A 555 texel to 8-bit R, G, B, A (this order, as RGBA8 in memory), each Expand5-ed; the alpha is 0xFF (X1R5G5B5:
/// the surface is opaque)
[[nodiscard]] constexpr std::array<uint8_t, 4> Unpack555(uint16_t texel) noexcept
{
	return {Expand5(static_cast<uint8_t>((texel >> 10) & 0x1Fu)), Expand5(static_cast<uint8_t>((texel >> 5) & 0x1Fu)),
	        Expand5(static_cast<uint8_t>(texel & 0x1Fu)), 0xFF};
}

/// An R5G6B5 texel to 8-bit R, G, B, A, Expand5 / Expand6 / Expand5; the alpha is 0xFF
[[nodiscard]] constexpr std::array<uint8_t, 4> Unpack565(uint16_t texel) noexcept
{
	return {Expand5(static_cast<uint8_t>((texel >> 11) & 0x1Fu)), Expand6(static_cast<uint8_t>((texel >> 5) & 0x3Fu)),
	        Expand5(static_cast<uint8_t>(texel & 0x1Fu)), 0xFF};
}

/// Pack555 or Pack565 by `format` (BinkCopyToBuffer's flags 9 / 10)
[[nodiscard]] constexpr uint16_t Pack(Format format, uint8_t r, uint8_t g, uint8_t b) noexcept
{
	return format == Format::Rgb565 ? Pack565(r, g, b) : Pack555(r, g, b);
}

/// Unpack555 or Unpack565 by `format`
[[nodiscard]] constexpr std::array<uint8_t, 4> Unpack(Format format, uint16_t texel) noexcept
{
	return format == Format::Rgb565 ? Unpack565(texel) : Unpack555(texel);
}

/// RGBA8 pixels (the alpha ignored) to 16-bit texels, as BinkCopyToBuffer fills the video player's framebuffer
/// (pitch width * 2). min(rgba.size() / 4, texels.size()) pixels
inline void Quantize(Format format, std::span<const uint8_t> rgba, std::span<uint16_t> texels) noexcept
{
	const size_t pixels = std::min(rgba.size() / 4, texels.size());
	for (size_t i = 0; i < pixels; ++i)
	{
		texels[i] = Pack(format, rgba[i * 4 + 0], rgba[i * 4 + 1], rgba[i * 4 + 2]);
	}
}

/// 16-bit texels to the RGBA8 the original samples (Unpack each). min(texels.size(), rgba.size() / 4) pixels
inline void Expand(Format format, std::span<const uint16_t> texels, std::span<uint8_t> rgba) noexcept
{
	const size_t pixels = std::min(texels.size(), rgba.size() / 4);
	for (size_t i = 0; i < pixels; ++i)
	{
		const auto c = Unpack(format, texels[i]);
		rgba[i * 4 + 0] = c[0];
		rgba[i * 4 + 1] = c[1];
		rgba[i * 4 + 2] = c[2];
		rgba[i * 4 + 3] = c[3];
	}
}

} // namespace openblack::graphics::rgb16
