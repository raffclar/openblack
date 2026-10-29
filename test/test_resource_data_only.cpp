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

#include <L3DFile.h>
#include <bgfx/bgfx.h>
#include <gtest/gtest.h>

#include "3D/L3DMesh.h"
#include "EngineConfig.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/Mesh.h"
#include "Graphics/Texture2D.h"
#include "Visual/UploadMode.h"

using namespace openblack;

namespace
{

l3d::L3DVertex Vertex(float x, float y, float z)
{
	l3d::L3DVertex vertex {};
	vertex.position = {x, y, z};
	vertex.normal = {0.0f, 1.0f, 0.0f};
	return vertex;
}

/// A model of one part with no bones and two primitives of a triangle each. The indices of the second primitive count
/// from its own first vertex.
l3d::L3DFile TwoTriangleModel()
{
	l3d::L3DFile model;
	l3d::L3DSubmeshHeader part {};
	part.flags.lodMask = 1;
	model.AddSubmesh(part);
	l3d::L3DPrimitiveHeader first {};
	first.numVertices = 3;
	first.numTriangles = 1;
	model.AddPrimitives({first, first});
	model.AddVertices({Vertex(0, 0, 0), Vertex(1, 0, 0), Vertex(0, 0, 1), Vertex(0, 5, 0), Vertex(2, 5, 0), Vertex(0, 5, 3)});
	model.AddIndices({0, 1, 2, 2, 1, 0});
	return model;
}

} // namespace

// The test program does not start the graphics library. Thus a load that makes a buffer, a texture or graphics memory
// stops the test.
TEST(ResourceDataOnly, AMeshThatKeepsOnlyItsDataMakesNoObjectOnTheGraphicsDevice)
{
	graphics::L3DMesh mesh("two triangles", false, graphics::UploadMode::DataOnly);
	ASSERT_TRUE(mesh.Load(TwoTriangleModel()));
	EXPECT_EQ(mesh.GetUploadMode(), graphics::UploadMode::DataOnly);
	ASSERT_EQ(mesh.GetSubMeshes().size(), 1U);
	const auto& part = *mesh.GetSubMeshes().front();
	EXPECT_FALSE(part.HasDeviceMesh());
	EXPECT_EQ(part.GetBlendSource(), nullptr);

	// The world reads the same data as from a mesh on the graphics device
	ASSERT_EQ(part.GetPrimitives().size(), 2U);
	EXPECT_EQ(part.GetPrimitives()[1].indicesOffset, 3U);
	const std::vector<uint16_t> indices {0, 1, 2, 5, 4, 3};
	EXPECT_EQ(part.GetBodyGeometry().indices, indices);
	EXPECT_EQ(part.GetBodyGeometry().positions.size(), 6U);
	EXPECT_EQ(part.GetSurface().indices, indices);
	EXPECT_EQ(part.GetSurfacePoints().size(), 6U);
	EXPECT_EQ(mesh.GetBoundingBox().maxima, glm::vec3(2.0f, 5.0f, 3.0f));
	EXPECT_EQ(mesh.GetBoundingBox().minima, glm::vec3(0.0f, 0.0f, 0.0f));
}

TEST(ResourceDataOnly, ATextureThatKeepsOnlyItsDataHasItsSizeAndFormatAndNoDeviceTexture)
{
	graphics::Texture2D texture("described");
	texture.Describe(64, 32, 3, graphics::TextureFormat::RGBA8);
	EXPECT_EQ(texture.GetResolution(), glm::u16vec2(64, 32));
	EXPECT_EQ(texture.GetLayerCount(), 3);
	EXPECT_EQ(texture.GetFormat(), graphics::TextureFormat::RGBA8);
	EXPECT_FALSE(bgfx::isValid(graphics::toBgfx(texture.GetNativeHandle())));
}

TEST(ResourceDataOnly, TheGameMakesTheObjectsOnTheGraphicsDeviceByDefault)
{
	EXPECT_EQ(EngineConfig {}.resourceUploads, graphics::UploadMode::Device);
}
