/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The original Bink library's YUV -> RGB (src/Video/BinkYuv.h): the colour tables and the nearest chroma.

#include <cstdint>

#include <vector>

#include <gtest/gtest.h>

#include "Video/BinkYuv.h"

using namespace openblack::video;

TEST(BinkYuv, Tables)
{
	// y' = max(0, 76309 * (Y - 16) >> 16): 16 -> 0, 22 -> 6, 23 -> 8, 235 -> 254, under 16 -> 0, over 235 unclamped
	EXPECT_EQ(bink_yuv::k_Luma[16], 0);
	EXPECT_EQ(bink_yuv::k_Luma[22], 6);
	EXPECT_EQ(bink_yuv::k_Luma[23], 8);
	EXPECT_EQ(bink_yuv::k_Luma[162], 169); // 146 * 255 / 219 is 170 exactly: 76309 is just under 255 / 219
	EXPECT_EQ(bink_yuv::k_Luma[235], 254);
	EXPECT_EQ(bink_yuv::k_Luma[10], 0);
	EXPECT_EQ(bink_yuv::k_Luma[255], 278);
	// The chroma tables round toward zero
	EXPECT_EQ(bink_yuv::k_BlueFromU[70], -117); // the classic 132201 would give -116
	EXPECT_EQ(bink_yuv::k_BlueFromU[71], -114);
	EXPECT_EQ(bink_yuv::k_BlueFromU[182], 108);
	EXPECT_EQ(bink_yuv::k_RedFromV[40], -140);
	EXPECT_EQ(bink_yuv::k_RedFromV[215], 138);
	EXPECT_EQ(bink_yuv::k_GreenFromU[128], 0);
	EXPECT_EQ(bink_yuv::k_GreenFromV[128], 0);
	EXPECT_EQ(bink_yuv::k_GreenFromV[129], 0);
	EXPECT_EQ(bink_yuv::k_GreenFromV[127], 0);
	// Grey stays grey; the clamp is on the sum
	const auto grey = bink_yuv::ToRgb(128, 128, 128);
	EXPECT_EQ(grey.r, 130);
	EXPECT_EQ(grey.g, 130);
	EXPECT_EQ(grey.b, 130);
	const auto dark = bink_yuv::ToRgb(12, 128, 124); // golden: G 3 with y' 0 (-53279 * -4 >> 16)
	EXPECT_EQ(dark.g, 3);
	EXPECT_EQ(dark.r, 0);
}

TEST(BinkYuv, NearestChroma)
{
	// 4x2 picture: one U / V per 2x2 block
	const uint8_t y[8] = {128, 128, 128, 128, 128, 128, 128, 128};
	const uint8_t u[2] = {128, 70};
	const uint8_t v[2] = {128, 128};
	std::vector<uint8_t> rgba(4 * 2 * 4);
	bink_yuv::CopyToRgba8({y, u, v, 4, 2, 2, 4, 2}, rgba);
	for (int row = 0; row < 2; ++row)
	{
		for (int x = 0; x < 4; ++x)
		{
			const auto* p = &rgba[static_cast<size_t>((row * 4 + x) * 4)];
			EXPECT_EQ(p[2], x < 2 ? 130 : 13) << x << "," << row; // 130 - 117
			EXPECT_EQ(p[3], 0xFF);
		}
	}
}

TEST(BinkYuv, OnePassIsTheThreePasses)
{
	// CopyToRgb16AndRgba8 gives the bytes of CopyToRgba8, rgb16::Quantize and rgb16::Expand, for both formats, on every
	// Y, U and V value (an odd width and height, so the last chroma column and row are shared by one pixel)
	constexpr uint32_t k_Width = 257;
	constexpr uint32_t k_Height = 9;
	constexpr uint32_t k_ChromaWidth = (k_Width + 1) / 2;
	constexpr uint32_t k_ChromaHeight = (k_Height + 1) / 2;
	std::vector<uint8_t> y(static_cast<size_t>(k_Width) * k_Height);
	std::vector<uint8_t> u(static_cast<size_t>(k_ChromaWidth) * k_ChromaHeight);
	std::vector<uint8_t> v(u.size());
	for (size_t i = 0; i < y.size(); ++i)
	{
		y[i] = static_cast<uint8_t>(i * 7);
	}
	for (size_t i = 0; i < u.size(); ++i)
	{
		u[i] = static_cast<uint8_t>(i);
		v[i] = static_cast<uint8_t>(255 - i * 3);
	}
	const openblack::video::bink_yuv::Planes planes {y.data(),      u.data(),      v.data(), k_Width,
	                                                 k_ChromaWidth, k_ChromaWidth, k_Width,  k_Height};
	const size_t pixels = static_cast<size_t>(k_Width) * k_Height;
	for (const auto format : {openblack::graphics::rgb16::Format::Rgb555, openblack::graphics::rgb16::Format::Rgb565})
	{
		std::vector<uint8_t> rgba8(pixels * 4);
		std::vector<uint16_t> texels(pixels);
		std::vector<uint8_t> sampled(pixels * 4);
		openblack::video::bink_yuv::CopyToRgba8(planes, rgba8);
		openblack::graphics::rgb16::Quantize(format, rgba8, texels);
		openblack::graphics::rgb16::Expand(format, texels, sampled);

		std::vector<uint16_t> fusedTexels(pixels, 0xABCD);
		std::vector<uint8_t> fusedSampled(pixels * 4, 0x5A);
		openblack::video::bink_yuv::CopyToRgb16AndRgba8(planes, format, fusedTexels, fusedSampled);
		EXPECT_EQ(fusedTexels, texels);
		EXPECT_EQ(fusedSampled, sampled);
	}
	// too small a buffer: nothing written
	std::vector<uint16_t> small(pixels - 1, 7);
	std::vector<uint8_t> rgba(pixels * 4, 9);
	openblack::video::bink_yuv::CopyToRgb16AndRgba8(planes, openblack::graphics::rgb16::Format::Rgb555, small, rgba);
	EXPECT_EQ(small, std::vector<uint16_t>(pixels - 1, 7));
	EXPECT_EQ(rgba, std::vector<uint8_t>(pixels * 4, 9));
}
