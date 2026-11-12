/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <gtest/gtest.h>

#include "Graphics/AlphaTest.h"

using namespace openblack::graphics::alpha_test;

namespace
{
/// The alpha of a pixel of a glyph: the glyph's level of 15 times the text's alpha of 255, as the shader multiplies them
float GlyphPixelAlpha(uint8_t level, uint8_t textAlpha)
{
	return (static_cast<float>(level * 17) / 255.0f) * (static_cast<float>(textAlpha) / 255.0f);
}
} // namespace

// The font pages let a pixel through when its alpha is 5 of 255 or more
TEST(AlphaTest, TheFontPagesDropAPixelOfLessThanFiveOf255)
{
	EXPECT_EQ(k_FontPageReference, 5);
	EXPECT_TRUE(Shows(5.0f / 255.0f, k_FontPageReference));
	EXPECT_FALSE(Shows(4.0f / 255.0f, k_FontPageReference));
	EXPECT_FALSE(Shows(4.9f / 255.0f, k_FontPageReference));
}

// The faintest level of a glyph (1 of 15) shows from a text alpha of 75, as 17 x 75 / 255 is 5
TEST(AlphaTest, TheFaintestGlyphLevelShowsFromATextAlphaOf75)
{
	EXPECT_TRUE(Shows(GlyphPixelAlpha(1, 75), k_FontPageReference));
	EXPECT_FALSE(Shows(GlyphPixelAlpha(1, 74), k_FontPageReference));
}

// A full glyph pixel shows from a text alpha of 5, and a fading text below that shows nothing
TEST(AlphaTest, AFullGlyphPixelShowsFromATextAlphaOfFive)
{
	EXPECT_TRUE(Shows(GlyphPixelAlpha(15, 5), k_FontPageReference));
	EXPECT_FALSE(Shows(GlyphPixelAlpha(15, 4), k_FontPageReference));
}

// A reference of 0 lets every pixel through, also a pixel with no alpha
TEST(AlphaTest, AReferenceOfZeroIsNoTest)
{
	EXPECT_TRUE(Shows(0.0f, 0));
	EXPECT_EQ(Uniform(0)[0], 0.0f);
	EXPECT_EQ(Uniform(k_FontPageReference)[0], 5.0f);
}
