/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <algorithm>
#include <array>

#include <gtest/gtest.h>

#include "Graphics/ShapeShadows.h"

namespace shape_shadows = openblack::graphics::shape_shadows;
namespace tree_shadows = openblack::graphics::tree_shadows;

namespace
{

shape_shadows::Sample At(float column, float row)
{
	return {.column = column, .row = row};
}

uint8_t TexelAt(const tree_shadows::Coverage& coverage, int x, int z)
{
	return coverage[static_cast<size_t>(x * tree_shadows::k_BlockTexels + z)];
}

int CoveredTexels(const tree_shadows::Coverage& coverage)
{
	return static_cast<int>(std::ranges::count_if(coverage, [](uint8_t bits) { return bits != 0; }));
}

// A triangle facing the sun: its corners in the order it casts
constexpr std::array<shape_shadows::Sample, 3> k_Facing {
    {{.column = 0.0f, .row = 0.0f}, {.column = 0.0f, .row = 4.0f}, {.column = 8.0f, .row = 0.0f}}};

} // namespace

TEST(ShapeShadows, TheOriginStaysWhereItIs)
{
	const auto cast = shape_shadows::Cast({0.0f, 0.0f, 0.0f}, {1000.0f, 2000.0f});
	EXPECT_NEAR(cast.x, 1000.0f, 0.05f);
	EXPECT_NEAR(cast.y, 2000.0f, 0.05f);
}

TEST(ShapeShadows, FallsAsFarAlongXAndZAsItIsHigh)
{
	// The sun stands as far along -x and -z as it is high, so a point is cast as far again along +x and +z
	const auto cast = shape_shadows::Cast({0.0f, 20.0f, 0.0f}, {1000.0f, 2000.0f});
	EXPECT_NEAR(cast.x, 1000.0f + 20.0f * 501000.0f / 499980.0f, 0.1f);
	EXPECT_NEAR(cast.y, 2000.0f + 20.0f * 502000.0f / 499980.0f, 0.1f);
}

TEST(ShapeShadows, BelowTheOriginFallsStraightDown)
{
	const auto cast = shape_shadows::Cast({5.0f, -30.0f, -7.0f}, {1000.0f, 2000.0f});
	EXPECT_NEAR(cast.x, 1005.0f, 0.05f);
	EXPECT_NEAR(cast.y, 1993.0f, 0.05f);
}

TEST(ShapeShadows, FourColumnsAndTwoRowsATexel)
{
	const auto sample = shape_shadows::SampleOf({170.0f, 330.0f}, {160.0f, 320.0f});
	// 256 texels over 160 units: 6.4 columns along z and 3.2 rows along x a unit
	EXPECT_FLOAT_EQ(sample.column, 64.0f);
	EXPECT_FLOAT_EQ(sample.row, 32.0f);
}

TEST(ShapeShadows, CoversEachRowUpToTheFarEdge)
{
	tree_shadows::Coverage coverage {};
	shape_shadows::Fill(coverage, k_Facing, false);
	// Rows 0 to 3 run from column 0 up to, not including, 8, 6, 4 and 2. The even rows of a texel take its low four
	// bits, the odd its high, a bit for each column.
	EXPECT_EQ(TexelAt(coverage, 0, 0), 0xFF);
	EXPECT_EQ(TexelAt(coverage, 0, 1), 0x0F | 0x30);
	EXPECT_EQ(TexelAt(coverage, 1, 0), 0x0F | 0x30);
	EXPECT_EQ(CoveredTexels(coverage), 3);
}

TEST(ShapeShadows, ATriangleFacingAwayCastsNothing)
{
	tree_shadows::Coverage coverage {};
	const std::array<shape_shadows::Sample, 3> away {k_Facing[0], k_Facing[2], k_Facing[1]};
	shape_shadows::Fill(coverage, away, false);
	EXPECT_EQ(CoveredTexels(coverage), 0);
}

TEST(ShapeShadows, ATriangleCastFromBothSidesIsTurnedAround)
{
	tree_shadows::Coverage facing {};
	shape_shadows::Fill(facing, k_Facing, true);
	tree_shadows::Coverage away {};
	shape_shadows::Fill(away, {k_Facing[0], k_Facing[2], k_Facing[1]}, true);
	EXPECT_EQ(facing, away);
	EXPECT_EQ(TexelAt(away, 0, 0), 0xFF);
}

TEST(ShapeShadows, RowsAreTakenTowardsZero)
{
	// A corner half a row before the block is taken to row 0, so the triangle starts there and not a row before
	tree_shadows::Coverage coverage {};
	shape_shadows::Fill(coverage, {At(0.0f, -0.5f), At(0.0f, 2.0f), At(4.0f, -0.5f)}, false);
	EXPECT_EQ(TexelAt(coverage, 0, 0), 0x0F | 0x30);
	EXPECT_EQ(CoveredTexels(coverage), 1);
}

TEST(ShapeShadows, StaysInsideTheBlock)
{
	tree_shadows::Coverage coverage {};
	// Reaches from before the block's first row and column to past its last
	shape_shadows::Fill(coverage, {At(-100.0f, -100.0f), At(-100.0f, 2000.0f), At(3000.0f, -100.0f)}, false);
	EXPECT_EQ(TexelAt(coverage, 0, 0), 0xFF);
	EXPECT_EQ(TexelAt(coverage, 0, tree_shadows::k_BlockTexels - 1), 0xFF);
	EXPECT_EQ(TexelAt(coverage, tree_shadows::k_BlockTexels - 1, 0), 0xFF);
}

TEST(ShapeShadows, OverlappingShadowsAreNoDarker)
{
	tree_shadows::Coverage coverage {};
	shape_shadows::Fill(coverage, k_Facing, false);
	shape_shadows::Fill(coverage, k_Facing, false);
	// A fully covered texel is half as bright, once
	EXPECT_EQ(tree_shadows::Shade(TexelAt(coverage, 0, 0)), 128);
}
