/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SkyPicturePack.h"

#include <cstring>

#include <algorithm>
#include <bit>
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
		return "a picture is not a PNG file of 8 or 16 bits for each channel";
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
	const auto* data = bytes.data();
	const auto length = static_cast<int>(bytes.size());
	int width = 0;
	int height = 0;
	int channels = 0;
	// A picture of 16 bits for each channel keeps all its bits
	const bool wide = stbi_is_16_bit_from_memory(data, length) != 0;
	const std::unique_ptr<void, decltype(&stbi_image_free)> texels(
	    wide ? static_cast<void*>(stbi_load_16_from_memory(data, length, &width, &height, &channels, 4))
	         : static_cast<void*>(stbi_load_from_memory(data, length, &width, &height, &channels, 4)),
	    &stbi_image_free);
	if (texels == nullptr || width <= 0 || height <= 0)
	{
		return std::nullopt;
	}
	Picture picture {.size = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)},
	                 .bits = static_cast<uint8_t>(wide ? 16 : 8),
	                 .texels = {}};
	const auto count = static_cast<size_t>(width) * static_cast<size_t>(height) * k_TexelBytes;
	const auto channelBytes = wide ? sizeof(uint16_t) : sizeof(uint8_t);
	picture.texels.resize(count * channelBytes);
	std::memcpy(picture.texels.data(), texels.get(), picture.texels.size());
	// The dome uses only the colours of the pictures
	for (size_t i = k_TexelBytes - 1; i < count; i += k_TexelBytes)
	{
		std::memset(&picture.texels[i * channelBytes], 0xFF, channelBytes);
	}
	return picture;
}

uint16_t HalfFromFloat(float value)
{
	// The bits of the half float come from the bits of the float, rounded to the nearest, with ties to even
	const auto bits = std::bit_cast<uint32_t>(value);
	const auto sign = static_cast<uint32_t>((bits >> 16) & 0x8000u);
	const int exponent = static_cast<int>((bits >> 23) & 0xFFu) - 127 + 15;
	uint32_t mantissa = bits & 0x7FFFFFu;
	const auto rounded = [](uint32_t kept, uint32_t rest, uint32_t half) {
		return (rest > half || (rest == half && (kept & 1u) != 0)) ? kept + 1 : kept;
	};
	if (exponent >= 31)
	{
		return static_cast<uint16_t>(sign | 0x7C00u);
	}
	if (exponent <= 0)
	{
		// A value under the smallest normal half float keeps fewer bits
		if (exponent < -10)
		{
			return static_cast<uint16_t>(sign);
		}
		mantissa |= 0x800000u;
		const auto shift = static_cast<uint32_t>(14 - exponent);
		const auto rest = mantissa & ((1u << shift) - 1u);
		return static_cast<uint16_t>(sign | rounded(mantissa >> shift, rest, 1u << (shift - 1u)));
	}
	// A carry of the mantissa goes into the exponent, which is correct
	const auto kept = (static_cast<uint32_t>(exponent) << 10) | (mantissa >> 13);
	return static_cast<uint16_t>(sign | rounded(kept, mantissa & 0x1FFFu, 0x1000u));
}

uint16_t HalfOf(const Picture& picture, size_t channel)
{
	if (picture.bits == 16)
	{
		uint16_t value = 0;
		std::memcpy(&value, &picture.texels.at(channel * sizeof(uint16_t)), sizeof(value));
		return HalfFromFloat(static_cast<float>(value) / static_cast<float>(std::numeric_limits<uint16_t>::max()));
	}
	return HalfFromFloat(static_cast<float>(picture.texels.at(channel)) /
	                     static_cast<float>(std::numeric_limits<uint8_t>::max()));
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
	const bool halfFloats = std::ranges::any_of(pictures, [](const Picture& picture) { return picture.bits == 16; });
	if (halfFloats && checked.side > k_LargestWideSide)
	{
		return {.pack = std::nullopt, .problem = Problem::TooLarge, .picture = 0};
	}
	Pack pack {.side = checked.side, .halfFloats = halfFloats, .texels = {}};
	const auto layerTexels = static_cast<size_t>(checked.side) * checked.side;
	pack.texels.reserve(layerTexels * (halfFloats ? k_HalfTexelBytes : k_TexelBytes) * pictures.size());
	for (auto& picture : pictures)
	{
		if (!halfFloats)
		{
			pack.texels.insert(pack.texels.end(), picture.texels.begin(), picture.texels.end());
			continue;
		}
		const auto start = pack.texels.size();
		pack.texels.resize(start + (layerTexels * k_HalfTexelBytes));
		for (size_t channel = 0; channel < layerTexels * k_TexelBytes; ++channel)
		{
			const auto half = HalfOf(picture, channel);
			std::memcpy(&pack.texels[start + (channel * sizeof(half))], &half, sizeof(half));
		}
		// The pack does not use the decoded picture again
		picture.texels = {};
	}
	return {.pack = std::move(pack), .problem = std::nullopt, .picture = 0};
}

} // namespace openblack::sky_picture_pack
