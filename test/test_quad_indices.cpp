/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <vector>

#include <gtest/gtest.h>

#include "Creature/CreatureFootprints.h"
#include "Graphics/GroundBlobs.h"
#include "Graphics/QuadIndices.h"

using namespace openblack;
using namespace openblack::graphics;

TEST(QuadIndices, EachQuadTakesThePatternAtItsOwnFourCorners)
{
	const auto indices = QuadIndexList(ground_blobs::k_Indices, 3);
	const std::vector<uint32_t> expected {0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7, 8, 9, 10, 8, 10, 11};
	EXPECT_EQ(indices, expected);
}

TEST(QuadIndices, TheFootprintsKeepTheirOwnPattern)
{
	const auto indices = QuadIndexList(creature_footprints::k_Indices, 2);
	const std::vector<uint32_t> expected {0, 1, 3, 1, 2, 3, 4, 5, 7, 5, 6, 7};
	EXPECT_EQ(indices, expected);
}

TEST(QuadIndices, TheTrianglesAreThoseOfTheSixCornersOfEachQuad)
{
	// A list of six corners for each quad, in the pattern, draws the same triangles as four corners with these indices
	constexpr uint32_t k_Quads = 5;
	std::vector<uint32_t> sixCorners;
	for (uint32_t quad = 0; quad < k_Quads; ++quad)
	{
		for (const auto corner : ground_blobs::k_Indices)
		{
			sixCorners.push_back((quad * 4) + corner);
		}
	}
	EXPECT_EQ(QuadIndexList(ground_blobs::k_Indices, k_Quads), sixCorners);
}

TEST(QuadIndices, NoQuadsGiveNoIndices)
{
	EXPECT_TRUE(QuadIndexList(ground_blobs::k_Indices, 0).empty());
}

TEST(QuadIndices, TheCapacityIsAPowerOfTwoThatHoldsTheQuads)
{
	EXPECT_EQ(QuadCapacity(0), k_LeastQuadCapacity);
	EXPECT_EQ(QuadCapacity(1), k_LeastQuadCapacity);
	EXPECT_EQ(QuadCapacity(k_LeastQuadCapacity), k_LeastQuadCapacity);
	EXPECT_EQ(QuadCapacity(k_LeastQuadCapacity + 1), 2 * k_LeastQuadCapacity);
	EXPECT_EQ(QuadCapacity(5000), 8192u);
}
