/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SavePictureMaker.h"

#include <cstring>

#include <algorithm>
#include <array>

#include "SavePictures.h"

using namespace openblack::gamesave;

namespace
{
constexpr uint32_t k_Side = k_PictureSide;
constexpr size_t k_RowSize = static_cast<size_t>(k_Side) * k_TexelSize;
constexpr uint32_t k_BlocksAcross = k_Side / k_PictureBlockSide;

/// The words at the start of a file: its size, then the description of a compressed picture of 256 by 256 texels. The
/// game also writes the place in its memory of the picture at word 10. That place has no use in the file, so openblack
/// writes zero.
constexpr std::array<uint32_t, 32> k_FileHeader {
    0x00008080, 0x0000007C, 0x00081007, 0x00000100, 0x00000100, 0x00008000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000020, 0x00000004, 0x31545844, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00001800, 0x00000000, 0x00000000, 0x00000000, 0x00000000};
static_assert(sizeof(k_FileHeader) == 4 + k_PictureDescriptionSize);

/// The weight of the first colour when the game mixes two colours, in parts of 255
constexpr int k_MixWeight = 0x80;
/// The small picture goes into the cell of this small picture
constexpr uint32_t k_SmallPictureCell = 4;

/// The game mixes two colours with integer parts of 255 and rounds down
[[nodiscard]] uint8_t Mix(uint8_t first, uint8_t second)
{
	return static_cast<uint8_t>(((second * (255 - k_MixWeight)) + (first * k_MixWeight)) / 255);
}

/// A colour of five bits made eight bits wide, with its high bits again in the low bits
[[nodiscard]] constexpr uint32_t Widen5(uint32_t value)
{
	return (value << 3u) | (value >> 2u);
}
[[nodiscard]] constexpr uint32_t Widen6(uint32_t value)
{
	return (value << 2u) | (value >> 4u);
}

struct Colour
{
	uint32_t red;
	uint32_t green;
	uint32_t blue;
};

[[nodiscard]] Colour Expand(uint16_t packed)
{
	return {.red = Widen5((packed >> 11u) & 0x1Fu), .green = Widen6((packed >> 5u) & 0x3Fu), .blue = Widen5(packed & 0x1Fu)};
}

/// The four colours of a block, as eight bits each
[[nodiscard]] std::array<Colour, 4> Palette(uint16_t first, uint16_t second)
{
	const auto a = Expand(first);
	const auto b = Expand(second);
	if (first > second)
	{
		return {a, b, Colour {(2 * a.red + b.red) / 3, (2 * a.green + b.green) / 3, (2 * a.blue + b.blue) / 3},
		        Colour {(a.red + 2 * b.red) / 3, (a.green + 2 * b.green) / 3, (a.blue + 2 * b.blue) / 3}};
	}
	return {a, b, Colour {(a.red + b.red) / 2, (a.green + b.green) / 2, (a.blue + b.blue) / 2}, Colour {0, 0, 0}};
}

[[nodiscard]] uint16_t ReadWord(std::span<const uint8_t> bytes, size_t at)
{
	return static_cast<uint16_t>(bytes[at] | (bytes[at + 1] << 8u));
}

/// The blocks of a file that the game can read, or nothing
[[nodiscard]] std::optional<std::vector<uint8_t>> BlocksOf(std::optional<std::span<const uint8_t>> file)
{
	if (!file.has_value())
	{
		return std::nullopt;
	}
	auto picture = ReadPictureFile(*file);
	if (!picture.has_value())
	{
		return std::nullopt;
	}
	return std::move(picture->blocks);
}

/// Copies the blocks of a cell of one picture into a cell of the same size of another picture
void CopyCell(std::span<const uint8_t> from, PictureCell source, std::span<uint8_t> to, PictureCell target)
{
	const uint32_t columns = source.width / k_PictureBlockSide;
	const uint32_t rows = source.height / k_PictureBlockSide;
	for (uint32_t row = 0; row < rows; ++row)
	{
		const auto sourceAt =
		    ((static_cast<size_t>(source.y / k_PictureBlockSide + row) * k_BlocksAcross) + (source.x / k_PictureBlockSide)) *
		    k_PictureBlockSize;
		const auto targetAt =
		    ((static_cast<size_t>(target.y / k_PictureBlockSide + row) * k_BlocksAcross) + (target.x / k_PictureBlockSide)) *
		    k_PictureBlockSize;
		std::memcpy(to.data() + targetAt, from.data() + sourceAt, static_cast<size_t>(columns) * k_PictureBlockSize);
	}
}
} // namespace

