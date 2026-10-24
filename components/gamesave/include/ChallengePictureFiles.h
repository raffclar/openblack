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

#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// The pictures of the challenge room in the files of the profile. The files are in the folder of the game that the
// player plays now, and each save and each load copies them. Each file has the layout of a file of the save pictures:
// a large picture file keeps the pictures of two challenges, a small picture file the pictures of eight.
namespace openblack::gamesave
{

/// The folder of the game that the player plays now, in the folder of the player
inline constexpr std::string_view k_CurrentGameFolder = "Current";

/// A save copies this number of large picture files and of small picture files, from the first file on
inline constexpr uint32_t k_ChallengeLargeFiles = 11;
inline constexpr uint32_t k_ChallengeSmallFiles = 3;

/// The file with the large picture of the challenge at a place of the room, and the file with its small picture
[[nodiscard]] std::string ChallengeLargePictureFileName(uint32_t place);
[[nodiscard]] std::string ChallengeSmallPictureFileName(uint32_t place);

/// The names of the files that a save and a load copy, in the order of the copy: the large picture files, then the
/// small picture files
[[nodiscard]] std::vector<std::string> ChallengePictureFileNames();

/// The two files of pictures after the game keeps the picture of a challenge
struct ChallengePictureWrite
{
	std::vector<uint8_t> large;
	std::vector<uint8_t> small;
};

/// Keeps the picture of the challenge at a place of the room in its two files, as the game does:
/// 1. It reads the large picture file (all black when it is missing or the game cannot read it).
/// 2. It puts the picture into the top half of the file for an even place, the bottom half for an odd place.
/// 3. It compresses and writes the file.
/// 4. It reads the small picture file in the same way and puts the small picture into the place's cell.
/// 5. It compresses and writes that file.
/// The picture is 256 by 128 texels, RGB, top row first, made smooth as the game makes it. The small picture comes from
/// the picture before the compression.
[[nodiscard]] ChallengePictureWrite PutChallengePicture(std::optional<std::span<const uint8_t>> large,
                                                        std::optional<std::span<const uint8_t>> small,
                                                        std::span<const uint8_t> picture, uint32_t place);

/// The compressed blocks of the picture of a challenge, as its files hold them, rows of blocks from the top. Each is
/// empty when its file is missing or the game cannot read it.
struct ChallengePictureCells
{
	/// 256 by 128 texels
	std::vector<uint8_t> large;
	/// 128 by 64 texels
	std::vector<uint8_t> small;
};

/// The files of the challenge pictures of one game, by their names. The store does not read or write the disk: the
/// owner gives it the files that it read and writes the files that the store changes.
class ChallengePictureStore
{
public:
	/// Puts a file in the store, or removes it from the store
	void Set(std::string_view name, std::optional<std::vector<uint8_t>> bytes);
	/// A file of the store, none when the store does not have it
	[[nodiscard]] std::optional<std::span<const uint8_t>> File(std::string_view name) const;
	/// Removes all the files
	void Clear();

	/// Keeps the picture of the challenge at a place in the store (PutChallengePicture). Returns the names of the large
	/// picture file and the small picture file that changed.
	std::pair<std::string, std::string> Put(uint32_t place, std::span<const uint8_t> picture);
	/// The blocks of the picture of the challenge at a place
	[[nodiscard]] ChallengePictureCells Cells(uint32_t place) const;

private:
	std::map<std::string, std::vector<uint8_t>, std::less<>> _files;
};

} // namespace openblack::gamesave
