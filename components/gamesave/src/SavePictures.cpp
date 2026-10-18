/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SavePictures.h"

#include <algorithm>
#include <array>

using namespace openblack::gamesave;

namespace
{
/// The description tells the kind of the compressed picture at this place
constexpr size_t k_KindOffset = 4 + 80;
constexpr std::array<uint8_t, 4> k_Kind {'D', 'X', 'T', '1'};
/// The large and the small pictures in a file
constexpr uint32_t k_LargePerFile = 2;
constexpr uint32_t k_SmallPerFile = 8;
} // namespace

std::optional<PictureFile> openblack::gamesave::ReadPictureFile(std::span<const uint8_t> bytes)
{
	if (bytes.size() < k_PictureFileSize)
	{
		return std::nullopt;
	}
	if (!std::ranges::equal(bytes.subspan(k_KindOffset, k_Kind.size()), k_Kind))
	{
		return std::nullopt;
	}
	const auto data = bytes.subspan(4 + k_PictureDescriptionSize, k_PictureDataSize);
	return PictureFile {.blocks = std::vector<uint8_t>(data.begin(), data.end())};
}

std::string openblack::gamesave::LargePictureFileName(uint32_t slot)
{
	return "save_game_picture_hires_" + std::to_string(slot / k_LargePerFile) + ".raw";
}

std::string openblack::gamesave::SmallPictureFileName(uint32_t slot)
{
	return "save_game_picture_lores_" + std::to_string(slot / k_SmallPerFile) + ".raw";
}

PictureCell openblack::gamesave::LargePictureCell(uint32_t slot)
{
	constexpr uint32_t k_Height = k_PictureSide / 2;
	return {.x = 0, .y = (slot & 1u) * k_Height, .width = k_PictureSide, .height = k_Height};
}

PictureCell openblack::gamesave::SmallPictureCell(uint32_t slot)
{
	constexpr uint32_t k_Width = k_PictureSide / 2;
	constexpr uint32_t k_Height = k_PictureSide / 4;
	return {.x = (slot & 1u) * k_Width, .y = ((slot & 7u) >> 1u) * k_Height, .width = k_Width, .height = k_Height};
}

std::vector<uint8_t> openblack::gamesave::CellBlocks(const PictureFile& file, PictureCell cell)
{
	constexpr uint32_t k_BlocksAcross = k_PictureSide / k_PictureBlockSide;
	const uint32_t firstColumn = cell.x / k_PictureBlockSide;
	const uint32_t firstRow = cell.y / k_PictureBlockSide;
	const uint32_t columns = cell.width / k_PictureBlockSide;
	const uint32_t rows = cell.height / k_PictureBlockSide;
	std::vector<uint8_t> blocks;
	if (firstColumn + columns > k_BlocksAcross || firstRow + rows > k_BlocksAcross ||
	    file.blocks.size() < static_cast<size_t>(k_BlocksAcross) * k_BlocksAcross * k_PictureBlockSize)
	{
		return blocks;
	}
	blocks.reserve(static_cast<size_t>(columns) * rows * k_PictureBlockSize);
	for (uint32_t row = firstRow; row < firstRow + rows; ++row)
	{
		const auto start = ((static_cast<size_t>(row) * k_BlocksAcross) + firstColumn) * k_PictureBlockSize;
		const auto end = start + (static_cast<size_t>(columns) * k_PictureBlockSize);
		blocks.insert(blocks.end(), file.blocks.begin() + static_cast<std::ptrdiff_t>(start),
		              file.blocks.begin() + static_cast<std::ptrdiff_t>(end));
	}
	return blocks;
}
