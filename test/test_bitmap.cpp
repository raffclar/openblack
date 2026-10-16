/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <vector>

#include <Bitmap.h>
#include <gtest/gtest.h>

using namespace openblack::rawimage;

namespace
{
using Colour = std::array<uint8_t, 3>;

/// A bitmap file with a header of 40 bytes, its palette and its texels as given
std::vector<uint8_t> File(int32_t width, int32_t height, uint16_t depth, uint32_t compression,
                          const std::vector<uint8_t>& palette, const std::vector<uint8_t>& texels)
{
	constexpr uint32_t k_Headers = 14 + 40;
	std::vector<uint8_t> bytes(k_Headers, 0);
	const auto word = [&bytes](size_t at, uint32_t value, size_t length) {
		for (size_t i = 0; i < length; ++i)
		{
			bytes.at(at + i) = static_cast<uint8_t>(value >> (8 * i));
		}
	};
	bytes.at(0) = 'B';
	bytes.at(1) = 'M';
	word(10, k_Headers + static_cast<uint32_t>(palette.size()), 4);
	word(14, 40, 4);
	word(18, static_cast<uint32_t>(width), 4);
	word(22, static_cast<uint32_t>(height), 4);
	word(26, 1, 2);
	word(28, depth, 2);
	word(30, compression, 4);
	word(46, static_cast<uint32_t>(palette.size() / 4), 4);
	bytes.insert(bytes.end(), palette.begin(), palette.end());
	bytes.insert(bytes.end(), texels.begin(), texels.end());
	word(2, static_cast<uint32_t>(bytes.size()), 4);
	return bytes;
}

/// A dark brown, then red, green and blue, each as blue, green, red and a spare byte
const std::vector<uint8_t> k_Palette {30, 20, 10, 0, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0, 0, 0};
} // namespace

TEST(Bitmap, TheRowsGoFromTheBottomUp)
{
	// Two rows of two texels, 24 bits a texel, each row padded to four bytes
	const auto image = DecodeBitmap(File(2, 2, 24, 0, {}, {0, 0, 255, 0, 255, 0, 0, 0, 255, 0, 0, 0, 0, 0, 0, 0}));
	ASSERT_TRUE(image.has_value());
	EXPECT_EQ(image->At(0, 1), (Colour {255, 0, 0}));
	EXPECT_EQ(image->At(1, 1), (Colour {0, 255, 0}));
	EXPECT_EQ(image->At(0, 0), (Colour {0, 0, 255}));
}

TEST(Bitmap, ANegativeHeightGoesFromTheTopDown)
{
	const auto image = DecodeBitmap(File(1, -2, 8, 0, k_Palette, {1, 0, 0, 0, 3, 0, 0, 0}));
	ASSERT_TRUE(image.has_value());
	EXPECT_EQ(image->At(0, 0), (Colour {255, 0, 0}));
	EXPECT_EQ(image->At(0, 1), (Colour {0, 0, 255}));
}

TEST(Bitmap, SmallTexelsComeFromThePalette)
{
	// Four bits a texel, the first texel in the top four bits
	const auto four = DecodeBitmap(File(2, 1, 4, 0, k_Palette, {0x21, 0, 0, 0}));
	ASSERT_TRUE(four.has_value());
	EXPECT_EQ(four->At(0, 0), (Colour {0, 255, 0}));
	EXPECT_EQ(four->At(1, 0), (Colour {255, 0, 0}));
	// One bit a texel
	const auto one = DecodeBitmap(File(3, 1, 1, 0, {0, 0, 0, 0, 255, 255, 255, 0}, {0b01000000, 0, 0, 0}));
	ASSERT_TRUE(one.has_value());
	EXPECT_EQ(one->At(0, 0), (Colour {0, 0, 0}));
	EXPECT_EQ(one->At(1, 0), (Colour {255, 255, 255}));
}

TEST(Bitmap, SixteenBitTexelsHaveFiveBitsAChannel)
{
	// Red at its highest, green at 1, blue at 16 of 31
	const uint16_t texel = (31u << 10u) | (1u << 5u) | 16u;
	const auto image =
	    DecodeBitmap(File(1, 1, 16, 0, {}, {static_cast<uint8_t>(texel), static_cast<uint8_t>(texel >> 8u), 0, 0}));
	ASSERT_TRUE(image.has_value());
	EXPECT_EQ(image->At(0, 0), (Colour {255, 8, 132}));
}

TEST(Bitmap, RunsOfEightBitIndices)
{
	// A run of two reds, an absolute part of three texels padded to a word, the end of a line, and the end
	const auto image = DecodeBitmap(File(5, 2, 8, 1, k_Palette, {2, 1, 0, 3, 2, 3, 2, 0, 0, 0, 1, 3, 0, 1}));
	ASSERT_TRUE(image.has_value());
	EXPECT_EQ(image->At(0, 1), (Colour {255, 0, 0}));
	EXPECT_EQ(image->At(2, 1), (Colour {0, 255, 0}));
	EXPECT_EQ(image->At(3, 1), (Colour {0, 0, 255}));
	EXPECT_EQ(image->At(0, 0), (Colour {0, 0, 255}));
	// A texel that the runs skip is black
	EXPECT_EQ(image->At(4, 0), (Colour {0, 0, 0}));
}

TEST(Bitmap, RunsOfFourBitIndicesTakeTurns)
{
	const auto image = DecodeBitmap(File(3, 1, 4, 2, k_Palette, {3, 0x12, 0, 1}));
	ASSERT_TRUE(image.has_value());
	EXPECT_EQ(image->At(0, 0), (Colour {255, 0, 0}));
	EXPECT_EQ(image->At(1, 0), (Colour {0, 255, 0}));
	EXPECT_EQ(image->At(2, 0), (Colour {255, 0, 0}));
}

TEST(Bitmap, ADamagedFileHasNoImage)
{
	EXPECT_FALSE(DecodeBitmap(std::vector<uint8_t>(10, 0)).has_value());
	auto file = File(4, 4, 24, 0, {}, {1, 2, 3});
	EXPECT_FALSE(DecodeBitmap(file).has_value());
	file.at(0) = 'X';
	EXPECT_FALSE(DecodeBitmap(file).has_value());
}
