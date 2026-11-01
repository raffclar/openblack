/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LowResolutionTextures.h"

#include <cctype>
#include <cstddef>

#include <algorithm>
#include <array>
#include <string>

namespace openblack::low_resolution_textures
{

namespace
{
/// The four bits of each channel of a texel of 16 bits
constexpr std::array<uint32_t, 4> k_Channels4444 = {0xF000u, 0x0F00u, 0x00F0u, 0x000Fu};
/// The five bits of each colour of a texel of 16 bits with one bit of alpha, and that bit
constexpr std::array<uint32_t, 3> k_Channels555 = {0x7C00u, 0x03E0u, 0x001Fu};
constexpr uint32_t k_OpaqueBit = 0x8000u;

/// The pictures that the game loads from their files at half their size, by the names of their files in lower case.
/// The atmosphere and the buttons of the front end are not in the list: the game keeps them at their full size.
constexpr std::array<std::string_view, 25> k_Halved = {
    "blobs",
    "burn",
    "c_ape_hair",
    "choosesymbol",
    "cool_effect",
    "forcefield",
    "gatheringtext",
    "icons",
    "leash",
    "misc0",
    "mousehelp",
    "p4",
    "p4t",
    "picturetexture",
    "pin",
    "rainbow",
    "s_fire",
    "sky",
    "smallbump",
    "smoke",
    "snow",
    "sun",
    "weather",
    "playerssymbols",
    "originalchoosesymbol",
};
} // namespace

bool HalvedAtLowDetail(std::string_view stem)
{
	std::string lower(stem);
	std::ranges::transform(lower, lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	const auto listed = [](std::string_view name) { return std::ranges::find(k_Halved, name) != k_Halved.end(); };
	// An alpha file goes with the picture whose name it adds an "a" to
	return listed(lower) || (lower.ends_with('a') && listed(std::string_view(lower).substr(0, lower.size() - 1)));
}

std::optional<std::vector<uint8_t>> Halve(std::span<const uint8_t> picture, uint16_t side)
{
	const auto texels = static_cast<size_t>(side) * side;
	if (side < 2 || texels == 0 || (picture.size() != texels && picture.size() != texels * 3))
	{
		return std::nullopt;
	}
	const size_t channels = picture.size() / texels;
	const size_t half = side / 2u;
	std::vector<uint8_t> out;
	out.reserve(half * half * channels);
	for (size_t row = 0; row < half; ++row)
	{
		for (size_t column = 0; column < half; ++column)
		{
			const auto from = picture.subspan((((row * 2) * side) + (column * 2)) * channels, channels);
			out.insert(out.end(), from.begin(), from.end());
		}
	}
	return out;
}

std::optional<std::vector<uint16_t>> HalveTexels(std::span<const uint16_t> picture, uint16_t side)
{
	const auto texels = static_cast<size_t>(side) * side;
	if (side < 2 || picture.size() != texels)
	{
		return std::nullopt;
	}
	const size_t half = side / 2u;
	std::vector<uint16_t> out;
	out.reserve(half * half);
	for (size_t row = 0; row < half; ++row)
	{
		for (size_t column = 0; column < half; ++column)
		{
			out.push_back(picture[((row * 2) * side) + (column * 2)]);
		}
	}
	return out;
}

namespace
{
/// A picture of 16-bit texels at half its size: each texel the mean of a two by two square, each channel of masks
/// summed, shifted down by 2 and kept to its own bits, then the bits of set added
std::optional<std::vector<uint16_t>> HalveByMean(std::span<const uint16_t> picture, uint16_t side,
                                                 std::span<const uint32_t> masks, uint32_t set)
{
	const auto texels = static_cast<size_t>(side) * side;
	if (side < 2 || picture.size() != texels)
	{
		return std::nullopt;
	}
	const size_t half = side / 2u;
	std::vector<uint16_t> out;
	out.reserve(half * half);
	for (size_t row = 0; row < half; ++row)
	{
		for (size_t column = 0; column < half; ++column)
		{
			const size_t at = ((row * 2) * side) + (column * 2);
			const std::array<uint32_t, 4> square {picture[at], picture[at + 1], picture[at + side], picture[at + side + 1]};
			uint32_t mean = set;
			for (const uint32_t mask : masks)
			{
				uint32_t sum = 0;
				for (const auto texel : square)
				{
					sum += texel & mask;
				}
				mean |= (sum >> 2u) & mask;
			}
			out.push_back(static_cast<uint16_t>(mean));
		}
	}
	return out;
}
} // namespace

std::optional<std::vector<uint16_t>> HalveByMean4444(std::span<const uint16_t> picture, uint16_t side)
{
	return HalveByMean(picture, side, k_Channels4444, 0u);
}

std::optional<std::vector<uint16_t>> HalveByMean555Opaque(std::span<const uint16_t> picture, uint16_t side)
{
	return HalveByMean(picture, side, k_Channels555, k_OpaqueBit);
}

} // namespace openblack::low_resolution_textures
