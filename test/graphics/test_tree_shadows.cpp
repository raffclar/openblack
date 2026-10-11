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
#include <vector>

#include <gtest/gtest.h>

#include "Graphics/TreeShadows.h"

namespace tree_shadows = openblack::graphics::tree_shadows;

namespace
{

tree_shadows::AlphaMask SolidMask(uint8_t alpha)
{
	tree_shadows::AlphaMask mask {};
	mask.fill(alpha);
	return mask;
}

tree_shadows::Corner At(float x, float z, glm::vec2 uv = {0.0f, 0.0f})
{
	return {.texel = {x, z}, .uv = uv};
}

int RowCount(const tree_shadows::Coverage& coverage, int x)
{
	int count = 0;
	for (int z = 0; z < tree_shadows::k_BlockTexels; ++z)
	{
		count += coverage[static_cast<size_t>(x * tree_shadows::k_BlockTexels + z)] != 0 ? 1 : 0;
	}
	return count;
}

} // namespace

TEST(TreeShadows, EachAlphaBitDarkensTheLandByHalfAnEighth)
{
	EXPECT_EQ(tree_shadows::Shade(0x00), 255);
	EXPECT_EQ(tree_shadows::Shade(0x10), 240);
	EXPECT_EQ(tree_shadows::Shade(0x30), 224);
	EXPECT_EQ(tree_shadows::Shade(0x70), 208);
	// A fully solid leaf: about a quarter darker
	EXPECT_EQ(tree_shadows::Shade(0xF0), 192);
	// All eight bits, which only a building's shadow sets: half as bright
	EXPECT_EQ(tree_shadows::Shade(0xFF), 128);
}

TEST(TreeShadows, TheMaskReadsTheAlphaAtTheStartOfEachPart)
{
	std::vector<uint16_t> texels(256 * 256, 0x0FFF);
	// Alpha 9 at the first texel of the second part across and the third part down, alpha 5 just after it
	texels[8 * 256 + 4] = 0x9000;
	texels[8 * 256 + 5] = 0x5000;
	const auto mask = tree_shadows::MaskOf(texels, 256, 256);
	EXPECT_EQ(mask[2 * 64 + 1], 0x90);
	EXPECT_EQ(mask[0], 0x00);
	EXPECT_EQ(std::count(mask.begin(), mask.end(), 0x90), 1);
	EXPECT_EQ(std::count(mask.begin(), mask.end(), 0x50), 0);
}

TEST(TreeShadows, AFootStaysWhereItStandsAndAHeightFallsAsFarAlongXAndZ)
{
	const glm::vec2 foot {1000.0f, 2000.0f};
	const glm::vec2 block {960.0f, 1920.0f};
	const auto base = tree_shadows::Flatten({0.0f, 0.0f, 0.0f}, foot, block, {0.0f, 0.0f});
	EXPECT_NEAR(base.texel.x, 40.0f * 1.6f, 1e-3f);
	EXPECT_NEAR(base.texel.y, 80.0f * 1.6f, 1e-3f);
	const auto top = tree_shadows::Flatten({0.0f, 20.0f, 0.0f}, foot, block, {1.0f, 1.0f});
	// The sun is placed from the world's origin, so the shadow falls a little further than the height. Single precision
	// so far from the sun lands on 32nds of a unit, as the game's did.
	EXPECT_NEAR(top.texel.x - base.texel.x, 20.0f * (1.0f + 1000.0f / 500000.0f) * 1.6f, 0.06f);
	EXPECT_NEAR(top.texel.y - base.texel.y, 20.0f * (1.0f + 2000.0f / 500000.0f) * 1.6f, 0.06f);
	EXPECT_FLOAT_EQ(top.uv.x, 63.5f);
	// Below the foot falls straight down
	const auto below = tree_shadows::Flatten({3.0f, -10.0f, 0.0f}, foot, block, {0.0f, 0.0f});
	EXPECT_NEAR(below.texel.x - base.texel.x, 3.0f * 1.6f, 1e-3f);
	EXPECT_NEAR(below.texel.y, base.texel.y, 1e-3f);
}

TEST(TreeShadows, ATriangleFillsEveryTexelOfARowBetweenItsSteppedEdges)
{
	tree_shadows::Coverage coverage {};
	tree_shadows::Fill(coverage, {At(1.5f, 1.2f), At(5.9f, 1.0f), At(1.0f, 5.7f)}, SolidMask(0xF0));
	// The long edge steps down a texel along z only after more than a whole texel's worth along x
	EXPECT_EQ(RowCount(coverage, 0), 0);
	EXPECT_EQ(RowCount(coverage, 1), 5);
	EXPECT_EQ(RowCount(coverage, 2), 5);
	EXPECT_EQ(RowCount(coverage, 3), 4);
	EXPECT_EQ(RowCount(coverage, 4), 3);
	EXPECT_EQ(RowCount(coverage, 5), 2);
	EXPECT_EQ(RowCount(coverage, 6), 0);
	EXPECT_EQ(coverage[2 * 256 + 5], 0xF0);
}

