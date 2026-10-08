/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The game's headerless .raw images: three bytes a pixel or one, the size known from what the image is for

#include <RawImage.h>
#include <gtest/gtest.h>

using namespace openblack;

TEST(RawImage, ExactSizesDecode)
{
	const std::vector<uint8_t> rgb {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
	const auto image = rawimage::DecodeRgb(rgb, 2, 2);
	ASSERT_TRUE(image.has_value());
	EXPECT_EQ(image->width, 2u);
	EXPECT_EQ(image->At(1, 0), (std::array<uint8_t, 3> {4, 5, 6}));
	EXPECT_EQ(image->At(0, 1), (std::array<uint8_t, 3> {7, 8, 9}));

	const std::vector<uint8_t> grey {10, 20, 30, 40, 50, 60};
	const auto alpha = rawimage::DecodeGrey(grey, 3, 2);
	ASSERT_TRUE(alpha.has_value());
	EXPECT_EQ(alpha->At(2, 0), 30);
	EXPECT_EQ(alpha->At(0, 1), 40);
}

TEST(RawImage, AnyOtherSizeGivesNothing)
{
	const std::vector<uint8_t> bytes(11, 0);
	EXPECT_FALSE(rawimage::DecodeRgb(bytes, 2, 2).has_value());
	EXPECT_FALSE(rawimage::DecodeGrey(bytes, 2, 2).has_value());
	EXPECT_FALSE(rawimage::DecodeRgb(std::vector<uint8_t> {}, 0, 0).has_value());
	// an RGB image's bytes are not a grey image of the same size
	EXPECT_FALSE(rawimage::DecodeGrey(std::vector<uint8_t>(12, 0), 2, 2).has_value());
}
