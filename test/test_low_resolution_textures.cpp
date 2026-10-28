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
