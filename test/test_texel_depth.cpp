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

#include <gtest/gtest.h>

#include "Graphics/TexelDepth.h"

namespace texel_depth = openblack::graphics::texel_depth;

TEST(TexelDepth, FourBitsKeepOnlyTheTopFourBitsReadBackAsFifteenths)
{
	// The low four bits are dropped, not rounded: 0x1F stays with 0x10, a whisker short of 0x20
	EXPECT_EQ(texel_depth::FourBits(0x00), 0x00);
	EXPECT_EQ(texel_depth::FourBits(0x0F), 0x00);
	EXPECT_EQ(texel_depth::FourBits(0x10), 0x11);
	EXPECT_EQ(texel_depth::FourBits(0x1F), 0x11);
	EXPECT_EQ(texel_depth::FourBits(0x80), 0x88);
	EXPECT_EQ(texel_depth::FourBits(0xF0), 0xFF);
	EXPECT_EQ(texel_depth::FourBits(0xFF), 0xFF);
	// Every value read back is one of sixteen evenly spaced levels, n * 17
	for (int value = 0; value < 256; ++value)
	{
		const auto reduced = texel_depth::FourBits(static_cast<uint8_t>(value));
		EXPECT_EQ(reduced % 17, 0);
		EXPECT_EQ(reduced / 17, value >> 4);
	}
}

TEST(TexelDepth, ReduceLeavesFullDepthTexelsAsTheyAre)
{
	std::vector<uint8_t> texels {0x12, 0x34, 0x56, 0x78};
	texel_depth::Reduce(texels, texel_depth::Depth::Full);
	EXPECT_EQ(texels, (std::vector<uint8_t> {0x12, 0x34, 0x56, 0x78}));
	texel_depth::Reduce(texels, texel_depth::Depth::FourBits);
	EXPECT_EQ(texels, (std::vector<uint8_t> {0x11, 0x33, 0x55, 0x77}));
}

TEST(TexelDepth, Rgb555KeepsTheTopFiveBitsOfEachChannelAndIsOpaque)
{
	// Red in bits 10 to 14, green in 5 to 9, blue in 0 to 4, and the top bit set
	EXPECT_EQ(texel_depth::Rgb555(0x00, 0x00, 0x00), 0x8000);
	EXPECT_EQ(texel_depth::Rgb555(0xFF, 0x00, 0x00), 0xFC00);
	EXPECT_EQ(texel_depth::Rgb555(0x00, 0xFF, 0x00), 0x83E0);
	EXPECT_EQ(texel_depth::Rgb555(0x00, 0x00, 0xFF), 0x801F);
	// The low three bits are dropped, not rounded
	EXPECT_EQ(texel_depth::Rgb555(0x07, 0x0F, 0x17), 0x8000 | (0u << 10u) | (1u << 5u) | 2u);
	for (int value = 0; value < 256; ++value)
	{
		const auto v = static_cast<uint8_t>(value);
		EXPECT_EQ(texel_depth::Rgb555(v, v, v) & 0x1Fu, static_cast<unsigned>(value >> 3));
	}
}

TEST(TexelDepth, Rgb565KeepsOnlyFiveBitsOfGreenToo)
{
	EXPECT_EQ(texel_depth::Rgb565(0xFF, 0x00, 0x00), 0xF800);
	EXPECT_EQ(texel_depth::Rgb565(0x00, 0x00, 0xFF), 0x001F);
	// Green's sixth bit stays clear even at full green
	EXPECT_EQ(texel_depth::Rgb565(0x00, 0xFF, 0x00), 0x07C0);
	EXPECT_EQ(texel_depth::Rgb565(0x00, 0xFC, 0x00) & 0x20u, 0u);
}

TEST(TexelDepth, PackRgb555PacksColoursAndGreys)
{
	const std::array<uint8_t, 6> rgb {0xFF, 0x80, 0x08, 0x00, 0x00, 0x00};
	EXPECT_EQ(texel_depth::PackRgb555(rgb, 3), (std::vector<uint16_t> {texel_depth::Rgb555(0xFF, 0x80, 0x08), 0x8000}));
	const std::array<uint8_t, 2> grey {0x80, 0xFF};
	EXPECT_EQ(texel_depth::PackRgb555(grey, 1),
	          (std::vector<uint16_t> {texel_depth::Rgb555(0x80, 0x80, 0x80), texel_depth::Rgb555(0xFF, 0xFF, 0xFF)}));
}

TEST(TexelDepth, RawFilesKeepTheDepthTheGameLoadsThemAt)
{
	constexpr size_t k_Colours = static_cast<size_t>(256) * 256 * 3;
	constexpr size_t k_Alpha = static_cast<size_t>(256) * 256;
	using texel_depth::Classify;
	using texel_depth::Depth;
	using texel_depth::DepthOf;
	using texel_depth::RawFile;
	EXPECT_EQ(Classify(k_Colours, true, false), RawFile::ColoursWithAlpha);
	EXPECT_EQ(Classify(k_Colours, false, false), RawFile::Colours);
	EXPECT_EQ(Classify(k_Alpha, false, true), RawFile::Alpha);
	// A small grey picture drawn as an alpha, and small pictures read as numbers
	EXPECT_EQ(Classify(32 * 32, false, true), RawFile::Alpha);
	EXPECT_EQ(Classify(40 * 40, false, false), RawFile::Data);
	EXPECT_EQ(Classify(k_Alpha, false, false), RawFile::Data);
	EXPECT_EQ(DepthOf(RawFile::ColoursWithAlpha), Depth::FourBits);
	EXPECT_EQ(DepthOf(RawFile::Alpha), Depth::FourBits);
	EXPECT_EQ(DepthOf(RawFile::Colours), Depth::FiveBits);
	EXPECT_EQ(DepthOf(RawFile::Data), Depth::Full);
}
