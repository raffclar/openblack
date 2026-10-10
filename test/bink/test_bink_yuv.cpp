/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The game's Bink library colours: the tables, every (Y, U, V) against the library's own output, the chroma shared by
// each 2x2 block and the lookup the shader uses.

#include <cstdint>

#include <array>
#include <vector>

#include <BinkYuv.h>
#include <gtest/gtest.h>

#include "BinkGolden.h"
#include "Crc32.h"

using namespace openblack::bink;
using namespace openblack::bink::test;

TEST(BinkYuv, Tables)
{
	// y' = max(0, 76309 * (Y - 16) >> 16): nothing under 16, and no clamp over 235
	EXPECT_EQ(k_LumaTable[10], 0);
	EXPECT_EQ(k_LumaTable[16], 0);
	EXPECT_EQ(k_LumaTable[22], 6);
	EXPECT_EQ(k_LumaTable[23], 8);
	EXPECT_EQ(k_LumaTable[162], 169);
	EXPECT_EQ(k_LumaTable[235], 254);
	EXPECT_EQ(k_LumaTable[255], 278);
	// The chroma tables round towards zero; blue's factor is one more than the textbook one, which gives -116 at U = 70
	EXPECT_EQ(k_BlueFromU[70], -117);
	EXPECT_EQ(k_BlueFromU[71], -114);
	EXPECT_EQ(k_BlueFromU[182], 108);
	EXPECT_EQ(k_RedFromV[40], -140);
	EXPECT_EQ(k_RedFromV[215], 138);
	EXPECT_EQ(k_GreenFromU[128], 0);
	EXPECT_EQ(k_GreenFromV[127], 0);
	EXPECT_EQ(k_GreenFromV[129], 0);
	EXPECT_EQ(k_GreenFromV[219], -73);
	const auto grey = ToRgb(128, 128, 128);
	EXPECT_EQ(grey.r, 130);
	EXPECT_EQ(grey.g, 130);
	EXPECT_EQ(grey.b, 130);
	// The clamp is on the sum
	const auto dark = ToRgb(12, 128, 124);
	EXPECT_EQ(dark.r, 0);
	EXPECT_EQ(dark.g, 3);
}

TEST(BinkYuv, EveryColourIsTheLibrarys)
{
	Crc32 crc;
	std::array<uint8_t, 256 * 3> row {};
	for (uint32_t v = 0; v < 256; ++v)
	{
		for (uint32_t u = 0; u < 256; ++u)
		{
			for (uint32_t y = 0; y < 256; ++y)
			{
				const auto colour = ToRgb(static_cast<uint8_t>(y), static_cast<uint8_t>(u), static_cast<uint8_t>(v));
				row[y * 3] = colour.r;
				row[y * 3 + 1] = colour.g;
				row[y * 3 + 2] = colour.b;
			}
			crc.Add(row);
		}
	}
	EXPECT_EQ(crc.Value(), golden::k_EveryColour);
}

TEST(BinkYuv, ChromaIsSharedByEachBlock)
{
	// 5 x 3 pixels: chroma 3 x 2, the last column and row each sharing their sample with fewer pixels
	const std::vector<uint8_t> y(5 * 3, 128);
	const std::vector<uint8_t> u = {128, 70, 128, 128, 128, 70};
	const std::vector<uint8_t> v(6, 128);
	const Picture picture {
	    .y = {.pixels = y, .stride = 5, .width = 5, .height = 3},
	    .u = {.pixels = u, .stride = 3, .width = 3, .height = 2},
	    .v = {.pixels = v, .stride = 3, .width = 3, .height = 2},
	};
	std::vector<uint8_t> out(5 * 3 * 4);
	ConvertPicture(picture, PixelLayout::Bgrx8, out);
	for (uint32_t row = 0; row < 3; ++row)
	{
		for (uint32_t x = 0; x < 5; ++x)
		{
			const uint8_t* pixel = &out[(row * 5 + x) * 4];
			const bool blue = (row < 2 && (x == 2 || x == 3)) || (row == 2 && x == 4);
			EXPECT_EQ(pixel[0], blue ? 13 : 130) << x << "," << row; // 130 - 117
			EXPECT_EQ(pixel[2], 130);
			EXPECT_EQ(pixel[3], 0);
		}
	}
	ConvertPicture(picture, PixelLayout::Rgba8, out);
	EXPECT_EQ(out[2], 130);
	EXPECT_EQ(out[3], 0xFF);
	// Too small a buffer is left alone
	std::vector<uint8_t> small(10, 9);
	ConvertPicture(picture, PixelLayout::Rgba8, small);
	EXPECT_EQ(small, std::vector<uint8_t>(10, 9));
}

TEST(BinkYuv, ShaderLookupHoldsTheTables)
{
	const auto lookup = MakeShaderLookup();
	for (size_t i = 0; i < 256; ++i)
	{
		EXPECT_EQ(lookup[i][0], k_LumaTable[i]);
		EXPECT_EQ(lookup[i][1], k_RedFromV[i]);
		EXPECT_EQ(lookup[i][2], k_GreenFromV[i]);
		EXPECT_EQ(lookup[256 + i][0], k_GreenFromU[i]);
		EXPECT_EQ(lookup[256 + i][1], k_BlueFromU[i]);
	}
}
