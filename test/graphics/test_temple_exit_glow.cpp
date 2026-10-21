/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Graphics/TempleExitGlow.h"

using namespace openblack::graphics;
namespace glow = openblack::graphics::temple_exit_glow;

// The sheet does not show in the first 2.2 seconds of the walk to the door
TEST(TempleExitGlow, NoneBeforeItsStart)
{
	EXPECT_FALSE(glow::At(0.0f).has_value());
	EXPECT_FALSE(glow::At(2.1f).has_value());
}

// After its start the sheet grows: its grey, its alpha and its far end with the strength
TEST(TempleExitGlow, GrowsAfterItsStart)
{
	const auto sheet = glow::At(3.0f);
	ASSERT_TRUE(sheet.has_value());
	// A strength of about 1.2
	EXPECT_EQ(sheet->colour.r, 152);
	EXPECT_EQ(sheet->colour.g, 152);
	EXPECT_EQ(sheet->colour.b, 152);
	EXPECT_EQ(sheet->colour.a, 90);
	EXPECT_FLOAT_EQ(sheet->nearWidth, 0.0f);
	EXPECT_NEAR(sheet->farWidth, 18.0f, 1e-3f);
	EXPECT_NEAR(sheet->farHeight, 32.0f, 1e-3f);
	EXPECT_FLOAT_EQ(sheet->nearHeight, 20.0f);
}

// At full strength the near end is white and opaque, and it widens
TEST(TempleExitGlow, FullStrength)
{
	const auto sheet = glow::At(10.0f);
	ASSERT_TRUE(sheet.has_value());
	EXPECT_EQ(sheet->colour.r, 255);
	EXPECT_EQ(sheet->colour.a, 255);
	EXPECT_FLOAT_EQ(sheet->nearWidth, 14.0f);
	EXPECT_FLOAT_EQ(sheet->farWidth, 60.0f);
	EXPECT_FLOAT_EQ(sheet->farHeight, 60.0f);
}

// The sheet lies from the way out, a quarter turn round the room, towards the middle of the room
TEST(TempleExitGlow, LiesInLineWithTheWayOut)
{
	const auto sheet = glow::At(4.0f);
	ASSERT_TRUE(sheet.has_value());
	EXPECT_NEAR(sheet->near.x, 0.0f, 1e-3f);
	EXPECT_FLOAT_EQ(sheet->near.y, 5.0f);
	EXPECT_NEAR(sheet->near.z, 80.0f, 1e-3f);
	EXPECT_NEAR(sheet->far.z, 60.0f, 1e-3f);
}

// The sheet has eight strips of four points, coloured at the door and clear at the far end, and 34 triangles
TEST(TempleExitGlow, MeshShape)
{
	const auto sheet = glow::At(4.0f);
	ASSERT_TRUE(sheet.has_value());
	BeamMesh mesh;
	glow::Append(*sheet, 0.0f, mesh);
	ASSERT_EQ(mesh.vertices.size(), 32u);
	EXPECT_EQ(mesh.indices.size(), 102u);
	for (size_t strip = 0; strip < 8; ++strip)
	{
		EXPECT_EQ(mesh.vertices[strip * 4].colour, sheet->colour);
		EXPECT_EQ(mesh.vertices[strip * 4 + 1].colour, sheet->colour);
		EXPECT_EQ(mesh.vertices[strip * 4 + 2].colour.a, 0);
		EXPECT_EQ(mesh.vertices[strip * 4 + 3].colour.a, 0);
		// The top of the near end is 20 above its base
		EXPECT_NEAR(mesh.vertices[strip * 4 + 1].position.y - mesh.vertices[strip * 4].position.y, 20.0f, 1e-4f);
	}
	for (const auto index : mesh.indices)
	{
		EXPECT_LT(index, 32u);
	}
}
