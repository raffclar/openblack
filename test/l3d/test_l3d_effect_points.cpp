/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstring>

#include <vector>

#include <L3DFile.h>
#include <gtest/gtest.h>

using namespace openblack::l3d;

namespace
{
/// An effect-point block: its size, its count, then two points
std::vector<uint8_t> MakeBlock(int32_t count)
{
	std::vector<L3DNewEffectPoint> points(2);
	points[0].matrix = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, -4.974f, 0.0f, -11.988f};
	points[0].kind = 1;
	points[1].matrix = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, -8.813f, 0.0f, -9.147f};
	points[1].kind = 2;
	const uint32_t size = 2 * sizeof(uint32_t) + static_cast<uint32_t>(points.size() * sizeof(L3DNewEffectPoint));
	std::vector<uint8_t> block(size);
	std::memcpy(block.data(), &size, sizeof(size));
	std::memcpy(block.data() + sizeof(size), &count, sizeof(count));
	std::memcpy(block.data() + 2 * sizeof(uint32_t), points.data(), points.size() * sizeof(L3DNewEffectPoint));
	return block;
}
} // namespace

TEST(L3DEffectPoints, ReadsEachPointsPlaceAndKind)
{
	std::vector<L3DNewEffectPoint> points;
	ASSERT_TRUE(DecodeNewEffectPoints(MakeBlock(2), points));
	ASSERT_EQ(points.size(), 2);
	EXPECT_FLOAT_EQ(points[0].matrix[9], -4.974f);
	EXPECT_FLOAT_EQ(points[0].matrix[11], -11.988f);
	EXPECT_EQ(points[0].kind, 1u);
	EXPECT_EQ(points[1].kind, 2u);
}

TEST(L3DEffectPoints, RefusesPointsOutsideTheBlock)
{
	std::vector<L3DNewEffectPoint> points;
	EXPECT_FALSE(DecodeNewEffectPoints(MakeBlock(3), points));
	EXPECT_FALSE(DecodeNewEffectPoints(MakeBlock(-1), points));
	EXPECT_FALSE(DecodeNewEffectPoints(std::vector<uint8_t>(4), points));
	EXPECT_TRUE(points.empty());
}
