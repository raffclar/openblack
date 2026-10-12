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
