/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <algorithm>
#include <vector>

#include <gtest/gtest.h>

#include "3D/L3DSkinTexels.h"
#include "3D/LowResolutionTextures.h"

using namespace openblack;

TEST(LowResolutionTextures, ThePicturesTheGameHalvesAndTheirAlphaFiles)
{
	EXPECT_TRUE(low_resolution_textures::HalvedAtLowDetail("misc0"));
	EXPECT_TRUE(low_resolution_textures::HalvedAtLowDetail("misc0a"));
	EXPECT_TRUE(low_resolution_textures::HalvedAtLowDetail("S_Fire"));
	EXPECT_TRUE(low_resolution_textures::HalvedAtLowDetail("Sky"));
	EXPECT_TRUE(low_resolution_textures::HalvedAtLowDetail("C_Ape_Haira"));
	// The game keeps the atmosphere and the front end's buttons at their full size
	EXPECT_FALSE(low_resolution_textures::HalvedAtLowDetail("atmos"));
	EXPECT_FALSE(low_resolution_textures::HalvedAtLowDetail("Front_end_buttons"));
	EXPECT_FALSE(low_resolution_textures::HalvedAtLowDetail("human_shadow"));
}

TEST(LowResolutionTextures, HalvingTakesTheFirstTexelOfEachTwoByTwoSquare)
{
	// Each texel holds its row and its column
	std::vector<uint8_t> picture(static_cast<size_t>(256) * 256 * 3);
	for (size_t row = 0; row < 256; ++row)
	{
		for (size_t column = 0; column < 256; ++column)
		{
			const auto at = ((row * 256) + column) * 3;
			picture[at] = static_cast<uint8_t>(row);
			picture[at + 1] = static_cast<uint8_t>(column);
			picture[at + 2] = 7;
		}
	}
	const auto half = low_resolution_textures::Halve(picture);
	ASSERT_TRUE(half.has_value());
	ASSERT_EQ(half->size(), static_cast<size_t>(128) * 128 * 3);
	const auto at = ((5u * 128u) + 9u) * 3u;
	EXPECT_EQ((*half)[at], 10);
	EXPECT_EQ((*half)[at + 1], 18);
	EXPECT_EQ((*half)[at + 2], 7);

	// An alpha file has a byte a texel
	std::vector<uint8_t> alpha(static_cast<size_t>(256) * 256);
	alpha[(2 * 256) + 4] = 200;
	alpha[(3 * 256) + 5] = 100;
	const auto halfAlpha = low_resolution_textures::Halve(alpha);
	ASSERT_TRUE(halfAlpha.has_value());
	EXPECT_EQ((*halfAlpha)[(1 * 128) + 2], 200);
	EXPECT_EQ(std::count(halfAlpha->begin(), halfAlpha->end(), uint8_t {100}), 0);

	// A picture of another size is not halved
	EXPECT_FALSE(low_resolution_textures::Halve(std::vector<uint8_t>(1000)).has_value());
}

TEST(LowResolutionTextures, APictureMadeInMemoryTakesTheMeanOfEachSquareRoundedDown)
{
	std::vector<uint16_t> picture(static_cast<size_t>(256) * 256, 0xF000);
	// The square at row 2, column 4 of the half picture
	const size_t at = (4 * 256) + 8;
	picture[at] = 0xF123;
	picture[at + 1] = 0xF456;
	picture[at + 256] = 0x7789;
	picture[at + 257] = 0x0ABC;
	const auto half = low_resolution_textures::HalveByMean4444(picture);
	ASSERT_TRUE(half.has_value());
	ASSERT_EQ(half->size(), static_cast<size_t>(128) * 128);
	// Each channel is the mean of its four, rounded down: alpha (15 + 15 + 7 + 0) / 4 = 9, red (1 + 4 + 7 + 10) / 4 = 5,
	// green (2 + 5 + 8 + 11) / 4 = 6, blue (3 + 6 + 9 + 12) / 4 = 7
	EXPECT_EQ((*half)[(2 * 128) + 4], 0x9567);
	EXPECT_EQ((*half)[0], 0xF000);

	// A picture of another size is not halved
	EXPECT_FALSE(low_resolution_textures::HalveByMean4444(std::vector<uint16_t>(1000)).has_value());
}

TEST(LowResolutionTextures, AnOpaquePictureTakesTheMeanOfItsFiveBitColoursAndStaysOpaque)
{
	std::vector<uint16_t> picture(static_cast<size_t>(256) * 256, 0x0000);
	// Red 1, 2, 3, 9 makes 15 / 4 = 3; green 31 four times stays 31; blue 0, 0, 0, 3 makes 0
	picture[0] = static_cast<uint16_t>((1u << 10u) | (31u << 5u));
	picture[1] = static_cast<uint16_t>((2u << 10u) | (31u << 5u));
	picture[256] = static_cast<uint16_t>((3u << 10u) | (31u << 5u));
	picture[257] = static_cast<uint16_t>((9u << 10u) | (31u << 5u) | 3u);
	const auto half = low_resolution_textures::HalveByMean555Opaque(picture);
	ASSERT_TRUE(half.has_value());
	EXPECT_EQ((*half)[0], static_cast<uint16_t>(0x8000u | (3u << 10u) | (31u << 5u)));
	EXPECT_EQ((*half)[1], 0x8000);
}

TEST(LowResolutionTextures, TheSkinsOfAModelLoadAtHalfSizeAtTheLowestDetailLevels)
{
	std::vector<uint16_t> skin(static_cast<size_t>(256) * 256, 0xF000);
	skin[1] = 0xF444;
	// Full size at the other detail levels
	EXPECT_EQ(l3d_skin::LoadedTexels(skin, 0, false), skin);
	EXPECT_EQ(l3d_skin::LoadedSide(false), 256);
	// Half size, each texel the mean of four, at the lowest detail levels
	const auto half = l3d_skin::LoadedTexels(skin, 0, true);
	ASSERT_EQ(half.size(), static_cast<size_t>(128) * 128);
	EXPECT_EQ(half[0], 0xF111);
	EXPECT_EQ(l3d_skin::LoadedSide(true), 128);
	EXPECT_EQ(l3d_skin::SideOf(half.size()), 128);
	// An opaque skin takes the five-bit mean and stays opaque: red (28 + 28 + 28 + 29) / 4 = 28, green (0 + 0 + 0 + 2) / 4
	// = 0, blue (0 + 0 + 0 + 4) / 4 = 1
	EXPECT_EQ(l3d_skin::LoadedTexels(skin, l3d_skin::k_OpaqueSkinsFlag, true)[0], 0xF001);
}
