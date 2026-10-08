/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <vector>

#include <gtest/gtest.h>

#include "Graphics/Argb4444.h"
#include "Resources/Loaders.h"

using openblack::resources::Texture2DLoader;

namespace
{
Texture2DLoader::ColourAlphaDesc Desc(uint16_t size, Texture2DLoader::ColourAlpha packing)
{
	return {.size = size, .packing = packing};
}
} // namespace

TEST(ColourAlphaTexture, InterleavedKeepsEveryByte)
{
	// a 2 x 2 texture: colour bytes 0..11, alpha 100..103
	std::vector<uint8_t> colour(12);
	for (uint8_t i = 0; i < colour.size(); ++i)
	{
		colour[i] = i;
	}
	const std::vector<uint8_t> alpha = {100, 101, 102, 103};
	const auto rgba = Texture2DLoader::PackColourAlpha(colour, alpha, Desc(2, Texture2DLoader::ColourAlpha::Interleaved));
	ASSERT_TRUE(rgba.has_value());
	const std::vector<uint8_t> expected = {0, 1, 2, 100, 3, 4, 5, 101, 6, 7, 8, 102, 9, 10, 11, 103};
	EXPECT_EQ(*rgba, expected);
}

TEST(ColourAlphaTexture, InterleavedNeedsBothFilesWhole)
{
	const std::vector<uint8_t> colour(12, 1);
	EXPECT_FALSE(Texture2DLoader::PackColourAlpha(colour, std::vector<uint8_t>(3, 0),
	                                              Desc(2, Texture2DLoader::ColourAlpha::Interleaved)));
	EXPECT_FALSE(Texture2DLoader::PackColourAlpha(std::vector<uint8_t>(11, 1), std::vector<uint8_t>(4, 0),
	                                              Desc(2, Texture2DLoader::ColourAlpha::Interleaved)));
}

TEST(ColourAlphaTexture, Argb4444IsWhatPackRawGives)
{
	const std::vector<uint8_t> colour(openblack::graphics::argb4444::k_ColourBytes, 0x7F);
	const std::vector<uint8_t> alpha(0x10000, 0xF0);
	const auto rgba = Texture2DLoader::PackColourAlpha(colour, alpha, Desc(256, Texture2DLoader::ColourAlpha::Argb4444));
	ASSERT_TRUE(rgba.has_value());
	EXPECT_EQ(*rgba, openblack::graphics::argb4444::PackRaw(colour, alpha));
	// the alpha file may be missing
	EXPECT_TRUE(Texture2DLoader::PackColourAlpha(colour, {}, Desc(256, Texture2DLoader::ColourAlpha::Argb4444)));
	// a colour file of another size is not this format
	EXPECT_FALSE(Texture2DLoader::PackColourAlpha(std::vector<uint8_t>(100, 0), alpha,
	                                              Desc(256, Texture2DLoader::ColourAlpha::Argb4444)));
}