std::optional<std::vector<uint8_t>> openblack::gamesave::SampleScreen(const ScreenPixels& screen)
{
	const size_t needed = static_cast<size_t>(screen.pitch) * screen.height;
	if (screen.width == 0 || screen.height == 0 || screen.pitch < screen.width * 4 || screen.bgra.size() < needed)
	{
		return std::nullopt;
	}
	std::vector<uint8_t> picture(static_cast<size_t>(k_ScreenPictureWidth) * k_ScreenPictureHeight * k_TexelSize);
	auto* out = picture.data();
	for (uint32_t row = 0; row < k_ScreenPictureHeight; ++row)
	{
		// The steps across and down are powers of two of the screen's size, so the place of each texel is exact
		const uint32_t screenRow = (row * screen.height) / k_ScreenPictureHeight;
		const uint32_t storedRow = screen.bottomUp ? screen.height - 1 - screenRow : screenRow;
		const auto* line = screen.bgra.data() + (static_cast<size_t>(storedRow) * screen.pitch);
		uint32_t red = line[2];
		uint32_t green = line[1];
		uint32_t blue = line[0];
		for (uint32_t column = 0; column < k_ScreenPictureWidth; ++column)
		{
			const auto* pixel = line + (static_cast<size_t>((column * screen.width) / k_ScreenPictureWidth) * 4);
			*out++ = static_cast<uint8_t>((red + pixel[2]) >> 1u);
			*out++ = static_cast<uint8_t>((green + pixel[1]) >> 1u);
			*out++ = static_cast<uint8_t>((blue + pixel[0]) >> 1u);
			red = pixel[2];
			green = pixel[1];
			blue = pixel[0];
		}
	}
	return picture;
}

void openblack::gamesave::SmoothLargePicture(std::span<uint8_t> texels)
{
	if (texels.size() < k_PictureTexelsSize)
	{
		return;
	}
	for (uint32_t row = 0; row + 1 < k_ScreenPictureHeight; ++row)
	{
		for (uint32_t column = 0; column + 1 < k_Side; ++column)
		{
			auto* texel = texels.data() + (row * k_RowSize) + (static_cast<size_t>(column) * k_TexelSize);
			for (size_t colour = 0; colour < k_TexelSize; ++colour)
			{
				const auto top = Mix(texel[colour], texel[colour + k_TexelSize]);
				const auto bottom = Mix(texel[colour + k_RowSize], texel[colour + k_RowSize + k_TexelSize]);
				texel[colour] = Mix(top, bottom);
			}
		}
	}
}

void openblack::gamesave::PutSmallPicture(std::span<uint8_t> texels, std::span<const uint8_t> largePicture)
{
	PutSmallPictureInCell(texels, largePicture, k_SmallPictureCell);
}

void openblack::gamesave::PutSmallPictureInCell(std::span<uint8_t> texels, std::span<const uint8_t> largePicture,
                                                uint32_t smallCell)
{
	const auto cell = SmallPictureCell(smallCell);
	if (texels.size() < k_PictureTexelsSize || largePicture.size() < k_ScreenPictureHeight * k_RowSize)
	{
		return;
	}
	for (uint32_t row = 0; row < cell.height; ++row)
	{
		const auto* from = largePicture.data() + (static_cast<size_t>(row) * 2 * k_RowSize);
		auto* to =
		    texels.data() + (static_cast<size_t>(cell.y + row) * k_RowSize) + (static_cast<size_t>(cell.x) * k_TexelSize);
		for (uint32_t column = 0; column < cell.width; ++column)
		{
			for (size_t colour = 0; colour < k_TexelSize; ++colour)
			{
				to[colour] = static_cast<uint8_t>((from[colour] + from[colour + k_TexelSize]) >> 1u);
			}
			from += 2 * k_TexelSize;
			to += k_TexelSize;
		}
	}
}

