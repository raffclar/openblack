/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <vector>

#include <Gui/GameFont.h>
#include <gtest/gtest.h>

using openblack::gui::GameFont;

namespace
{
/// The height of the bitmaps of all the game's fonts
constexpr uint16_t k_FontHeight = 80;

/// A clear bitmap of a width at the height of the game's fonts
std::vector<uint8_t> ClearBitmap(uint16_t width)
{
	return std::vector<uint8_t>(static_cast<size_t>(width) * k_FontHeight, 0);
}

uint8_t CountAt(const GameFont::HalfGlyph& half, uint16_t x, uint16_t y)
{
	return half.count.at((static_cast<size_t>(y) * half.width) + x);
}
} // namespace

TEST(GameFontGlyphs, TheAlphaOfAHalfGlyphPixelComesFromHowManyOfItsFourPixelsAreSet)
{
	EXPECT_EQ(GameFont::HalfGlyphAlpha(0), 0x0);
	EXPECT_EQ(GameFont::HalfGlyphAlpha(1), 0x4);
	EXPECT_EQ(GameFont::HalfGlyphAlpha(2), 0x8);
	EXPECT_EQ(GameFont::HalfGlyphAlpha(3), 0xC);
	EXPECT_EQ(GameFont::HalfGlyphAlpha(4), 0xF);
}

TEST(GameFontGlyphs, AnEvenWidthGlyphHalvesTwoPixelsAcrossAndTwoRowsDown)
{
	// Every pixel set where its column plus its row is a multiple of three, so that each square of four differs
	constexpr uint16_t k_Width = 6;
	auto bitmap = ClearBitmap(k_Width);
	for (uint16_t y = 0; y < k_FontHeight; ++y)
	{
		for (uint16_t x = 0; x < k_Width; ++x)
		{
			bitmap[(static_cast<size_t>(y) * k_Width) + x] = (x + y) % 3 == 0 ? 1 : 0;
		}
	}
	const auto half = GameFont::MakeHalfGlyph(bitmap, k_Width);
	ASSERT_EQ(half.width, 3);
	ASSERT_EQ(half.count.size(), 3u * GameFont::HalfGlyph::k_Height);
	for (uint16_t y = 0; y < GameFont::HalfGlyph::k_Height; ++y)
	{
		for (uint16_t x = 0; x < half.width; ++x)
		{
			uint8_t expected = 0;
			for (uint16_t sy = y * 2u; sy < (y * 2u) + 2u; ++sy)
			{
				for (uint16_t sx = x * 2u; sx < (x * 2u) + 2u; ++sx)
				{
					expected += bitmap[(static_cast<size_t>(sy) * k_Width) + sx];
				}
			}
			EXPECT_EQ(CountAt(half, x, y), expected) << "at " << x << ", " << y;
		}
	}
}

TEST(GameFontGlyphs, TheLastColumnOfAnOddWidthGlyphReadsTheStartOfTheNextRows)
{
	// A glyph 3 wide: the second half pixel of the first row reads the third column of the first two rows and the
	// first column of the next two rows
	constexpr uint16_t k_Width = 3;
	auto bitmap = ClearBitmap(k_Width);
	bitmap[(2 * k_Width) + 0] = 1;
	const auto half = GameFont::MakeHalfGlyph(bitmap, k_Width);
	ASSERT_EQ(half.width, 2);
	EXPECT_EQ(CountAt(half, 1, 0), 1);
	// So the pixel is not in the second row, where the bitmap's third and fourth rows would be
	EXPECT_EQ(CountAt(half, 0, 1), 0);
	EXPECT_EQ(CountAt(half, 1, 1), 0);
}

TEST(GameFontGlyphs, EachRowOfAnOddWidthGlyphStartsOnePixelFurtherOn)
{
	// A glyph 3 wide: each row of the half glyph starts seven pixels after the one before, not six
	constexpr uint16_t k_Width = 3;
	constexpr uint16_t k_Row = 5;
	auto bitmap = ClearBitmap(k_Width);
	bitmap[static_cast<size_t>(k_Row) * 7] = 1;
	const auto half = GameFont::MakeHalfGlyph(bitmap, k_Width);
	EXPECT_EQ(CountAt(half, 0, k_Row), 1);
	// The same pixel in the plain halving is in the second column of that row
	EXPECT_EQ(CountAt(half, 1, k_Row), 0);
}

TEST(GameFontGlyphs, AllOfAGlyphPastItsBitmapReadsAsClear)
{
	// A glyph 1 wide is read past its bitmap in its later rows
	constexpr uint16_t k_Width = 1;
	std::vector<uint8_t> bitmap(static_cast<size_t>(k_Width) * k_FontHeight, 1);
	const auto half = GameFont::MakeHalfGlyph(bitmap, k_Width);
	ASSERT_EQ(half.width, 1);
	// Each row starts three pixels on: the first row reads pixels 0, 1, 1 and 2
	EXPECT_EQ(CountAt(half, 0, 0), 4);
	// Row 26 reads pixels 78, 79, 79 and one past the end
	EXPECT_EQ(CountAt(half, 0, 26), 3);
	EXPECT_EQ(CountAt(half, 0, GameFont::HalfGlyph::k_Height - 1), 0);
}

TEST(GameFontGlyphs, TheSmallGlyphAveragesFourHalfGlyphPixelsWithAClearColumnEitherSide)
{
	constexpr uint16_t k_Width = 8;
	std::vector<uint8_t> bitmap(static_cast<size_t>(k_Width) * k_FontHeight, 1);
	// One pixel of the half glyph partly set: three of four
	bitmap[1] = 0;
	const auto half = GameFont::MakeHalfGlyph(bitmap, k_Width);
	const auto small = GameFont::MakeSmallGlyph(half, k_Width);
	ASSERT_EQ(small.width, 4);
	// (12 + 15 + 15 + 15) / 4, rounded down
	EXPECT_EQ(small.At(1, 0), 14);
	EXPECT_EQ(small.At(2, 0), 15);
	EXPECT_EQ(small.At(0, 0), 0);
	EXPECT_EQ(small.At(3, 0), 0);
	EXPECT_EQ(small.At(1, GameFont::SmallGlyph::k_Height - 1), 15);
}
