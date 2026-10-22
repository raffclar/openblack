/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>

#include <gtest/gtest.h>

#include "Common/Bitmap16B.h"

using openblack::Texel1555Bytes;

TEST(Texel1555Bytes, ExpandsEachColourToTheNearestByte)
{
	EXPECT_EQ(Texel1555Bytes(0xFFFFU), (std::array<uint8_t, 4> {255, 255, 255, 255}));
	EXPECT_EQ(Texel1555Bytes(0x0000U), (std::array<uint8_t, 4> {0, 0, 0, 0}));
	// Red 31, green 0, blue 16: 16 of 31 is the byte 132 (131.6 to the nearest)
	EXPECT_EQ(Texel1555Bytes(0x7C10U), (std::array<uint8_t, 4> {255, 0, 132, 0}));
	// Green 1 is the byte 8 (8.2 to the nearest)
	EXPECT_EQ(Texel1555Bytes(0x8020U), (std::array<uint8_t, 4> {0, 8, 0, 255}));
}
