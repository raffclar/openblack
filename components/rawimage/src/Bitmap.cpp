/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Bitmap.h"

#include <array>
#include <bit>
#include <vector>

using namespace openblack::rawimage;

namespace
{
constexpr size_t k_FileHeader = 14;
constexpr uint32_t k_CoreHeader = 12;
constexpr uint32_t k_InfoHeader = 40;
constexpr int64_t k_LargestSide = 16384;

enum class Compression : uint32_t
{
	None = 0,
	RunLength8 = 1,
	RunLength4 = 2,
	Masks = 3,
};

class Reader
{
public:
	explicit Reader(std::span<const uint8_t> bytes)
	    : _bytes(bytes)
	{
	}
	[[nodiscard]] bool Has(size_t at, size_t count) const { return at <= _bytes.size() && _bytes.size() - at >= count; }
	[[nodiscard]] uint8_t Byte(size_t at) const { return Has(at, 1) ? _bytes[at] : uint8_t {0}; }
	[[nodiscard]] uint32_t Word(size_t at, size_t count) const
	{
		uint32_t value = 0;
		for (size_t i = 0; i < count; ++i)
		{
			value |= static_cast<uint32_t>(Byte(at + i)) << (8 * i);
		}
		return value;
	}

private:
	std::span<const uint8_t> _bytes;
};

/// A channel from the bits of a mask, with its top bit as the top bit of the byte
uint8_t Channel(uint32_t value, uint32_t mask)
{
	if (mask == 0)
	{
		return 0;
	}
	const auto shift = std::countr_zero(mask);
	const auto bits = std::popcount(mask);
	const auto raw = (value & mask) >> shift;
	if (bits >= 8)
	{
		return static_cast<uint8_t>(raw >> (bits - 8));
	}
	// The bits repeat to fill the byte
	uint32_t result = 0;
	for (int filled = 8 - bits; filled > -bits; filled -= bits)
	{
		result |= filled >= 0 ? raw << filled : raw >> -filled;
	}
	return static_cast<uint8_t>(result);
}
} // namespace

