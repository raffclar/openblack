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

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// The pictures of the saved games that the save game room shows on its walls. They are in the folder "Pictures" of the
// folder "Saved Games". Each file has one compressed picture of 256 by 256 texels. A file of large pictures keeps the
// pictures of two slots, one above the other. A file of small pictures keeps the pictures of eight slots, in four rows
// of two.
namespace openblack::gamesave
{

inline constexpr std::string_view k_PicturesFolder = "Pictures";
/// The texels across and down a file's picture
inline constexpr uint32_t k_PictureSide = 256;
/// A file has a word with the size of its description, the description and the compressed picture
inline constexpr size_t k_PictureDescriptionSize = 124;
inline constexpr size_t k_PictureDataSize = 32768;
inline constexpr size_t k_PictureFileSize = 4 + k_PictureDescriptionSize + k_PictureDataSize;
/// The compressed picture has a block of 8 bytes for each square of 4 by 4 texels
inline constexpr uint32_t k_PictureBlockSide = 4;
inline constexpr size_t k_PictureBlockSize = 8;

/// The compressed picture of a file, its blocks in rows from the top
struct PictureFile
{
	std::vector<uint8_t> blocks;
};

/// Reads a file of pictures. Nothing when the file is too short or its picture is not of the compressed kind.
[[nodiscard]] std::optional<PictureFile> ReadPictureFile(std::span<const uint8_t> bytes);

/// The file with the large picture of the slot, and the file with its small picture
[[nodiscard]] std::string LargePictureFileName(uint32_t slot);
[[nodiscard]] std::string SmallPictureFileName(uint32_t slot);

/// The part of a file's picture that is the picture of a slot, in texels from the top left
struct PictureCell
{
	uint32_t x;
	uint32_t y;
	uint32_t width;
	uint32_t height;

	bool operator==(const PictureCell&) const = default;
};
/// A large picture is the top or the bottom half of its file's picture
[[nodiscard]] PictureCell LargePictureCell(uint32_t slot);
/// A small picture is a quarter of the height and half of the width of its file's picture
[[nodiscard]] PictureCell SmallPictureCell(uint32_t slot);

/// The compressed blocks of a cell of a file's picture, in rows from the top: a compressed picture of the cell's size
[[nodiscard]] std::vector<uint8_t> CellBlocks(const PictureFile& file, PictureCell cell);

} // namespace openblack::gamesave
