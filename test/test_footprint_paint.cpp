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

#include "3D/FootprintPaint.h"

namespace footprint_paint = openblack::footprint_paint;

namespace
{
constexpr int k_Side = footprint_paint::k_BlockTexels;

/// A block of black land, opaque
std::vector<uint16_t> BlackBlock()
{
	return std::vector<uint16_t>(static_cast<size_t>(k_Side) * k_Side, 0xF000);
}

uint16_t At(const std::vector<uint16_t>& block, int row, int column)
{
	return block[static_cast<size_t>(row * k_Side + column)];
}

/// A square of 10 units (16 texels) a side from the origin, as two triangles that share the diagonal from (0, 0) to
/// (10, 10), with the picture's texel coordinates equal to the corners'
std::array<footprint_paint::Triangle, 2> Square()
{
	return {{
	    {.corners = {glm::vec2(0.0f, 0.0f), glm::vec2(10.0f, 0.0f), glm::vec2(10.0f, 10.0f)},
	     .texels = {glm::vec2(0.0f, 0.0f), glm::vec2(10.0f, 0.0f), glm::vec2(10.0f, 10.0f)}},
	    {.corners = {glm::vec2(0.0f, 0.0f), glm::vec2(10.0f, 10.0f), glm::vec2(0.0f, 10.0f)},
	     .texels = {glm::vec2(0.0f, 0.0f), glm::vec2(10.0f, 10.0f), glm::vec2(0.0f, 10.0f)}},
	}};
}

void PaintSquare(std::vector<uint16_t>& block, const std::vector<uint16_t>& picture, int side,
                 const footprint_paint::Placement& placement = {}, footprint_paint::Mix mix = footprint_paint::Mix::Colour,
                 glm::ivec2 blockIndex = {0, 0})
{
	const auto square = Square();
	footprint_paint::PaintFootprint(block, blockIndex, {.width = side, .height = side, .texels = picture}, square, placement,
	                                mix);
}
} // namespace

TEST(FootprintPaint, OpaqueTexelsStillLetALittleLandThrough)
{
	EXPECT_EQ(footprint_paint::Coverage(0), 0);
	EXPECT_EQ(footprint_paint::Coverage(8), 128);
	EXPECT_EQ(footprint_paint::Coverage(15), 240);
	// A white footprint fully opaque over black land reaches only 14 of 15
	EXPECT_EQ(footprint_paint::MixChannel(0, 15, 15), 14);
	// Black over white leaves the land at 0: 15 * 15 / 255 rounds down
	EXPECT_EQ(footprint_paint::MixChannel(15, 0, 15), 0);
}

TEST(FootprintPaint, ClearTexelsLeaveTheLandAsItWas)
{
	for (uint8_t land = 0; land < 16; ++land)
	{
		EXPECT_EQ(footprint_paint::MixChannel(land, 15, 0), land);
	}
	EXPECT_EQ(footprint_paint::Paint(0x9ABC, 0x0FFF), 0x9ABC);
}

TEST(FootprintPaint, ChannelsAreRoundedDown)
{
	// Half covered (8 x 16 = 128 of 255): (4 x 127 + 9 x 128) / 255 = 6.5 -> 6
	EXPECT_EQ(footprint_paint::MixChannel(4, 9, 8), 6);
}

TEST(FootprintPaint, PaintingKeepsTheLandsCoastAlpha)
{
	const auto painted = footprint_paint::Paint(0x3000, 0xFFFF);
	EXPECT_EQ(painted & 0xF000, 0x3000);
	EXPECT_EQ(painted & 0x0FFF, 0x0EEE);
}

TEST(FootprintPaint, AChannelLowersTheAlphaAndKeepsTheColour)
{
	EXPECT_EQ(footprint_paint::LowerAlpha(0xF123, 0x3FFF), 0x3123);
	// A channel more opaque than the land leaves the land as it is
	EXPECT_EQ(footprint_paint::LowerAlpha(0x2123, 0x9000), 0x2123);
}

TEST(FootprintPaint, BothEndsOfEachRowAreFilled)
{
	const std::vector<uint16_t> picture(16 * 16, 0xFFFF);
	auto block = BlackBlock();
	PaintSquare(block, picture, 16);
	// 10 units are 16 texels: the rows and the columns 0 to 16 are painted, both ends too
	EXPECT_NE(At(block, 16, 16), 0xF000);
	EXPECT_NE(At(block, 16, 3), 0xF000);
	EXPECT_NE(At(block, 3, 16), 0xF000);
	EXPECT_EQ(At(block, 17, 3), 0xF000);
	EXPECT_EQ(At(block, 3, 17), 0xF000);
}

