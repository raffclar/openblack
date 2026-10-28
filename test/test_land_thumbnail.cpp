/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <array>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <OnlineMapThumbnail.h>
#include <gtest/gtest.h>

#include "Gui/LandThumbnail.h"

using namespace openblack;
using namespace openblack::gui::land_thumbnail;

namespace
{
struct Place
{
	int32_t x;
	int32_t y;
	int8_t owner;
};

void PutU32(std::vector<std::byte>& bytes, uint32_t value)
{
	for (size_t i = 0; i < 4; ++i)
	{
		bytes.push_back(static_cast<std::byte>(value >> (i * 8)));
	}
}

/// The bytes of a thumbnail file, made as the game's files are
std::vector<std::byte> ThumbnailBytes(std::string_view description, std::span<const Place> citadels,
                                      std::span<const Place> towns, std::span<const uint8_t> picture)
{
	std::vector<std::byte> bytes;
	for (const auto c : onlinemap::k_ThumbnailMagic)
	{
		bytes.push_back(static_cast<std::byte>(c));
	}
	PutU32(bytes, 2);
	PutU32(bytes, static_cast<uint32_t>(citadels.size()));
	PutU32(bytes, static_cast<uint32_t>(towns.size()));
	PutU32(bytes, static_cast<uint32_t>(description.size()));
	for (const auto c : description)
	{
		bytes.push_back(static_cast<std::byte>(c));
	}
	for (const auto& list : {citadels, towns})
	{
		for (const auto& place : list)
		{
			PutU32(bytes, static_cast<uint32_t>(place.x));
			PutU32(bytes, static_cast<uint32_t>(place.y));
			bytes.push_back(static_cast<std::byte>(place.owner));
		}
	}
	PutU32(bytes, static_cast<uint32_t>(picture.size()));
	for (const auto value : picture)
	{
		bytes.push_back(static_cast<std::byte>(value));
	}
	return bytes;
}

constexpr std::array<uint32_t, 8> k_Colours = {0xFFFF4646u, 0xFF47FF54u, 0xFFE347FFu, 0xFF47F9FFu,
                                               0xFFFFFD47u, 0xFF4777FFu, 0xFFFFA247u, 0xFF000000u};

uint16_t At(const Picture& picture, int32_t x, int32_t y)
{
	return picture.texels.at(static_cast<size_t>((y * k_TextureSide) + x));
}
} // namespace

TEST(LandThumbnail, TheFileGivesItsParts)
{
	const std::array<Place, 1> citadels {{{.x = 1, .y = 2, .owner = 0}}};
	const std::array<Place, 2> towns {{{.x = 3, .y = 4, .owner = 1}, {.x = 5, .y = 6, .owner = -1}}};
	const std::array<uint8_t, 12> picture {};
	const auto bytes = ThumbnailBytes("A land.", citadels, towns, picture);
	const auto file = onlinemap::ParseThumbnail(bytes);
	ASSERT_TRUE(file.has_value());
	EXPECT_EQ(file->description, "A land.");
	ASSERT_EQ(file->citadels.size(), 1u);
	EXPECT_EQ(file->citadels[0].y, 2);
	ASSERT_EQ(file->towns.size(), 2u);
	EXPECT_EQ(file->towns[1].x, 5);
	EXPECT_EQ(file->towns[1].owner, -1);
	EXPECT_EQ(file->picture.size(), 12u);

	// A file with another start, or shorter than its lengths, is not a thumbnail
	auto wrong = bytes;
	wrong[0] = std::byte {'l'};
	EXPECT_FALSE(onlinemap::ParseThumbnail(wrong).has_value());
	EXPECT_FALSE(onlinemap::ParseThumbnail(std::span(bytes).first(bytes.size() - 1)).has_value());
}

TEST(LandThumbnail, PixelsArePackedWithSignedBytes)
{
	// Black is clear
	EXPECT_EQ(TexelOf(0, 0, 0), 0x0000);
	// Channels under 128 keep their top four bits, under an opaque alpha
	EXPECT_EQ(TexelOf(0x70, 0x50, 0x30), 0xF753);
	EXPECT_EQ(TexelOf(0, 0, 1), 0xF000);
	// A red of 128 or more lowers the alpha by one
	EXPECT_EQ(TexelOf(0x90, 0x20, 0x10), 0xE921);
	// A green of 128 or more lowers the red by one, and a blue of 128 or more lowers the green by one
	EXPECT_EQ(TexelOf(0x70, 0x90, 0x10), 0xF691);
	EXPECT_EQ(TexelOf(0x70, 0x50, 0x90), 0xF749);
	// White
	EXPECT_EQ(TexelOf(0xFF, 0xFF, 0xFF), 0xEEEF);
	// The owners' colours are opaque
	EXPECT_EQ(TexelOfColour(0xFFFF4646u), 0xFF44);
	EXPECT_EQ(TexelOfColour(0xFF000000u), 0xF000);
}

