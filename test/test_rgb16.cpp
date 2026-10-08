/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// graphics::rgb16 (src/Graphics/Rgb16.h) against the original's texture conversion: its 555 branch
// and its own 565 branch, emulated below, and the bit replication of the
// expansion (inferred: the driver's; the reference PNGs assume it too)

#include <cstdint>

#include <array>
#include <vector>

#include <gtest/gtest.h>

#include "Graphics/Rgb16.h"

using namespace openblack::graphics;

namespace
{
/// The original's 555 branch for one pixel, step by step
uint16_t Emulate555(uint8_t r, uint8_t g, uint8_t b)
{
	uint32_t x = r;
	uint32_t y = g;
	x &= 0x1F8u;
	x <<= 5;
	y &= 0x3FF8u;
	y += x;
	x = 0;
	x = (x & ~0xFFu) | b;
	x = (x & ~0xFFu) | ((x & 0xFFu) >> 3);
	x &= 0xFFFFu;
	const uint32_t packed = x + y * 4;
	return static_cast<uint16_t>(packed);
}

/// The original's raw 565 branch for one pixel
uint16_t EmulateRaw565(uint8_t r, uint8_t g, uint8_t b)
{
	uint32_t y = g;
	uint32_t x = 0;
	x = (x & ~0xFFu) | r;
	x = (x & ~0xFFu) | (x & 0xF8u);
	x &= 0xFFFFu;
	y &= 0x1FF8u;
	x <<= 5;
	y += x;
	x = 0;
	x = (x & ~0xFFu) | b;
	x = (x & ~0xFFu) | ((x & 0xFFu) >> 3);
	x &= 0xFFFFu;
	const uint32_t packed = x + y * 8;
	return static_cast<uint16_t>(packed);
}
} // namespace

TEST(Rgb16, QuantizeExpandCut)
{
	for (uint32_t v = 0; v < 256; ++v)
	{
		const auto byte = static_cast<uint8_t>(v);
		EXPECT_EQ(rgb16::Quantize5(byte), v >> 3);
		EXPECT_EQ(rgb16::Quantize6(byte), v >> 2);
		EXPECT_EQ(rgb16::Cut5(byte), (v & 0xF8u) | (v >> 5));
		EXPECT_EQ(rgb16::Cut6(byte), (v & 0xFCu) | (v >> 6));
		EXPECT_EQ(rgb16::Cut5(rgb16::Cut5(byte)), rgb16::Cut5(byte));
		EXPECT_EQ(rgb16::Cut6(rgb16::Cut6(byte)), rgb16::Cut6(byte));
	}
	EXPECT_EQ(rgb16::Expand5(0), 0);
	EXPECT_EQ(rgb16::Expand5(31), 255);
	EXPECT_EQ(rgb16::Expand5(16), 0x84);
	EXPECT_EQ(rgb16::Expand6(0), 0);
	EXPECT_EQ(rgb16::Expand6(63), 255);
	EXPECT_EQ(rgb16::Expand6(32), 0x82);
	// the bits above the field are ignored
	EXPECT_EQ(rgb16::Expand5(0xE0 | 31), 255);
	EXPECT_EQ(rgb16::Expand6(0xC0 | 63), 255);
}

TEST(Rgb16, Pack555IsTheRawBranch)
{
	for (uint32_t r = 0; r < 256; ++r)
	{
		for (uint32_t g = 0; g < 256; ++g)
		{
			for (uint32_t b = 0; b < 256; b += 7)
			{
				const auto rr = static_cast<uint8_t>(r);
				const auto gg = static_cast<uint8_t>(g);
				const auto bb = static_cast<uint8_t>(b);
				ASSERT_EQ(rgb16::Pack555(rr, gg, bb), Emulate555(rr, gg, bb)) << r << " " << g << " " << b;
				ASSERT_EQ(rgb16::PackRaw565(rr, gg, bb), EmulateRaw565(rr, gg, bb)) << r << " " << g << " " << b;
			}
		}
	}
	EXPECT_EQ(rgb16::Pack555(0xFF, 0xFF, 0xFF), 0x7FFF);    // bit 15 stays 0
	EXPECT_EQ(rgb16::PackRaw565(0xFF, 0xFF, 0xFF), 0xFFDF); // the lowest green bit stays 0
}