std::optional<RgbImage> openblack::rawimage::DecodeBitmap(std::span<const uint8_t> bytes)
{
	const Reader file(bytes);
	if (!file.Has(0, k_FileHeader + k_CoreHeader) || file.Byte(0) != 'B' || file.Byte(1) != 'M')
	{
		return std::nullopt;
	}
	const auto bitsAt = static_cast<size_t>(file.Word(10, 4));
	const auto headerSize = file.Word(k_FileHeader, 4);
	int64_t width = 0;
	int64_t height = 0;
	uint32_t depth = 0;
	auto compression = Compression::None;
	uint32_t coloursUsed = 0;
	size_t paletteAt = k_FileHeader + headerSize;
	size_t paletteEntry = 4;
	std::array<uint32_t, 3> masks {0x7C00, 0x03E0, 0x001F};
	if (headerSize == k_CoreHeader)
	{
		width = static_cast<int16_t>(file.Word(k_FileHeader + 4, 2));
		height = static_cast<int16_t>(file.Word(k_FileHeader + 6, 2));
		depth = file.Word(k_FileHeader + 10, 2);
		paletteEntry = 3;
	}
	else if (headerSize >= k_InfoHeader && file.Has(k_FileHeader, headerSize))
	{
		width = static_cast<int32_t>(file.Word(k_FileHeader + 4, 4));
		height = static_cast<int32_t>(file.Word(k_FileHeader + 8, 4));
		depth = file.Word(k_FileHeader + 14, 2);
		compression = static_cast<Compression>(file.Word(k_FileHeader + 16, 4));
		coloursUsed = file.Word(k_FileHeader + 32, 4);
		if (compression == Compression::Masks)
		{
			// The masks follow the header, or are part of a header that is longer than the first
			const auto masksAt = k_FileHeader + k_InfoHeader;
			masks = {file.Word(masksAt, 4), file.Word(masksAt + 4, 4), file.Word(masksAt + 8, 4)};
			if (headerSize == k_InfoHeader)
			{
				paletteAt += 3 * 4;
			}
		}
		else if (depth == 32)
		{
			masks = {0x00FF0000, 0x0000FF00, 0x000000FF};
		}
	}
	else
	{
		return std::nullopt;
	}
	const bool topDown = height < 0;
	height = topDown ? -height : height;
	if (width <= 0 || height <= 0 || width > k_LargestSide || height > k_LargestSide)
	{
		return std::nullopt;
	}
	const bool runLength = compression == Compression::RunLength8 || compression == Compression::RunLength4;
	const bool masked = compression == Compression::Masks;
	const bool depthOk = (compression == Compression::None &&
	                      (depth == 1 || depth == 4 || depth == 8 || depth == 16 || depth == 24 || depth == 32)) ||
	                     (compression == Compression::RunLength8 && depth == 8) ||
	                     (compression == Compression::RunLength4 && depth == 4) || (masked && (depth == 16 || depth == 32));
	if (!depthOk)
	{
		return std::nullopt;
	}
	// The palette of an image of 8 bits or fewer a texel
	std::vector<std::array<uint8_t, 3>> palette;
	if (depth <= 8)
	{
		const auto most = 1u << depth;
		const auto count = coloursUsed != 0 && coloursUsed < most ? coloursUsed : most;
		for (uint32_t i = 0; i < count; ++i)
		{
			const auto at = paletteAt + (i * paletteEntry);
			palette.push_back({file.Byte(at + 2), file.Byte(at + 1), file.Byte(at)});
		}
	}
	const auto w = static_cast<uint32_t>(width);
	const auto h = static_cast<uint32_t>(height);
	RgbImage image {.width = w, .height = h, .pixels = {}};
	// Texels that a compressed image skips stay black, as Windows leaves them
	image.pixels.assign(static_cast<size_t>(w) * h, std::array<uint8_t, 3> {0, 0, 0});
	const auto fromPalette = [&palette](uint32_t index) {
		return index < palette.size() ? palette[index] : std::array<uint8_t, 3> {0, 0, 0};
	};
	// The file keeps the rows from the bottom up unless its height is negative
	const auto put = [&image, topDown, h, w](uint32_t x, uint32_t row, const std::array<uint8_t, 3>& colour) {
		if (x < w && row < h)
		{
			const auto y = topDown ? row : h - 1 - row;
			image.pixels[(static_cast<size_t>(y) * w) + x] = colour;
		}
	};
	if (runLength)
	{
		const bool four = compression == Compression::RunLength4;
		size_t at = bitsAt;
		uint32_t x = 0;
		uint32_t row = 0;
		while (file.Has(at, 2) && row < h)
		{
			const auto count = file.Byte(at);
			const auto value = file.Byte(at + 1);
			at += 2;
			if (count > 0)
			{
				// A run of one index, or of two indices one after the other
				for (uint32_t i = 0; i < count; ++i, ++x)
				{
					const uint32_t index = four ? ((i % 2 == 0) ? (value >> 4u) : (value & 0x0Fu)) : uint32_t {value};
					put(x, row, fromPalette(index));
				}
				continue;
			}
			if (value == 0)
			{
				x = 0;
				++row;
			}
			else if (value == 1)
			{
				break;
			}
			else if (value == 2)
			{
				x += file.Byte(at);
				row += file.Byte(at + 1);
				at += 2;
			}
			else
			{
				// Indices as they are, padded to a whole number of 16-bit words
				const uint32_t length = four ? (value + 1u) / 2u : value;
				for (uint32_t i = 0; i < value; ++i, ++x)
				{
					const auto byte = file.Byte(at + (four ? i / 2 : i));
					const uint32_t index = four ? ((i % 2 == 0) ? (byte >> 4u) : (byte & 0x0Fu)) : uint32_t {byte};
					put(x, row, fromPalette(index));
				}
				at += length + (length % 2);
			}
		}
		return image;
	}
	const auto stride = ((static_cast<size_t>(w) * depth + 31) / 32) * 4;
	if (!file.Has(bitsAt, stride * h))
	{
		return std::nullopt;
	}
	for (uint32_t row = 0; row < h; ++row)
	{
		const auto line = bitsAt + (row * stride);
		for (uint32_t x = 0; x < w; ++x)
		{
			std::array<uint8_t, 3> colour {};
			switch (depth)
			{
			case 1:
			case 4:
			case 8:
			{
				const auto bit = static_cast<size_t>(x) * depth;
				const auto byte = file.Byte(line + (bit / 8));
				const auto shift = static_cast<uint32_t>(8 - depth - (bit % 8));
				colour = fromPalette((byte >> shift) & ((1u << depth) - 1u));
				break;
			}
			case 16:
			{
				const auto value = file.Word(line + (static_cast<size_t>(x) * 2), 2);
				colour = {Channel(value, masks[0]), Channel(value, masks[1]), Channel(value, masks[2])};
				break;
			}
			case 24:
			{
				const auto at = line + (static_cast<size_t>(x) * 3);
				colour = {file.Byte(at + 2), file.Byte(at + 1), file.Byte(at)};
				break;
			}
			default:
			{
				const auto value = file.Word(line + (static_cast<size_t>(x) * 4), 4);
				colour = {Channel(value, masks[0]), Channel(value, masks[1]), Channel(value, masks[2])};
				break;
			}
			}
			put(x, row, colour);
		}
	}
	return image;
}
