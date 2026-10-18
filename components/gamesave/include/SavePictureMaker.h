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
#include <string_view>
#include <vector>

// How the game makes the pictures of the saved games. The game takes a picture of the screen when the player goes into
// the temple and when it saves by itself. It keeps that picture in a temporary file: a large picture in the top half
// and a small picture below it. A save copies the two pictures from the temporary file into the files of the slot.
namespace openblack::gamesave
{

/// The temporary file in the folder of the pictures
inline constexpr std::string_view k_TemporaryPictureFileName = "save_game_picture_temporary.raw";

/// The large picture that the game takes from the screen, in texels
inline constexpr uint32_t k_ScreenPictureWidth = 256;
inline constexpr uint32_t k_ScreenPictureHeight = 128;
/// A texel of a picture is three bytes: red, green and blue
inline constexpr size_t k_TexelSize = 3;
/// The texels of a whole picture of a file, rows from the top
inline constexpr size_t k_PictureTexelsSize = static_cast<size_t>(256) * 256 * k_TexelSize;

/// The pixels of a screen: four bytes each, blue, green, red and a byte that is not used. The rows are pitch bytes
/// apart, the top row first, or the bottom row first when bottomUp.
struct ScreenPixels
{
	uint32_t width {0};
	uint32_t height {0};
	uint32_t pitch {0};
	bool bottomUp {false};
	std::span<const uint8_t> bgra;
};

/// The large picture from the screen: 256 by 128 texels, rows from the top. Each row and column of the picture takes
/// the row and the column of the screen at the same fraction, rounded down. Each texel is the mean of the screen pixel
/// at its place and the pixel of the texel to its left (the first texel of a row uses its own pixel two times). Nothing
/// when the screen has no pixels.
[[nodiscard]] std::optional<std::vector<uint8_t>> SampleScreen(const ScreenPixels& screen);

/// Makes the large picture in the top half of a whole picture smooth. Each texel, but the last row and the last column,
/// mixes with the texel to its right, the texel below it and the texel below and to the right. The mix is half and half
/// in three steps, with 128 and 127 parts of 255 and the result rounded down.
void SmoothLargePicture(std::span<uint8_t> texels);

/// Puts the small picture into a whole picture, in the cell of the fifth small picture (the left of the third row). Each
/// texel of the small picture is the mean of two texels next to each other on an even row of the large picture.
void PutSmallPicture(std::span<uint8_t> texels, std::span<const uint8_t> largePicture);

/// Compresses a whole picture into the blocks of a file. The texels first lose the three low bits of each colour, as
/// the game keeps them with five bits for each colour before it compresses them.
[[nodiscard]] std::vector<uint8_t> EncodePicture(std::span<const uint8_t> texels);
/// The texels of the blocks of a file, with five bits for each colour as the game reads them back
[[nodiscard]] std::vector<uint8_t> DecodePicture(std::span<const uint8_t> blocks);

/// A file of pictures with the compressed blocks
[[nodiscard]] std::vector<uint8_t> WritePictureFile(std::span<const uint8_t> blocks);
/// The blocks of a file that the game makes new: every byte is 0xFF
[[nodiscard]] std::vector<uint8_t> NewPictureBlocks();

/// The temporary file after the game takes a picture of the screen. The game reads the old file (all black when there
/// is none), puts the large picture into its top half, makes it smooth and writes the file. Then it reads the written
/// file again, puts the small picture into it and writes the file again.
[[nodiscard]] std::vector<uint8_t> MakeTemporaryPicture(std::optional<std::span<const uint8_t>> oldFile,
                                                        std::span<const uint8_t> screenPicture);

/// The files of the large and the small pictures that hold the pictures of a slot, after a save into the slot
struct SlotPictureFiles
{
	std::vector<uint8_t> large;
	std::vector<uint8_t> small;
};

/// Copies the blocks of the two pictures of the temporary file into the cells of the slot in the slot's two files. A
/// file that is not there or that the game cannot read comes as a new file, the temporary file too.
[[nodiscard]] SlotPictureFiles PutSlotPictures(std::optional<std::span<const uint8_t>> temporary,
                                               std::optional<std::span<const uint8_t>> large,
                                               std::optional<std::span<const uint8_t>> small, uint32_t slot);

} // namespace openblack::gamesave
