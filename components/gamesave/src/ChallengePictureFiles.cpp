/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ChallengePictureFiles.h"

#include <algorithm>

#include "SavePictureMaker.h"
#include "SavePictures.h"

using namespace openblack::gamesave;

namespace
{
/// The large pictures and the small pictures in a file
constexpr uint32_t k_LargePerFile = 2;
constexpr uint32_t k_SmallPerFile = 8;
constexpr size_t k_RowSize = static_cast<size_t>(k_PictureSide) * k_TexelSize;
constexpr size_t k_LargeSize = static_cast<size_t>(k_ScreenPictureHeight) * k_RowSize;

/// The texels of a file as the game reads them: all black when the file is missing or the game cannot read it
[[nodiscard]] std::vector<uint8_t> TexelsOf(std::optional<std::span<const uint8_t>> file)
{
	const auto picture = file.has_value() ? ReadPictureFile(*file) : std::nullopt;
	return picture.has_value() ? DecodePicture(picture->blocks) : std::vector<uint8_t>(k_PictureTexelsSize, 0);
}

[[nodiscard]] std::vector<uint8_t> CellOf(std::optional<std::span<const uint8_t>> file, PictureCell cell)
{
	const auto picture = file.has_value() ? ReadPictureFile(*file) : std::nullopt;
	return picture.has_value() ? CellBlocks(*picture, cell) : std::vector<uint8_t> {};
}
} // namespace

std::string openblack::gamesave::ChallengeLargePictureFileName(uint32_t place)
{
	return "screenshots_hires_" + std::to_string(place / k_LargePerFile) + "_map_0.raw";
}

std::string openblack::gamesave::ChallengeSmallPictureFileName(uint32_t place)
{
	return "screenshots_lores_" + std::to_string(place / k_SmallPerFile) + "_map_0.raw";
}

std::vector<std::string> openblack::gamesave::ChallengePictureFileNames()
{
	std::vector<std::string> names;
	names.reserve(k_ChallengeLargeFiles + k_ChallengeSmallFiles);
	for (uint32_t file = 0; file < k_ChallengeLargeFiles; ++file)
	{
		names.push_back(ChallengeLargePictureFileName(file * k_LargePerFile));
	}
	for (uint32_t file = 0; file < k_ChallengeSmallFiles; ++file)
	{
		names.push_back(ChallengeSmallPictureFileName(file * k_SmallPerFile));
	}
	return names;
}

ChallengePictureWrite openblack::gamesave::PutChallengePicture(std::optional<std::span<const uint8_t>> large,
                                                               std::optional<std::span<const uint8_t>> small,
                                                               std::span<const uint8_t> picture, uint32_t place)
{
	// The picture fills its half of the large picture file. A short picture leaves the rest of its half as it was.
	auto largeTexels = TexelsOf(large);
	const auto half = LargePictureCell(place);
	std::copy_n(picture.begin(), std::min(picture.size(), k_LargeSize),
	            largeTexels.begin() + static_cast<std::ptrdiff_t>(static_cast<size_t>(half.y) * k_RowSize));
	auto largeFile = WritePictureFile(EncodePicture(largeTexels));

	// The small picture comes from the picture before the compression
	auto smallTexels = TexelsOf(small);
	if (picture.size() >= k_LargeSize)
	{
		PutSmallPictureInCell(smallTexels, picture.first(k_LargeSize), place);
	}
	return {.large = std::move(largeFile), .small = WritePictureFile(EncodePicture(smallTexels))};
}

void ChallengePictureStore::Set(std::string_view name, std::optional<std::vector<uint8_t>> bytes)
{
	if (const auto found = _files.find(name); found != _files.end())
	{
		_files.erase(found);
	}
	if (bytes.has_value())
	{
		_files.emplace(std::string(name), std::move(*bytes));
	}
}

std::optional<std::span<const uint8_t>> ChallengePictureStore::File(std::string_view name) const
{
	const auto found = _files.find(name);
	return found != _files.end() ? std::optional<std::span<const uint8_t>>(found->second) : std::nullopt;
}

void ChallengePictureStore::Clear()
{
	_files.clear();
}

std::pair<std::string, std::string> ChallengePictureStore::Put(uint32_t place, std::span<const uint8_t> picture)
{
	auto largeName = ChallengeLargePictureFileName(place);
	auto smallName = ChallengeSmallPictureFileName(place);
	auto written = PutChallengePicture(File(largeName), File(smallName), picture, place);
	Set(largeName, std::move(written.large));
	Set(smallName, std::move(written.small));
	return {std::move(largeName), std::move(smallName)};
}

ChallengePictureCells ChallengePictureStore::Cells(uint32_t place) const
{
	return {.large = CellOf(File(ChallengeLargePictureFileName(place)), LargePictureCell(place)),
	        .small = CellOf(File(ChallengeSmallPictureFileName(place)), SmallPictureCell(place))};
}
