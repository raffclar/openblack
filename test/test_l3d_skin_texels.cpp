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

#include <gtest/gtest.h>

#include "3D/L3DSkinTexels.h"

using namespace openblack;

TEST(L3DSkinTexels, ModelsWithoutTheFlagKeepTheirSkinsAsTheyAre)
{
	EXPECT_EQ(l3d_skin::LayoutOf(0x2000U, true), l3d_skin::Layout::Argb4444);
	EXPECT_EQ(l3d_skin::LayoutOf(0x2000U, false), l3d_skin::Layout::Argb4444);
	constexpr std::array<uint16_t, 2> k_Texels {0x1234U, 0xF0A5U};
	const auto bytes = l3d_skin::TexelsIn(l3d_skin::Layout::Argb4444, k_Texels);
	ASSERT_EQ(bytes.size(), 4U);
	EXPECT_EQ(bytes[0], 0x34U);
	EXPECT_EQ(bytes[1], 0x12U);
	EXPECT_EQ(bytes[2], 0xA5U);
	EXPECT_EQ(bytes[3], 0xF0U);
}

TEST(L3DSkinTexels, ModelsWithTheFlagHaveOpaqueFiveBitSkins)
{
	EXPECT_EQ(l3d_skin::LayoutOf(0x2000U | 0x10000U, true), l3d_skin::Layout::Argb1555);
	EXPECT_EQ(l3d_skin::LayoutOf(0x10000U, false), l3d_skin::Layout::Rgba8);
	// A grey texel of 5 bits for each colour (16 of 31), with the top bit clear as the file keeps it
	constexpr uint16_t k_Grey = (16U << 10U) | (16U << 5U) | 16U;
	constexpr std::array<uint16_t, 1> k_Texels {k_Grey};
	const auto opaque = l3d_skin::TexelsIn(l3d_skin::Layout::Argb1555, k_Texels);
	ASSERT_EQ(opaque.size(), 2U);
	EXPECT_EQ(opaque[0] | (opaque[1] << 8U), 0x8000U | k_Grey);
	const auto bytes = l3d_skin::TexelsIn(l3d_skin::Layout::Rgba8, k_Texels);
	ASSERT_EQ(bytes.size(), 4U);
	EXPECT_EQ(bytes[0], bytes[1]);
	EXPECT_EQ(bytes[1], bytes[2]);
	EXPECT_EQ(bytes[3], 0xFFU);
}

TEST(L3DSkinTexels, OpaqueSkinsAreSolidAtEachTexel)
{
	const auto alpha = l3d_skin::SolidAlphaTexels(3);
	ASSERT_EQ(alpha.size(), 3U);
	for (const auto texel : alpha)
	{
		EXPECT_EQ(texel & 0xF000U, 0xF000U);
	}
}