TEST(TreeShadows, TheMaskIsReadAWholePlaceOnFromTheNearest)
{
	tree_shadows::AlphaMask mask {};
	mask[1 * 64 + 1] = 0xA0;
	mask[0] = 0x50;
	tree_shadows::Coverage coverage {};
	const auto corner = At(10.0f, 20.0f, {0.5f, 0.5f});
	tree_shadows::Fill(coverage, {corner, corner, corner}, mask);
	EXPECT_EQ(coverage[10 * 256 + 20], 0xA0);
	// At the far end the place wraps round to the first
	tree_shadows::Coverage far {};
	const auto end = At(10.0f, 20.0f, {63.5f, 63.5f});
	tree_shadows::Fill(far, {end, end, end}, mask);
	EXPECT_EQ(far[10 * 256 + 20], 0x50);
}

TEST(TreeShadows, OverlappingShadowsJoinTheirBitsSoTheLandGetsNoDarker)
{
	tree_shadows::Coverage coverage {};
	const auto triangle = std::array {At(1.0f, 1.0f), At(3.0f, 1.0f), At(1.0f, 3.0f)};
	tree_shadows::Fill(coverage, triangle, SolidMask(0xF0));
	tree_shadows::Fill(coverage, triangle, SolidMask(0xF0));
	EXPECT_EQ(coverage[1 * 256 + 1], 0xF0);
	tree_shadows::Coverage mixed {};
	tree_shadows::Fill(mixed, triangle, SolidMask(0x80));
	tree_shadows::Fill(mixed, triangle, SolidMask(0x70));
	EXPECT_EQ(mixed[1 * 256 + 1], 0xF0);
}

TEST(TreeShadows, NothingOutsideTheBlockIsWritten)
{
	tree_shadows::Coverage coverage {};
	const auto across = std::array {At(-5.0f, 250.0f), At(3.0f, 250.0f), At(-5.0f, 262.0f)};
	EXPECT_TRUE(tree_shadows::Reaches(across));
	tree_shadows::Fill(coverage, across, SolidMask(0xF0));
	EXPECT_EQ(coverage[0 * 256 + 250], 0xF0);
	EXPECT_EQ(coverage[4 * 256 + 250], 0x00);
	const auto outside = std::array {At(-9.0f, 10.0f), At(-3.0f, 12.0f), At(-5.0f, 20.0f)};
	EXPECT_FALSE(tree_shadows::Reaches(outside));
	// A point a fraction before the block is taken down towards nothing, onto its first texel
	EXPECT_TRUE(tree_shadows::Reaches({At(-0.5f, 4.0f), At(-0.5f, 4.0f), At(-0.5f, 4.0f)}));
}

TEST(TreeShadows, ABlockTakesTheTreesFromFourCellsBeforeItToFiveAfter)
{
	EXPECT_TRUE(tree_shadows::TakesShadowFrom({1, 1}, {12, 36}));
	EXPECT_FALSE(tree_shadows::TakesShadowFrom({1, 1}, {11, 20}));
	EXPECT_FALSE(tree_shadows::TakesShadowFrom({1, 1}, {20, 37}));
	for (int cell = 0; cell < 100; ++cell)
	{
		const auto blocks = tree_shadows::BlocksTakingShadowFrom(cell);
		for (int block = -2; block < 8; ++block)
		{
			const bool inside = block >= blocks.x && block <= blocks.y;
			EXPECT_EQ(inside, tree_shadows::TakesShadowFrom({block, 0}, {cell, 0})) << cell << " " << block;
		}
	}
}

TEST(TreeShadows, ACompressedTexturesAlphaIsReadAsTheGameTurnsItInto4Bits)
{
	// One 4 by 4 block of each: DXT1 with its fourth colour a hole, DXT3 with explicit alpha, DXT5 between two values
	std::vector<uint8_t> dxt1 {0x00, 0x00, 0xFF, 0xFF, 0x03, 0x00, 0x00, 0x00};
	const auto holes = tree_shadows::MaskOfCompressed(dxt1, 4, 4, tree_shadows::Compression::Dxt1);
	// Texel (0, 0) uses index 3 (a hole), (1, 0) index 0 (solid); every place reads texel (0, 0) of the 4 by 4 texture
	EXPECT_EQ(holes[0], 0x00);
	std::vector<uint8_t> solid {0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	EXPECT_EQ(tree_shadows::MaskOfCompressed(solid, 4, 4, tree_shadows::Compression::Dxt1)[0], 0xF0);

	std::vector<uint8_t> dxt3(16, 0);
	dxt3[0] = 0x07;
	EXPECT_EQ(tree_shadows::MaskOfCompressed(dxt3, 4, 4, tree_shadows::Compression::Dxt3)[0], 0x70);

	std::vector<uint8_t> dxt5(16, 0);
	dxt5[0] = 255;
	dxt5[1] = 0;
	dxt5[2] = 0x02; // index 2: six sevenths of the first value
	EXPECT_EQ(tree_shadows::MaskOfCompressed(dxt5, 4, 4, tree_shadows::Compression::Dxt5)[0], ((6 * 255 / 7) >> 4) << 4);
}
