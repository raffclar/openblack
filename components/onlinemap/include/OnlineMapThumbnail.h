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

/// The thumbnail of a land of a network game, as the game keeps it in its "Online Maps" folder: a ".thm" file beside
/// the ".map" file. The game shows it in the Landscape tab of the game room.
///
/// The format, little endian:
/// - the 24 characters "LiOnHeAd server_map_file"
/// - a 32-bit number that the game does not read
/// - a 32-bit count of the citadels, a 32-bit count of the towns and a 32-bit length of the description
/// - the description, as text
/// - the citadels, then the towns: 9 bytes each, a 32-bit x, a 32-bit y and the owner's 8-bit number
/// - a 32-bit length of the picture, then the picture: red, green and blue bytes, a row at a time from the top
namespace openblack::onlinemap
{

/// A citadel or a town on the picture of the land
struct ThumbnailPlace
{
	/// The column of the picture
	int32_t x {0};
	/// The row of the picture
	int32_t y {0};
	/// The number of the player that has the place, -1 for none
	int8_t owner {0};
};

/// The parts of a ".thm" file. The views point into the bytes that the file gave.
struct ThumbnailFile
{
	std::string_view description;
	std::vector<ThumbnailPlace> citadels;
	std::vector<ThumbnailPlace> towns;
	/// Three bytes for each pixel of the picture
	std::span<const std::byte> picture;
};

/// The 24 characters at the start of the file
inline constexpr std::string_view k_ThumbnailMagic = "LiOnHeAd server_map_file";

/// The parts of the bytes of a ".thm" file. Nothing when the file does not start with k_ThumbnailMagic or when its
/// lengths are more than its bytes.
[[nodiscard]] std::optional<ThumbnailFile> ParseThumbnail(std::span<const std::byte> bytes);

} // namespace openblack::onlinemap
