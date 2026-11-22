/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SkyPicturePack.h"

#include <algorithm>
#include <limits>
#include <memory>

// The decoder of the pictures stays private to this file
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#if defined(_MSC_VER) && !defined(__clang__)
// The compiler tells about unused functions of the decoder only at the end of the file
#pragma warning(disable : 4505)
#pragma warning(push, 0)
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
#include <stb_image.h>
#if defined(_MSC_VER) && !defined(__clang__)
#pragma warning(pop)
#else
#pragma GCC diagnostic pop
#endif

namespace openblack::sky_picture_pack
{

std::array<std::string, sky_dome::k_Pictures> PictureFiles()
{
	auto files = sky_dome::PictureFiles();
	for (auto& file : files)
	{
		file = file.substr(0, file.rfind('.')) + ".png";
	}
	return files;
}

std::string_view Describe(Problem problem)
{
	switch (problem)
	{
	case Problem::MissingPicture:
		return "a picture is missing";
	case Problem::UnreadablePicture:
		return "a picture is not a PNG file of 8 bits for each channel";
	case Problem::NotSquare:
		return "a picture is not square";
	case Problem::NotWholeStep:
		return "the side of a picture is not a whole multiple of 256";
	case Problem::TooLarge:
		return "a picture is too large for the graphics device";
	case Problem::SidesDiffer:
		return "the pictures do not have the same side";
	}
	return "an unknown problem";
}

Checked CheckSizes(std::span<const Size> sizes, uint32_t largestTexture)
{
	const auto largestSide = std::min(k_LargestSide, largestTexture / k_TargetSides);
	for (size_t i = 0; i < sizes.size(); ++i)
	{
		const auto& size = sizes[i];
		std::optional<Problem> problem;
		if (size.width != size.height)
		{
			problem = Problem::NotSquare;
		}
		else if (size.width == 0 || size.width % k_SideStep != 0)
		{
			problem = Problem::NotWholeStep;
		}
		else if (size.width > largestSide)
		{
			problem = Problem::TooLarge;
		}
		else if (size.width != sizes.front().width)
		{
			problem = Problem::SidesDiffer;
		}
		if (problem.has_value())
		{
			return {.side = 0, .problem = problem, .picture = i};
		}
	}
	return {.side = sizes.empty() ? 0 : sizes.front().width, .problem = std::nullopt, .picture = 0};
}

std::optional<Picture> Decode(std::span<const uint8_t> bytes)
{
	if (bytes.empty() || bytes.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
	{
		return std::nullopt;
	}
	int width = 0;
	int height = 0;
	int channels = 0;
	const std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> texels(
	    stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &width, &height, &channels, 4), &stbi_image_free);
	// A picture of 16 bits for each channel would come back cut to 8 bits, thus the pack refuses it
	if (texels == nullptr || width <= 0 || height <= 0 ||
	    stbi_is_16_bit_from_memory(bytes.data(), static_cast<int>(bytes.size())) != 0)
	{
		return std::nullopt;
	}
	Picture picture {.size = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}, .texels = {}};
	const auto count = static_cast<size_t>(width) * static_cast<size_t>(height) * k_TexelBytes;
	picture.texels.assign(texels.get(), texels.get() + count);
	// The dome uses only the colours of the pictures
	for (size_t i = k_TexelBytes - 1; i < count; i += k_TexelBytes)
	{
		picture.texels[i] = 0xFF;
	}
	return picture;
}

Loaded Load(const Reader& read, uint32_t largestTexture)
{
	const auto files = PictureFiles();
	std::vector<Picture> pictures;
	std::vector<Size> sizes;
	pictures.reserve(files.size());
	sizes.reserve(files.size());
	for (size_t i = 0; i < files.size(); ++i)
	{
		const auto bytes = read(files.at(i));
		if (!bytes.has_value())
		{
			return {.pack = std::nullopt, .problem = Problem::MissingPicture, .picture = i};
		}
		auto picture = Decode(*bytes);
		if (!picture.has_value())
		{
			return {.pack = std::nullopt, .problem = Problem::UnreadablePicture, .picture = i};
		}
		sizes.push_back(picture->size);
		pictures.push_back(std::move(*picture));
	}
	const auto checked = CheckSizes(sizes, largestTexture);
	if (checked.problem.has_value())
	{
		return {.pack = std::nullopt, .problem = checked.problem, .picture = checked.picture};
	}
	Pack pack {.side = checked.side, .texels = {}};
	pack.texels.reserve(static_cast<size_t>(checked.side) * checked.side * k_TexelBytes * pictures.size());
	for (const auto& picture : pictures)
	{
		pack.texels.insert(pack.texels.end(), picture.texels.begin(), picture.texels.end());
	}
	return {.pack = std::move(pack), .problem = std::nullopt, .picture = 0};
}

} // namespace openblack::sky_picture_pack
