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

/// How many bits a channel of a texture keeps. Black & White keeps the 256 by 256 texture files it loads in 16 bits a
/// texel. A texture loaded with the alpha file beside it keeps four bits each for red, green, blue and alpha; one loaded
/// without an alpha keeps five bits each for red, green and blue. Either way each channel keeps only the top bits of the
/// file's 8-bit value, without rounding or dithering, and the graphics card reads n back as n fifteenths or n
/// thirty-firsts.
namespace openblack::graphics::texel_depth
{

enum class Depth : uint8_t
{
	/// Eight bits a channel, as in the files
	Full,
	/// Four bits a channel, as the game keeps textures loaded with an alpha
	FourBits,
	/// Five bits each for red, green and blue and no alpha, as the game keeps textures loaded without an alpha
	FiveBits,
};

/// An 8-bit channel kept in four bits and read back as 8 bits: n fifteenths of 255 is n * 17
[[nodiscard]] constexpr uint8_t FourBits(uint8_t value)
{
	const auto top = static_cast<uint8_t>(value >> 4u);
	return static_cast<uint8_t>((top << 4u) | top);
}

/// An opaque texel kept in 16 bits, five bits each for red (bits 10 to 14), green (5 to 9) and blue (0 to 4): the top
/// five bits of each channel. The top bit is set, so a card reading it as having a one-bit alpha reads it as opaque.
[[nodiscard]] constexpr uint16_t Rgb555(uint8_t red, uint8_t green, uint8_t blue)
{
	return static_cast<uint16_t>(0x8000u | ((red >> 3u) << 10u) | ((green >> 3u) << 5u) | (blue >> 3u));
}

/// The texel as the game keeps it on a card offering only 5, 6 and 5 bits for 16-bit colours: red in bits 11 to 15,
/// green in 5 to 10 and blue in 0 to 4. Green keeps only its top five bits as well, so its lowest bit is always clear.
/// Cards offering the five-bit format, as every card does today, get Rgb555 instead.
[[nodiscard]] constexpr uint16_t Rgb565(uint8_t red, uint8_t green, uint8_t blue)
{
	return static_cast<uint16_t>(((red >> 3u) << 11u) | ((green >> 3u) << 6u) | (blue >> 3u));
}

/// Keeps every 8-bit channel of the texels in four bits, or leaves them all at full depth. Five bits a channel needs
/// 16-bit texels: see PackRgb555.
inline void Reduce(std::span<uint8_t> channels, Depth depth)
{
	if (depth == Depth::FourBits)
	{
		std::ranges::transform(channels, channels.begin(), FourBits);
	}
}

/// Opaque 16-bit texels, five bits a channel, from texels of `channels` 8-bit values each: red, green and blue, or one
/// grey value standing for all three
[[nodiscard]] inline std::vector<uint16_t> PackRgb555(std::span<const uint8_t> texels, size_t channels)
{
	std::vector<uint16_t> packed(texels.size() / channels);
	for (size_t i = 0; i < packed.size(); ++i)
	{
		const auto texel = texels.subspan(i * channels, channels);
		packed[i] = channels >= 3 ? Rgb555(texel[0], texel[1], texel[2]) : Rgb555(texel[0], texel[0], texel[0]);
	}
	return packed;
}

/// What a raw texture file of the game is to it
enum class RawFile : uint8_t
{
	/// Colours, 256 texels a side, drawn with the alpha file beside them
	ColoursWithAlpha,
	/// Colours, 256 texels a side, with no alpha file
	Colours,
	/// The alpha file beside a colour file, or a grey picture the game draws as an alpha
	Alpha,
	/// Numbers the game reads itself and never draws as a texture, such as palettes and small light pictures
	Data,
};

/// What a raw file is: colours fill 256 by 256 texels three bytes each, and their alpha is the file named as the colours
/// with an "a" after them
[[nodiscard]] constexpr RawFile Classify(size_t bytes, bool hasAlphaFile, bool isAlpha)
{
	constexpr size_t k_ColourBytes = static_cast<size_t>(256) * 256 * 3;
	if (bytes == k_ColourBytes)
	{
		return hasAlphaFile ? RawFile::ColoursWithAlpha : RawFile::Colours;
	}
	return isAlpha ? RawFile::Alpha : RawFile::Data;
}

/// The depth the game keeps a raw file at once loaded as a texture
[[nodiscard]] constexpr Depth DepthOf(RawFile file)
{
	switch (file)
	{
	case RawFile::ColoursWithAlpha:
	case RawFile::Alpha:
		return Depth::FourBits;
	case RawFile::Colours:
		return Depth::FiveBits;
	case RawFile::Data:
		break;
	}
	return Depth::Full;
}

} // namespace openblack::graphics::texel_depth