TEST(Rgb16, Pack565)
{
	EXPECT_EQ(rgb16::Pack565(0xFF, 0xFF, 0xFF), 0xFFFF);
	EXPECT_EQ(rgb16::Pack565(0xFF, 0, 0), 0xF800);
	EXPECT_EQ(rgb16::Pack565(0, 0xFF, 0), 0x07E0);
	EXPECT_EQ(rgb16::Pack565(0, 0, 0xFF), 0x001F);
	EXPECT_EQ(rgb16::Pack565(0, 0x04, 0), 0x0020); // 6 bits of green
	EXPECT_EQ(rgb16::Pack555(0xFF, 0, 0), 0x7C00);
	EXPECT_EQ(rgb16::Pack555(0, 0xFF, 0), 0x03E0);
	EXPECT_EQ(rgb16::Pack555(0, 0, 0xFF), 0x001F);
	EXPECT_EQ(rgb16::Pack(rgb16::Format::Rgb555, 1, 2, 3), rgb16::Pack555(1, 2, 3));
	EXPECT_EQ(rgb16::Pack(rgb16::Format::Rgb565, 1, 2, 3), rgb16::Pack565(1, 2, 3));
}

TEST(Rgb16, UnpackIsTheCut)
{
	for (uint32_t v = 0; v < 256; v += 3)
	{
		const auto a = static_cast<uint8_t>(v);
		const auto b = static_cast<uint8_t>(255 - v);
		const auto c = static_cast<uint8_t>(v * 7);
		const std::array<uint8_t, 4> cut555 {rgb16::Cut5(a), rgb16::Cut5(b), rgb16::Cut5(c), 0xFF};
		const std::array<uint8_t, 4> cut565 {rgb16::Cut5(a), rgb16::Cut6(b), rgb16::Cut5(c), 0xFF};
		EXPECT_EQ(rgb16::Unpack555(rgb16::Pack555(a, b, c)), cut555);
		EXPECT_EQ(rgb16::Unpack565(rgb16::Pack565(a, b, c)), cut565);
		EXPECT_EQ(rgb16::Unpack(rgb16::Format::Rgb555, rgb16::Pack555(a, b, c)), cut555);
		EXPECT_EQ(rgb16::Unpack(rgb16::Format::Rgb565, rgb16::Pack565(a, b, c)), cut565);
	}
	// bit 15 of a 555 texel is not read
	EXPECT_EQ(rgb16::Unpack555(0x8000), (std::array<uint8_t, 4> {0, 0, 0, 0xFF}));
}

TEST(Rgb16, QuantizeAndExpandSpans)
{
	const std::vector<uint8_t> rgba {0x12, 0x34, 0x56, 0x00, 0xFF, 0x80, 0x07, 0x99};
	std::vector<uint16_t> texels(2);
	rgb16::Quantize(rgb16::Format::Rgb555, rgba, texels);
	EXPECT_EQ(texels[0], rgb16::Pack555(0x12, 0x34, 0x56));
	EXPECT_EQ(texels[1], rgb16::Pack555(0xFF, 0x80, 0x07));
	std::vector<uint8_t> back(8, 0x11);
	rgb16::Expand(rgb16::Format::Rgb555, texels, back);
	const std::vector<uint8_t> expected {rgb16::Cut5(0x12), rgb16::Cut5(0x34), rgb16::Cut5(0x56), 0xFF,
	                                     rgb16::Cut5(0xFF), rgb16::Cut5(0x80), rgb16::Cut5(0x07), 0xFF};
	EXPECT_EQ(back, expected);

	// the shorter of the two decides
	std::vector<uint16_t> one(1, 0xABCD);
	rgb16::Quantize(rgb16::Format::Rgb565, rgba, one);
	EXPECT_EQ(one[0], rgb16::Pack565(0x12, 0x34, 0x56));
	std::vector<uint8_t> four(4);
	rgb16::Expand(rgb16::Format::Rgb565, texels, four);
	EXPECT_EQ(four[3], 0xFF);
}
