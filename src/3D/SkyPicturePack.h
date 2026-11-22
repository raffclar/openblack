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

#include <array>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "3D/SkyDome.h"

/// An optional pack of larger sky pictures, for the debug menu only. The pack has the nine pictures of the sky's dome
/// as PNG files, in the same order and with the same two halves in each picture as the game's own pictures. Each
/// picture is a square with a side that is a whole multiple of 256 texels. The game does not change when there is no
/// pack.
namespace openblack::sky_picture_pack
{

/// The folder of the pack in the profile folder
inline constexpr std::string_view k_FolderName = "sky_pictures";
/// The side of each picture is a whole multiple of this, the side of the game's own pictures
inline constexpr uint32_t k_SideStep = sky_dome::k_Rows;
/// The largest side of a picture
inline constexpr uint32_t k_LargestSide = 4096;
/// The dome's target puts the three alignments one above the other, thus it is three sides high
inline constexpr uint32_t k_TargetSides = 3;
/// The bytes of a texel: red, green, blue and alpha
inline constexpr size_t k_TexelBytes = 4;

/// The files of the pictures, in the order of sky_dome::PictureFiles, with the same names and a ".png" extension
[[nodiscard]] std::array<std::string, sky_dome::k_Pictures> PictureFiles();

/// The width and the height of a picture, in texels
struct Size
{
	uint32_t width;
	uint32_t height;

	bool operator==(const Size&) const = default;
};

/// Why the pack does not load
enum class Problem : uint8_t
{
	/// A picture is not in the folder
	MissingPicture,
	/// A picture is not a PNG file that the game can read
	UnreadablePicture,
	/// The width and the height of a picture are not equal
	NotSquare,
	/// The side of a picture is not a whole multiple of 256
	NotWholeStep,
	/// The side of a picture is larger than the game or the graphics device can keep
	TooLarge,
	/// A picture has a side that is not the side of the first picture
	SidesDiffer,
};

/// The words that tell a developer about a problem
[[nodiscard]] std::string_view Describe(Problem problem);

/// The result of a check of the sizes of the pictures
struct Checked
{
	/// The side of each picture when all the sizes are good, else 0
	uint32_t side {0};
	std::optional<Problem> problem;
	/// The picture that has the problem
	size_t picture {0};
};

/// Checks the sizes of the pictures. Each picture is a square with a side that is a whole multiple of 256, and all the
/// pictures have the same side. The target of three sides high must also fit in the largest texture of the device.
[[nodiscard]] Checked CheckSizes(std::span<const Size> sizes, uint32_t largestTexture);

/// A picture as texels of 8 bits for each channel, red first. The alpha of each texel is 255.
struct Picture
{
	Size size {0, 0};
	std::vector<uint8_t> texels;
};

/// Reads a PNG file of 8 bits for each channel. Nothing is returned when the bytes are not such a file.
[[nodiscard]] std::optional<Picture> Decode(std::span<const uint8_t> bytes);

/// The pictures of a pack, one layer after the other in the order of PictureFiles
struct Pack
{
	uint32_t side {0};
	std::vector<uint8_t> texels;
};

/// The result of a load of a pack: the pack, or the problem and the picture that has it
struct Loaded
{
	std::optional<Pack> pack;
	std::optional<Problem> problem;
	size_t picture {0};
};

/// Gives the bytes of a file of the pack by its name, or nothing when the file is missing
using Reader = std::function<std::optional<std::vector<uint8_t>>(std::string_view file)>;

/// Reads, decodes and checks all the pictures of a pack
[[nodiscard]] Loaded Load(const Reader& read, uint32_t largestTexture);

} // namespace openblack::sky_picture_pack