TEST(FootprintPaint, TheSharedDiagonalIsPaintedTwice)
{
	// Half covered white: one coat takes black to 7, a second coat takes it to 11
	const std::vector<uint16_t> picture(16 * 16, 0x8FFF);
	auto block = BlackBlock();
	PaintSquare(block, picture, 16);
	// The walk along the diagonal steps across only when its error is more than the length, so it keeps one texel
	// behind the true diagonal
	EXPECT_EQ(At(block, 5, 4) & 0xF, 11);
	EXPECT_EQ(At(block, 5, 5) & 0xF, 7);
	EXPECT_EQ(At(block, 9, 2) & 0xF, 7);
	EXPECT_EQ(At(block, 2, 9) & 0xF, 7);
	// The first row is the edge of both triangles at its first texel only
	EXPECT_EQ(At(block, 0, 0) & 0xF, 11);
	EXPECT_EQ(At(block, 0, 5) & 0xF, 7);
}

TEST(FootprintPaint, EachLandTexelTakesTheNearestPictureTexelKeptInsideThePicture)
{
	// A picture of 4 by 4 with a different blue in each column; the coordinates of the square run to 10, past its edge
	std::vector<uint16_t> picture(16);
	for (int v = 0; v < 4; ++v)
	{
		for (int u = 0; u < 4; ++u)
		{
			picture[static_cast<size_t>(v * 4 + u)] = static_cast<uint16_t>(0xF000 | (u + 1));
		}
	}
	auto block = BlackBlock();
	PaintSquare(block, picture, 4);
	// At row 16 the picture's u is 10 at every column: past the picture, so its last column, blue 4
	EXPECT_EQ(At(block, 16, 8) & 0xF, footprint_paint::MixChannel(0, 4, 15));
	// At row 1 and column 0 the picture's u is 0.625: its texel 1, blue 2, painted by both triangles
	EXPECT_EQ(At(block, 1, 0) & 0xF, footprint_paint::MixChannel(footprint_paint::MixChannel(0, 2, 15), 2, 15));
}

TEST(FootprintPaint, OnlyTheBlocksOwnTexelsArePainted)
{
	const std::vector<uint16_t> picture(16 * 16, 0xFFFF);
	auto block = BlackBlock();
	// The square from 5 units before the corner of the block along x, and 5 before its far edge along z: three
	// quarters of it fall outside
	PaintSquare(block, picture, 16, {.origin = {-5.0f, 155.0f}});
	EXPECT_NE(At(block, 0, 255), 0xF000);
	EXPECT_NE(At(block, 8, 248), 0xF000);
	EXPECT_EQ(At(block, 9, 248), 0xF000);
	EXPECT_EQ(At(block, 8, 247), 0xF000);
}

TEST(FootprintPaint, TheBlockMovesTheFootprintByItsCorner)
{
	const std::vector<uint16_t> picture(16 * 16, 0xFFFF);
	auto block = BlackBlock();
	// Block (2, 1) starts at 320 along x and 160 along z
	PaintSquare(block, picture, 16, {.origin = {330.0f, 170.0f}}, footprint_paint::Mix::Colour, {2, 1});
	EXPECT_EQ(At(block, 15, 15), 0xF000);
	EXPECT_NE(At(block, 16, 16), 0xF000);
	EXPECT_NE(At(block, 32, 32), 0xF000);
	EXPECT_EQ(At(block, 33, 32), 0xF000);
}

TEST(FootprintPaint, CornersGoToTheNearestTexelAndAHalfGoesToTheEvenOne)
{
	const std::vector<uint16_t> picture(16 * 16, 0xFFFF);
	// 0.3125 units is half a texel: the near corner goes to texel 0, the far corner (16.5 texels) to texel 16
	auto block = BlackBlock();
	PaintSquare(block, picture, 16, {.origin = {0.3125f, 0.0f}});
	EXPECT_NE(At(block, 0, 0), 0xF000);
	EXPECT_NE(At(block, 16, 0), 0xF000);
	EXPECT_EQ(At(block, 17, 0), 0xF000);
	// 0.9375 units is a texel and a half: the near corner goes to texel 2, the far corner (17.5 texels) to texel 18
	block = BlackBlock();
	PaintSquare(block, picture, 16, {.origin = {0.9375f, 0.0f}});
	EXPECT_EQ(At(block, 1, 0), 0xF000);
	EXPECT_NE(At(block, 2, 0), 0xF000);
	EXPECT_NE(At(block, 18, 0), 0xF000);
	EXPECT_EQ(At(block, 19, 0), 0xF000);
}

TEST(FootprintPaint, AChannelPaintsOnlyTheAlpha)
{
	const std::vector<uint16_t> picture(16 * 16, 0x2FFF);
	std::vector<uint16_t> block(static_cast<size_t>(k_Side) * k_Side, 0xF123);
	PaintSquare(block, picture, 16, {}, footprint_paint::Mix::LowerAlpha);
	EXPECT_EQ(At(block, 5, 5), 0x2123);
	EXPECT_EQ(At(block, 20, 5), 0xF123);
}
