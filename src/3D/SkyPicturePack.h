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

/// An optional pack of larger sky pictures, which the sky shows by default. The pack has the nine pictures of the sky's dome
/// as PNG files, in the same order and with the same two halves in each picture as the game's own pictures. Each
/// picture is a square with a side that is a whole multiple of 256 texels. A picture has 8 or 16 bits for each channel.
/// The game does not change when there is no pack.
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
/// The largest side of a pack that has a picture of 16 bits for each channel. Such a pack keeps 8 bytes for each texel.
inline constexpr uint32_t k_LargestWideSide = 2048;
/// The bytes of a texel of 8 bits for each channel: red, green, blue and alpha
inline constexpr size_t k_TexelBytes = 4;
/// The bytes of a texel of a pack of half floats: red, green, blue and alpha, each a 16-bit float
inline constexpr size_t k_HalfTexelBytes = 8;

/// Whether the sky shows the pictures of the pack. The setting is on by default. With no pack that loaded, or with the
/// low resolution dome, the sky quietly shows the game's own pictures.
[[nodiscard]] constexpr bool ShowsPack(bool settingOn, bool packLoaded, bool lowResolution)
{
	return settingOn && packLoaded && !lowResolution;
}

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

/// A picture as texels of red, green, blue and alpha. The alpha of each texel is the highest value.
struct Picture
{
	Size size {0, 0};
	/// The bits of each channel: 8, or 16 for a file of 16 bits for each channel
	uint8_t bits {8};
	/// One byte for each channel, or for 16 bits a uint16_t in the order of the bytes of the machine
	std::vector<uint8_t> texels;
};

/// Reads a PNG file of 8 or 16 bits for each channel. Nothing is returned when the bytes are not such a file.
[[nodiscard]] std::optional<Picture> Decode(std::span<const uint8_t> bytes);

/// The 16-bit float nearest to a float, with ties to even. A value too large for a half float gives infinity.
[[nodiscard]] uint16_t HalfFromFloat(float value);

/// The 16-bit float of a channel of a picture, from 0 to 1. A picture of 8 bits gives the value of the byte over 255,
/// a picture of 16 bits the value over 65535.
[[nodiscard]] uint16_t HalfOf(const Picture& picture, size_t channel);

/// The pictures of a pack, one layer after the other in the order of PictureFiles
struct Pack
{
	uint32_t side {0};
	/// When a picture has 16 bits for each channel, the pack keeps all its texels as 16-bit floats. Thus the dome gets
	/// each step of these pictures. Else the pack keeps 8 bits for each channel.
	bool halfFloats {false};
	/// The texels: k_TexelBytes for each, or k_HalfTexelBytes with half floats
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