TEST(LandThumbnail, ThePictureFillsTheTopLeftAndPlacesHaveRings)
{
	// A picture of 9 by 9 pixels, all a dark grey
	std::vector<uint8_t> pixels(static_cast<size_t>(9 * 9 * 3), 0x10);
	const std::array<Place, 1> citadels {{{.x = 4, .y = 4, .owner = 0}}};
	const std::array<Place, 1> towns {{{.x = 1, .y = 7, .owner = -1}}};
	const auto bytes = ThumbnailBytes("", citadels, towns, pixels);
	const auto file = onlinemap::ParseThumbnail(bytes);
	ASSERT_TRUE(file.has_value());
	const auto picture = Paint(*file, k_Colours);
	EXPECT_EQ(picture.side, 9);
	ASSERT_EQ(picture.texels.size(), static_cast<size_t>(k_TextureSide * k_TextureSide));
	EXPECT_EQ(At(picture, 8, 0), 0xF111);
	// Past the picture the texels are clear
	EXPECT_EQ(At(picture, 9, 0), 0);
	EXPECT_EQ(At(picture, 0, 9), 0);
	// The citadel of player 0: red rings of radius 2 to 0 round its place
	const auto red = TexelOfColour(k_Colours[0]);
	EXPECT_EQ(At(picture, 4, 4), red);
	EXPECT_EQ(At(picture, 4, 6), red);
	EXPECT_EQ(At(picture, 2, 4), red);
	EXPECT_EQ(At(picture, 6, 4), red);
	EXPECT_EQ(At(picture, 4, 7), 0xF111);
	// The town that no player has: black rings of radius 1
	const auto black = TexelOfColour(k_Colours[7]);
	EXPECT_EQ(At(picture, 1, 7), black);
	EXPECT_EQ(At(picture, 1, 8), black);
	EXPECT_EQ(At(picture, 1, 5), 0xF111);
	EXPECT_EQ(UvMax(picture.side).x, 9.0f / 256.0f);
}

TEST(LandThumbnail, TheLogoIsWhiteInItsCell)
{
	std::vector<uint8_t> logo(static_cast<size_t>(k_LogoCellSide * k_LogoCellSide), 0);
	logo[0] = 15;
	logo[1] = 8;
	const auto sheet = LogoSheet(logo);
	ASSERT_EQ(sheet.size(), static_cast<size_t>(k_TextureSide * k_TextureSide));
	// Cell 5 is the second cell of the second row
	const auto first = static_cast<size_t>((64 * k_TextureSide) + 64);
	EXPECT_EQ(sheet[first], 0xFFFF);
	EXPECT_EQ(sheet[first + 1], 0x8FFF);
	EXPECT_EQ(sheet[first + 2], 0x0FFF);
	EXPECT_EQ(sheet[0], 0);
	const auto [uvMin, uvMax] = LogoCellUv();
	EXPECT_EQ(uvMin, glm::vec2(0.25f, 0.25f));
	EXPECT_EQ(uvMax, glm::vec2(0.5f, 0.5f));
	// A quarter of the box's opacity, cut to whole steps
	EXPECT_EQ(LogoAlpha(1.0f), 63.0f / 255.0f);
	EXPECT_EQ(LogoAlpha(0.0f), 0.0f);
}

TEST(LandThumbnail, ThePicturesAreInTheirControls)
{
	// The thumbnail is between the list of the lands and the land's information
	EXPECT_EQ(k_Place.min, glm::ivec2(340, 410));
	EXPECT_EQ(k_Place.max, glm::ivec2(460, 530));
	// The logo is a square in the middle of the clan's information, 10 less than its height
	const gui::DialogRect info {.min = {410, 410}, .max = {660, 530}};
	const auto logo = LogoPlace(info);
	EXPECT_EQ(logo.min, glm::ivec2(480, 415));
	EXPECT_EQ(logo.max, glm::ivec2(590, 525));
}
