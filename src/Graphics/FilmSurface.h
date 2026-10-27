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
#include <array>
#include <optional>
#include <span>

/// The 16-bit surface that each film picture goes through before the game draws it.
///
/// The video library copies the picture from 8 bits a channel to 16 bits a pixel. Each channel keeps only its top bits.
/// There is no rounding and no dither. The game then shows the 16-bit texels as a texture of the same format, and the
/// display card reads a level n of m bits as n / (2^m - 1). A display card that writes the result to 8 bits a channel
/// writes the nearest byte.
namespace openblack::graphics::film_surface
{

enum class Format : uint8_t
{
	/// 5 bits each for red (bits 10 to 14), green (5 to 9) and blue (0 to 4). Bit 15 stays clear
	Rgb555,
	/// 5 bits for red (bits 11 to 15), 6 bits for green (5 to 10) and 5 bits for blue (0 to 4)
	Rgb565,
};

/// A texture format that a display card offers to the game
struct OfferedFormat
{
	uint32_t bits {0};
	/// The format keeps red, green and blue
	bool rgb {false};
	/// The format keeps an alpha
	bool alpha {false};
	uint32_t redMask {0};
	uint32_t greenMask {0};
	uint32_t blueMask {0};
};

/// The surface the game uses on a display card that offers `formats`, in the order that the card offers them.
///
/// The game looks at each opaque 16-bit RGB format in that order and keeps the last one. A format with 5 bits for each
/// channel stops the search. The game uses 565 only when the kept format has 6 bits of green. Thus a card that offers a
/// 5-bit format anywhere in its list gives 555.
[[nodiscard]] constexpr Format Choose(std::span<const OfferedFormat> formats) noexcept
{
	uint32_t keptGreen = 0;
	for (const auto& format : formats)
	{
		if (format.alpha || !format.rgb || format.bits != 16)
		{
			continue;
		}
		keptGreen = format.greenMask;
		if (format.redMask == 0x7C00 && format.greenMask == 0x03E0 && format.blueMask == 0x001F)
		{
			break;
		}
	}
	return keptGreen == 0x07E0 ? Format::Rgb565 : Format::Rgb555;
}

/// The surface on the display cards of today. Their drivers offer an opaque 5-bit format, so `Choose` gives 555.
inline constexpr Format k_TodaysCards = Format::Rgb555;

/// One 8-bit colour, red, green and blue
struct Rgb8
{
	uint8_t r {0};
	uint8_t g {0};
	uint8_t b {0};

	[[nodiscard]] constexpr bool operator==(const Rgb8&) const noexcept = default;
};

/// The 16-bit texel of an 8-bit colour: the top bits of each channel
[[nodiscard]] constexpr uint16_t Pack(Rgb8 colour, Format format) noexcept
{
	const auto red = static_cast<uint32_t>(colour.r >> 3u);
	const auto blue = static_cast<uint32_t>(colour.b >> 3u);
	if (format == Format::Rgb555)
	{
		return static_cast<uint16_t>((red << 10u) | (static_cast<uint32_t>(colour.g >> 3u) << 5u) | blue);
	}
	return static_cast<uint16_t>((red << 11u) | (static_cast<uint32_t>(colour.g >> 2u) << 5u) | blue);
}

/// The nearest byte to a level of `bits` bits, as the display card reads the level and writes it to 8 bits
[[nodiscard]] constexpr uint8_t Widen(uint32_t level, uint32_t bits) noexcept
{
	const uint32_t top = (1u << bits) - 1u;
	return static_cast<uint8_t>(((std::min(level, top) * 255u) + (top / 2u)) / top);
}

/// The 8-bit colour that the display card shows for a 16-bit texel
[[nodiscard]] constexpr Rgb8 Read(uint16_t texel, Format format) noexcept
{
	if (format == Format::Rgb555)
	{
		return {.r = Widen((texel >> 10u) & 0x1Fu, 5), .g = Widen((texel >> 5u) & 0x1Fu, 5), .b = Widen(texel & 0x1Fu, 5)};
	}
	return {.r = Widen((texel >> 11u) & 0x1Fu, 5), .g = Widen((texel >> 5u) & 0x3Fu, 6), .b = Widen(texel & 0x1Fu, 5)};
}

/// The colour of a film pixel after it goes through the 16-bit surface
[[nodiscard]] constexpr Rgb8 Quantise(Rgb8 colour, Format format) noexcept
{
	return Read(Pack(colour, format), format);
}

/// The value for the film shader: 0 for no 16-bit surface, 1 for 555 and 2 for 565
[[nodiscard]] constexpr float ShaderMode(std::optional<Format> format) noexcept
{
	if (!format)
	{
		return 0.0f;
	}
	return *format == Format::Rgb555 ? 1.0f : 2.0f;
}

} // namespace openblack::graphics::film_surface