std::vector<uint8_t> openblack::gamesave::EncodePicture(std::span<const uint8_t> texels)
{
	std::vector<uint8_t> blocks(k_PictureDataSize, 0);
	if (texels.size() < k_PictureTexelsSize)
	{
		return blocks;
	}
	for (uint32_t blockRow = 0; blockRow < k_BlocksAcross; ++blockRow)
	{
		for (uint32_t blockColumn = 0; blockColumn < k_BlocksAcross; ++blockColumn)
		{
			// The game keeps five bits of each colour, then compresses the texels into colours of 5, 6 and 5 bits
			std::array<uint16_t, 16> packed {};
			for (uint32_t i = 0; i < 16; ++i)
			{
				const auto* texel =
				    texels.data() + ((blockRow * 4 + i / 4) * k_RowSize) + ((blockColumn * 4 + i % 4) * k_TexelSize);
				const uint32_t red = texel[0] >> 3u;
				const uint32_t green = texel[1] >> 3u;
				const uint32_t blue = texel[2] >> 3u;
				packed[i] = static_cast<uint16_t>((red << 11u) | (((green << 1u) | (green >> 4u)) << 5u) | blue);
			}
			// The most and the least of each colour in the block
			std::array<uint32_t, 2> red {0, 31};
			std::array<uint32_t, 2> green {0, 63};
			std::array<uint32_t, 2> blue {0, 31};
			for (const auto colour : packed)
			{
				red[0] = std::max(red[0], (colour >> 11u) & 0x1Fu);
				red[1] = std::min(red[1], (colour >> 11u) & 0x1Fu);
				green[0] = std::max(green[0], (colour >> 5u) & 0x3Fu);
				green[1] = std::min(green[1], (colour >> 5u) & 0x3Fu);
				blue[0] = std::max(blue[0], colour & 0x1Fu);
				blue[1] = std::min(blue[1], colour & 0x1Fu);
			}
			const auto first = static_cast<uint16_t>((red[0] << 11u) | (green[0] << 5u) | blue[0]);
			const auto second = static_cast<uint16_t>((red[1] << 11u) | (green[1] << 5u) | blue[1]);
			uint32_t indices = 0;
			if (first != second)
			{
				const auto palette = Palette(first, second);
				for (uint32_t i = 0; i < 16; ++i)
				{
					const auto texel = Expand(packed[i]);
					uint32_t best = 0;
					uint32_t bestDistance = UINT32_MAX;
					for (uint32_t entry = 0; entry < palette.size(); ++entry)
					{
						const auto dr = static_cast<int32_t>(texel.red) - static_cast<int32_t>(palette[entry].red);
						const auto dg = static_cast<int32_t>(texel.green) - static_cast<int32_t>(palette[entry].green);
						const auto db = static_cast<int32_t>(texel.blue) - static_cast<int32_t>(palette[entry].blue);
						const auto distance = static_cast<uint32_t>((dr * dr) + (dg * dg) + (db * db));
						if (distance < bestDistance)
						{
							bestDistance = distance;
							best = entry;
						}
					}
					indices |= best << (2u * i);
				}
			}
			auto* block = blocks.data() + ((static_cast<size_t>(blockRow) * k_BlocksAcross) + blockColumn) * k_PictureBlockSize;
			block[0] = static_cast<uint8_t>(first & 0xFFu);
			block[1] = static_cast<uint8_t>(first >> 8u);
			block[2] = static_cast<uint8_t>(second & 0xFFu);
			block[3] = static_cast<uint8_t>(second >> 8u);
			for (uint32_t i = 0; i < 4; ++i)
			{
				block[4 + i] = static_cast<uint8_t>((indices >> (8u * i)) & 0xFFu);
			}
		}
	}
	return blocks;
}

std::vector<uint8_t> openblack::gamesave::DecodePicture(std::span<const uint8_t> blocks)
{
	std::vector<uint8_t> texels(k_PictureTexelsSize, 0);
	if (blocks.size() < k_PictureDataSize)
	{
		return texels;
	}
	for (uint32_t blockRow = 0; blockRow < k_BlocksAcross; ++blockRow)
	{
		for (uint32_t blockColumn = 0; blockColumn < k_BlocksAcross; ++blockColumn)
		{
			const auto at = ((static_cast<size_t>(blockRow) * k_BlocksAcross) + blockColumn) * k_PictureBlockSize;
			const auto palette = Palette(ReadWord(blocks, at), ReadWord(blocks, at + 2));
			const uint32_t indices = blocks[at + 4] | (blocks[at + 5] << 8u) | (blocks[at + 6] << 16u) |
			                         (static_cast<uint32_t>(blocks[at + 7]) << 24u);
			for (uint32_t i = 0; i < 16; ++i)
			{
				const auto& colour = palette[(indices >> (2u * i)) & 3u];
				auto* texel = texels.data() + ((blockRow * 4 + i / 4) * k_RowSize) + ((blockColumn * 4 + i % 4) * k_TexelSize);
				// The game reads the picture back with five bits for each colour
				texel[0] = static_cast<uint8_t>(colour.red & 0xF8u);
				texel[1] = static_cast<uint8_t>(colour.green & 0xF8u);
				texel[2] = static_cast<uint8_t>(colour.blue & 0xF8u);
			}
		}
	}
	return texels;
}

