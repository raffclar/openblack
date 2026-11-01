/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <gtest/gtest.h>

#include "3D/SnowFaces.h"
#include "Common/ModelInverseSquareRoot.h"

using namespace openblack;

TEST(SnowFaces, TheFaceIsUpByTheSumOfItsCornerNormalsMadeUnitLength)
{
	// Three normals that lean apart sum to one that points up
	EXPECT_NEAR(snow_faces::FaceUp({0.6f, 0.8f, 0.0f}, {-0.6f, 0.8f, 0.0f}, {0.0f, 0.8f, 0.6f}), 0.97014f, 1e-4f);
	EXPECT_NEAR(snow_faces::FaceUp({1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}), 0.0f, 1e-6f);
	EXPECT_NEAR(snow_faces::FaceUp({0.0f, -1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}), -1.0f, 1e-3f);
	// The length comes from the game's own inverse square root, not 1 / sqrt
	const float y = (0.8f + 0.8f) + 0.8f;
	const float z = 0.6f;
	const float length2 = (0.0f + y * y) + z * z;
	EXPECT_EQ(snow_faces::FaceUp({0.6f, 0.8f, 0.0f}, {-0.6f, 0.8f, 0.0f}, {0.0f, 0.8f, 0.6f}),
	          gutils::ModelInverseSquareRoot(length2) * y);
}

TEST(SnowFaces, TheUpPartKeepsItsValueInTwoWholeNumbers)
{
	for (const float up : {-1.0f, -0.70710677f, -0.25f, -0.0001f, 0.0f, 0.000123f, 0.3f, 0.97014f, 1.0f})
	{
		const auto packed = snow_faces::PackUp(up);
		// The low part is 14 bits, never below 0
		EXPECT_GE(packed[0], 0);
		EXPECT_LT(packed[0], 16384);
		EXPECT_NEAR(snow_faces::UnpackUp(packed[0], packed[1]), up, 1e-7f) << up;
	}
	EXPECT_EQ(snow_faces::UnpackUp(snow_faces::PackUp(1.0f)[0], snow_faces::PackUp(1.0f)[1]), 1.0f);
	EXPECT_EQ(snow_faces::UnpackUp(snow_faces::PackUp(-0.5f)[0], snow_faces::PackUp(-0.5f)[1]), -0.5f);
}

TEST(SnowFaces, ACornerReadsTheSnowByTheUpOfItsFaceAndThePartsOfItsTextureCoordinates)
{
	// Across: 0.4 for each unit of the up above -1, and 0.2 of the part of the first coordinate. Down: 0.2 of the part of
	// the second coordinate.
	const auto uv = snow_faces::SnowUv(0.5f, {0.25f, 0.75f});
	EXPECT_FLOAT_EQ(uv.x, 1.5f * 0.4f + 0.2f * 0.25f);
	EXPECT_FLOAT_EQ(uv.y, 0.2f * 0.75f);
	// The part is that of the size, whatever the sign, after the whole number
	const auto wrapped = snow_faces::SnowUv(-1.0f, {-2.25f, 3.5f});
	EXPECT_FLOAT_EQ(wrapped.x, 0.2f * 0.25f);
	EXPECT_FLOAT_EQ(wrapped.y, 0.2f * 0.5f);
	// A face that faces up reads at the right of the texture, where the snow lies thickest
	EXPECT_FLOAT_EQ(snow_faces::SnowUv(1.0f, {0.0f, 0.0f}).x, 0.8f);
}
