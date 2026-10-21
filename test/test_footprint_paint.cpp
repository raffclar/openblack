/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <random>
#include <span>
#include <utility>
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

namespace
{
/// The paint as it was before it filled only the rows that the edges reach: the oracle that the faster paint must give
/// bit for bit
namespace reference
{
using namespace openblack::footprint_paint;
/// A corner of a triangle on the block's texture: its row (along x) and column (along z) in texels, and its texel on
/// the picture
struct Corner
{
	float row;
	float column;
	float u;
	float v;
};

/// The span of one row of the block's texture that the edges of a triangle reach: its lowest and highest columns, and
/// the picture's coordinates at each of them
struct Span
{
	bool used {false};
	int low {0};
	int high {0};
	float uLow {0.0f};
	float uHigh {0.0f};
	float vLow {0.0f};
	float vHigh {0.0f};
};

using Spans = std::array<Span, k_BlockTexels>;

/// A value to the nearest whole number, a half to the even one, as the game's processor rounds it
[[nodiscard]] int Nearest(float value)
{
	return static_cast<int>(std::lrint(value));
}

/// Widens the span of a row to a column an edge reached. A column below the span becomes its low end, else a column
/// above it becomes its high end. The rows outside the block are not kept.
void Reach(Spans& spans, int row, int column, float u, float v)
{
	if (row < 0 || row >= k_BlockTexels)
	{
		return;
	}
	auto& span = spans.at(static_cast<size_t>(row));
	if (!span.used)
	{
		span = {.used = true, .low = column, .high = column, .uLow = u, .uHigh = u, .vLow = v, .vHigh = v};
		return;
	}
	if (column < span.low)
	{
		span.low = column;
		span.uLow = u;
		span.vLow = v;
	}
	else if (column > span.high)
	{
		span.high = column;
		span.uHigh = u;
		span.vHigh = v;
	}
}

/// Walks an edge from the corner with the lower row to the other, a texel at a time along its longer axis. The other
/// axis steps when the error that it gathers is more than the longer axis. The picture's coordinates step evenly.
void WalkEdge(Spans& spans, Corner start, Corner end)
{
	if (start.row > end.row)
	{
		std::swap(start, end);
	}
	int row = Nearest(start.row);
	int column = Nearest(start.column);
	const int rows = Nearest(end.row) - row;
	const int columnsSigned = Nearest(end.column) - column;
	const int rowStep = rows < 0 ? -1 : 1;
	const int columnStep = columnsSigned < 0 ? -1 : 1;
	const int rowsAcross = std::abs(rows);
	const int columnsAcross = std::abs(columnsSigned);
	float u = start.u;
	float v = start.v;
	int error = 0;
	Reach(spans, row, column, u, v);
	if (rowsAcross < columnsAcross)
	{
		const float du = (end.u - start.u) / static_cast<float>(columnsAcross);
		const float dv = (end.v - start.v) / static_cast<float>(columnsAcross);
		for (int step = 0; step < columnsAcross; ++step)
		{
			v = dv + v;
			column += columnStep;
			error += rowsAcross;
			u = du + u;
			if (error > columnsAcross)
			{
				row += rowStep;
				error -= columnsAcross;
			}
			Reach(spans, row, column, u, v);
		}
		return;
	}
	if (rowsAcross == 0)
	{
		return;
	}
	const float du = (end.u - start.u) / static_cast<float>(rowsAcross);
	const float dv = (end.v - start.v) / static_cast<float>(rowsAcross);
	for (int step = 0; step < rowsAcross; ++step)
	{
		u = du + u;
		row += rowStep;
		error += columnsAcross;
		v = dv + v;
		if (error > rowsAcross)
		{
			column += columnStep;
			error -= rowsAcross;
		}
		Reach(spans, row, column, u, v);
	}
}

/// Fills each row that the edges reached, from its low column to its high column, both ends too. The picture's
/// coordinates step evenly from those at the low end to those at the high end. The columns outside the block are not
/// painted. Each row is empty again after.
void FillSpans(Spans& spans, std::span<uint16_t> block, const Picture& picture, Mix mix)
{
	for (int row = 0; row < k_BlockTexels; ++row)
	{
		auto& span = spans.at(static_cast<size_t>(row));
		if (!span.used)
		{
			continue;
		}
		float du = 0.0f;
		float dv = 0.0f;
		const int across = span.high - span.low;
		if (across != 0)
		{
			const float inverse = 1.0f / static_cast<float>(across);
			du = (span.uHigh - span.uLow) * inverse;
			dv = (span.vHigh - span.vLow) * inverse;
		}
		float u = span.uLow;
		float v = span.vLow;
		const int last = std::min(span.high, k_BlockTexels - 1);
		for (int column = span.low; column <= last; ++column)
		{
			if (column >= 0)
			{
				const int texelU = std::clamp(Nearest(u), 0, picture.width - 1);
				const int texelV = std::clamp(Nearest(v), 0, picture.height - 1);
				const auto texel = picture.texels[static_cast<size_t>((picture.width * texelV) + texelU)];
				auto& land = block[static_cast<size_t>((row * k_BlockTexels) + column)];
				land = mix == Mix::Colour ? Paint(land, texel) : LowerAlpha(land, texel);
			}
			u = u + du;
			v = v + dv;
		}
		span.used = false;
	}
}
void PaintFootprint(std::span<uint16_t> block, glm::ivec2 blockIndex, const Picture& picture,
                    std::span<const Triangle> triangles, const Placement& placement, Mix mix)
{
	if (block.size() < static_cast<size_t>(k_BlockTexels) * k_BlockTexels || picture.width <= 0 || picture.height <= 0 ||
	    picture.texels.size() < static_cast<size_t>(picture.width) * static_cast<size_t>(picture.height))
	{
		return;
	}
	const float rowStart = static_cast<float>(blockIndex.x) * k_BlockUnits;
	const float columnStart = static_cast<float>(blockIndex.y) * k_BlockUnits;
	constexpr auto k_Texels = static_cast<float>(k_BlockTexels);
	// The game rounds each step to a float, in this order
	const auto toBlock = [&](glm::vec2 ground, glm::vec2 texel) {
		const float worldX = ((placement.xFromZ * ground.y + placement.xFromX * ground.x) + 0.0f) + placement.origin.x;
		const float worldZ = ((placement.zFromX * ground.x + placement.zFromZ * ground.y) + 0.0f) + placement.origin.y;
		return Corner {
		    .row = ((worldX - rowStart) * k_Texels) * k_UnitPerBlock,
		    .column = ((worldZ - columnStart) * k_Texels) * k_UnitPerBlock,
		    .u = texel.x,
		    .v = texel.y,
		};
	};
	Spans spans {};
	for (const auto& triangle : triangles)
	{
		std::array<Corner, 3> corners {};
		for (size_t i = 0; i < corners.size(); ++i)
		{
			corners.at(i) = toBlock(triangle.corners.at(i), triangle.texels.at(i));
		}
		WalkEdge(spans, corners[0], corners[1]);
		WalkEdge(spans, corners[1], corners[2]);
		WalkEdge(spans, corners[2], corners[0]);
		FillSpans(spans, block, picture, mix);
	}
}

} // namespace reference
} // namespace

