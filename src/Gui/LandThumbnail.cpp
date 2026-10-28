/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LandThumbnail.h"

#include <algorithm>
#include <bit>
#include <optional>

#include <OnlineMapThumbnail.h>

#include "Common/X87.h"

namespace openblack::gui::land_thumbnail
{
namespace
{
constexpr size_t k_TexelCount = static_cast<size_t>(k_TextureSide) * k_TextureSide;
/// A full turn, where each ring of a mark starts
constexpr float k_FullTurn = std::bit_cast<float>(0x40C90FDBu);
/// Four degrees: the step round a ring of a mark
constexpr float k_RingStep = std::bit_cast<float>(0x3D8EFA35u);
/// The owner of a place that no player has
constexpr int8_t k_NoOwner = -1;
constexpr size_t k_NoOwnerColour = 7;

/// Rings of radius 0 to radius round a place, as the game draws them. The texel index wraps into the next row at
/// the right edge, as the game's does. The game writes past the texture for a place at the top or the bottom edge;
/// this code does not, and no land of the game has such a place.
void Mark(Texels& texels, const onlinemap::ThumbnailPlace& place, int32_t radius, uint16_t texel)
{
	const auto x = static_cast<float>(place.x);
	const auto y = static_cast<float>(place.y);
	for (auto ring = radius; ring >= 0; --ring)
	{
		const auto r = static_cast<float>(ring);
		auto angle = k_FullTurn;
		do
		{
			const auto row = static_cast<int32_t>((x87::Mul(x87::Cos(angle), r) + y) + 0.5f);
			const auto column = static_cast<int32_t>((x87::Mul(x87::Sin(angle), r) + x) + 0.5f);
			angle -= k_RingStep;
			const auto index = (row << 8) + column;
			if (index >= 0 && static_cast<size_t>(index) < texels.size())
			{
				texels[static_cast<size_t>(index)] = texel;
			}
		} while (angle > 0.0f);
	}
}

/// The last place of a list at a pixel: a later place hides an earlier one
const onlinemap::ThumbnailPlace* FindPlace(const std::vector<onlinemap::ThumbnailPlace>& places, int32_t x, int32_t y)
{
	const auto found = std::ranges::find_if(places.rbegin(), places.rend(), [x, y](const onlinemap::ThumbnailPlace& place) {
		return place.x == x && place.y == y;
	});
	return found != places.rend() ? &*found : nullptr;
}

std::optional<uint16_t> OwnerTexel(const onlinemap::ThumbnailPlace& place, std::span<const uint32_t, 8> colours)
{
	// The files of the game have only the owners 0 to 7, and none
	const auto index = place.owner == k_NoOwner ? k_NoOwnerColour : static_cast<size_t>(place.owner);
	if (place.owner < k_NoOwner || index >= colours.size())
	{
		return std::nullopt;
	}
	return TexelOfColour(colours[index]);
}
} // namespace

uint16_t TexelOf(uint8_t red, uint8_t green, uint8_t blue) noexcept
{
	if (red == 0 && green == 0 && blue == 0)
	{
		return 0;
	}
	// The game reads each channel as a signed byte, and shifts blue with its sign
	const int32_t signedRed = static_cast<int8_t>(red);
	const int32_t signedGreen = static_cast<int8_t>(green);
	const int32_t signedBlue = static_cast<int8_t>(blue) >> 4;
	return static_cast<uint16_t>((((signedRed & 0xFF0) + 0xF00) << 4) + signedBlue + (signedGreen & 0xFFF0));
}

uint16_t TexelOfColour(uint32_t colour) noexcept
{
	const auto red = (colour >> 16) & 0xF0u;
	const auto green = (colour >> 8) & 0xF0u;
	const auto blue = (colour & 0xFFu) >> 4;
	return static_cast<uint16_t>(0xF000u | (red << 4) | green | blue);
}

Picture Paint(const onlinemap::ThumbnailFile& file, std::span<const uint32_t, 8> colours)
{
	Picture picture {.side = 0, .texels = Texels(k_TexelCount, 0)};
	// The side of the square picture is the float square root of its pixel count, cut to a whole number
	const auto pixels = static_cast<int32_t>(file.picture.size()) / 3;
	const auto side = static_cast<int32_t>(x87::Sqrt(static_cast<double>(pixels)));
	if (side <= 0 || side > k_TextureSide)
	{
		return picture;
	}
	picture.side = side;
	for (int32_t y = 0; y < side; ++y)
	{
		for (int32_t x = 0; x < side; ++x)
		{
			const auto at = static_cast<size_t>((y * side) + x) * 3;
			picture.texels[static_cast<size_t>((y * k_TextureSide) + x)] =
			    TexelOf(static_cast<uint8_t>(file.picture[at]), static_cast<uint8_t>(file.picture[at + 1]),
			            static_cast<uint8_t>(file.picture[at + 2]));
		}
	}
	constexpr int32_t k_TownRadius = 1;
	constexpr int32_t k_CitadelRadius = 2;
	for (int32_t y = 0; y < side; ++y)
	{
		for (int32_t x = 0; x < side; ++x)
		{
			if (const auto* town = FindPlace(file.towns, x, y); town != nullptr)
			{
				if (const auto texel = OwnerTexel(*town, colours); texel.has_value())
				{
					Mark(picture.texels, *town, k_TownRadius, *texel);
				}
			}
			if (const auto* citadel = FindPlace(file.citadels, x, y); citadel != nullptr)
			{
				if (const auto texel = OwnerTexel(*citadel, colours); texel.has_value())
				{
					Mark(picture.texels, *citadel, k_CitadelRadius, *texel);
				}
			}
		}
	}
	return picture;
}

Texels LogoSheet(std::span<const uint8_t> logo)
{
	Texels sheet(k_TexelCount, 0);
	const auto cellX = (k_LogoCell & 3) * k_LogoCellSide;
	const auto cellY = (k_LogoCell / 4) * k_LogoCellSide;
	for (int32_t y = 0; y < k_LogoCellSide; ++y)
	{
		for (int32_t x = 0; x < k_LogoCellSide; ++x)
		{
			const auto at = static_cast<size_t>((y * k_LogoCellSide) + x);
			if (at >= logo.size())
			{
				continue;
			}
			// White, with the logo's opacity as the alpha
			const auto alpha = static_cast<uint16_t>(logo[at] & 0xFu);
			sheet[static_cast<size_t>(((cellY + y) * k_TextureSide) + cellX + x)] =
			    static_cast<uint16_t>((alpha << 12) | 0xFFFu);
		}
	}
	return sheet;
}

float LogoAlpha(float boxAlpha) noexcept
{
	const auto alpha = std::clamp(static_cast<int32_t>(boxAlpha * 255.0f), 0, 255);
	return static_cast<float>(alpha / 4) / 255.0f;
}

} // namespace openblack::gui::land_thumbnail