std::vector<uint8_t> openblack::gamesave::DecodeBlocks(std::span<const uint8_t> blocks, uint32_t width, uint32_t height)
{
	const uint32_t across = width / k_PictureBlockSide;
	const uint32_t down = height / k_PictureBlockSide;
	const size_t rowSize = static_cast<size_t>(width) * k_TexelSize;
	if (blocks.size() < static_cast<size_t>(across) * down * k_PictureBlockSize)
	{
		return {};
	}
	std::vector<uint8_t> texels(rowSize * height, 0);
	for (uint32_t blockRow = 0; blockRow < down; ++blockRow)
	{
		for (uint32_t blockColumn = 0; blockColumn < across; ++blockColumn)
		{
			const auto at = ((static_cast<size_t>(blockRow) * across) + blockColumn) * k_PictureBlockSize;
			const auto palette = Palette(ReadWord(blocks, at), ReadWord(blocks, at + 2));
			const uint32_t indices = blocks[at + 4] | (blocks[at + 5] << 8u) | (blocks[at + 6] << 16u) |
			                         (static_cast<uint32_t>(blocks[at + 7]) << 24u);
			for (uint32_t i = 0; i < 16; ++i)
			{
				const auto& colour = palette[(indices >> (2u * i)) & 3u];
				auto* texel = texels.data() + ((blockRow * 4 + i / 4) * rowSize) + ((blockColumn * 4 + i % 4) * k_TexelSize);
				texel[0] = static_cast<uint8_t>(colour.red);
				texel[1] = static_cast<uint8_t>(colour.green);
				texel[2] = static_cast<uint8_t>(colour.blue);
			}
		}
	}
	return texels;
}

std::vector<uint8_t> openblack::gamesave::WritePictureFile(std::span<const uint8_t> blocks)
{
	std::vector<uint8_t> file(k_PictureFileSize, 0);
	std::memcpy(file.data(), k_FileHeader.data(), sizeof(k_FileHeader));
	std::copy_n(blocks.begin(), std::min(blocks.size(), k_PictureDataSize), file.begin() + sizeof(k_FileHeader));
	return file;
}

std::vector<uint8_t> openblack::gamesave::NewPictureBlocks()
{
	return std::vector<uint8_t>(k_PictureDataSize, 0xFF);
}

std::vector<uint8_t> openblack::gamesave::MakeTemporaryPicture(std::optional<std::span<const uint8_t>> oldFile,
                                                               std::span<const uint8_t> screenPicture)
{
	const auto oldBlocks = BlocksOf(oldFile);
	auto texels = oldBlocks.has_value() ? DecodePicture(*oldBlocks) : std::vector<uint8_t>(k_PictureTexelsSize, 0);
	const auto largeSize = std::min(screenPicture.size(), k_ScreenPictureHeight * k_RowSize);
	std::copy_n(screenPicture.begin(), largeSize, texels.begin());
	SmoothLargePicture(texels);
	const auto written = EncodePicture(texels);

	// The game reads the file that it wrote again, and puts the small picture from the large picture before compression
	auto again = DecodePicture(written);
	PutSmallPicture(again, std::span<const uint8_t>(texels).first(k_ScreenPictureHeight * k_RowSize));
	return WritePictureFile(EncodePicture(again));
}

SlotPictureFiles openblack::gamesave::PutSlotPictures(std::optional<std::span<const uint8_t>> temporary,
                                                      std::optional<std::span<const uint8_t>> large,
                                                      std::optional<std::span<const uint8_t>> small, uint32_t slot)
{
	const auto source = BlocksOf(temporary).value_or(NewPictureBlocks());
	auto largeBlocks = BlocksOf(large).value_or(NewPictureBlocks());
	auto smallBlocks = BlocksOf(small).value_or(NewPictureBlocks());
	CopyCell(source, LargePictureCell(0), largeBlocks, LargePictureCell(slot));
	CopyCell(source, SmallPictureCell(k_SmallPictureCell), smallBlocks, SmallPictureCell(slot));
	return {.large = WritePictureFile(largeBlocks), .small = WritePictureFile(smallBlocks)};
}