TEST(FootprintPaint, TheFasterPaintGivesTheBitsOfTheOldWay)
{
	std::mt19937 random(42);
	std::uniform_real_distribution<float> place(-40.0f, 200.0f);
	std::uniform_real_distribution<float> turn(-1.5f, 1.5f);
	std::uniform_real_distribution<float> texel(-2.0f, 34.0f);
	std::uniform_int_distribution<int> colour(0, 0xFFFF);
	std::vector<uint16_t> picture(32 * 32);
	for (auto& value : picture)
	{
		value = static_cast<uint16_t>(colour(random));
	}
	for (int round = 0; round < 400; ++round)
	{
		std::vector<footprint_paint::Triangle> triangles(1 + (round % 6));
		for (auto& triangle : triangles)
		{
			for (size_t i = 0; i < 3; ++i)
			{
				triangle.corners.at(i) = glm::vec2(turn(random) * 30.0f, turn(random) * 30.0f);
				triangle.texels.at(i) = glm::vec2(texel(random), texel(random));
			}
		}
		const footprint_paint::Placement placement {.xFromX = turn(random),
		                                            .xFromZ = turn(random),
		                                            .zFromX = turn(random),
		                                            .zFromZ = turn(random),
		                                            .origin = glm::vec2(place(random), place(random))};
		const auto mix = round % 3 == 0 ? footprint_paint::Mix::LowerAlpha : footprint_paint::Mix::Colour;
		const glm::ivec2 blockIndex {round % 2, (round / 2) % 2};
		auto fast = BlackBlock();
		for (auto& value : fast)
		{
			value = static_cast<uint16_t>(colour(random));
		}
		auto old = fast;
		const footprint_paint::Picture shape {.width = 32, .height = 32, .texels = picture};
		footprint_paint::PaintFootprint(fast, blockIndex, shape, triangles, placement, mix);
		reference::PaintFootprint(old, blockIndex, shape, triangles, placement, mix);
		ASSERT_EQ(fast, old) << "round " << round;
	}
}
