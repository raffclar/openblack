/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>
#include <cstring>

#include <memory>
#include <mutex>
#include <span>
#include <thread>
#include <utility>
#include <vector>

#include <L3DFile.h>
#include <gtest/gtest.h>

#include "3D/L3DMesh.h"
#include "Graphics/GraphicsDeviceInterface.h"
#include "Locator.h"
#include "Visual/UploadMode.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{

/// A graphics device that keeps the bytes of each object that it makes, in the order that it makes them
class FakeDevice final: public GraphicsDeviceInterface
{
public:
	struct Made
	{
		enum class Kind : uint8_t
		{
			Texture,
			TextureUpdate,
			VertexBuffer,
			DynamicVertexBuffer,
			IndexBuffer,
		};
		Kind kind;
		std::vector<uint8_t> bytes;
	};

	[[nodiscard]] const DeviceMemory* Copy(const void* data, uint32_t size) const override
	{
		auto* memory = Alloc(size);
		std::memcpy(Bytes(memory).data(), data, size);
		return memory;
	}
	[[nodiscard]] const DeviceMemory* Alloc(uint32_t size) const override
	{
		const std::scoped_lock lock(_mutex);
		++_calls;
		return reinterpret_cast<const DeviceMemory*>(
		    _memories.emplace_back(std::make_unique<std::vector<uint8_t>>(size)).get());
	}
	[[nodiscard]] const DeviceMemory* Reference(const void* data, uint32_t size) const override { return Copy(data, size); }
	[[nodiscard]] std::span<uint8_t> Bytes(const DeviceMemory* memory) const override
	{
		return *reinterpret_cast<std::vector<uint8_t>*>(const_cast<DeviceMemory*>(memory));
	}
	[[nodiscard]] bool HasTextureFormat(TextureFormat, TextureUse) const override
	{
		Count();
		return true;
	}
	[[nodiscard]] uint32_t MaxTextureSize() const override { return 4096; }
	void Frame() const override {}
	[[nodiscard]] TextureHandle CreateTexture2D(uint16_t, uint16_t, uint16_t, TextureFormat, uint64_t,
	                                            const DeviceMemory* texels) const override
	{
		Keep(Made::Kind::Texture, texels);
		return {static_cast<uint32_t>(_made.size())};
	}
	void UpdateTexture2D(TextureHandle, uint16_t, glm::u16vec2, glm::u16vec2, const DeviceMemory* texels) const override
	{
		Keep(Made::Kind::TextureUpdate, texels);
	}
	void ReadTexture(TextureHandle, uint16_t, void*) const override {}
	void SetName(TextureHandle, std::string_view) const override {}
	void Destroy(TextureHandle) const override {}
	[[nodiscard]] VertexBufferHandle CreateVertexBuffer(const DeviceMemory* vertices, const VertexDecl&) const override
	{
		Keep(Made::Kind::VertexBuffer, vertices);
		return {static_cast<uint32_t>(_made.size())};
	}
	[[nodiscard]] DynamicVertexBufferHandle CreateDynamicVertexBuffer(const DeviceMemory* vertices,
	                                                                  const VertexDecl&) const override
	{
		Keep(Made::Kind::DynamicVertexBuffer, vertices);
		return {static_cast<uint32_t>(_made.size())};
	}
	[[nodiscard]] VertexLayoutHandle CreateVertexLayout(const VertexDecl&) const override { return {1}; }
	void Update(DynamicVertexBufferHandle, uint32_t, const DeviceMemory*) const override {}
	void SetName(VertexBufferHandle, std::string_view) const override {}
	void Destroy(VertexBufferHandle) const override {}
	void Destroy(DynamicVertexBufferHandle) const override {}
	void Destroy(VertexLayoutHandle) const override {}
	[[nodiscard]] IndexBufferHandle CreateIndexBuffer(const DeviceMemory* indices, bool) const override
	{
		Keep(Made::Kind::IndexBuffer, indices);
		return {static_cast<uint32_t>(_made.size())};
	}
	[[nodiscard]] DynamicIndexBufferHandle CreateSharedIndexBuffer(const DeviceMemory* indices, bool) const override
	{
		Keep(Made::Kind::IndexBuffer, indices);
		return {static_cast<uint32_t>(_made.size())};
	}
	void SetName(IndexBufferHandle, std::string_view) const override {}
	void Destroy(IndexBufferHandle) const override {}
	void Destroy(DynamicIndexBufferHandle) const override {}

	/// The calls that ask the device for memory, a format or an object
	[[nodiscard]] size_t Calls() const
	{
		const std::scoped_lock lock(_mutex);
		return _calls;
	}
	[[nodiscard]] std::vector<Made> TakeMade() const
	{
		const std::scoped_lock lock(_mutex);
		return std::exchange(_made, {});
	}

private:
	void Count() const
	{
		const std::scoped_lock lock(_mutex);
		++_calls;
	}
	void Keep(Made::Kind kind, const DeviceMemory* memory) const
	{
		std::vector<uint8_t> bytes;
		if (memory != nullptr)
		{
			const auto span = Bytes(memory);
			bytes.assign(span.begin(), span.end());
		}
		const std::scoped_lock lock(_mutex);
		++_calls;
		_made.push_back({.kind = kind, .bytes = std::move(bytes)});
	}

	mutable std::mutex _mutex;
	mutable size_t _calls {0};
	mutable std::vector<std::unique_ptr<std::vector<uint8_t>>> _memories;
	mutable std::vector<Made> _made;
};

/// Puts a fake device in the locator for the life of the test
class MeshPrepare: public ::testing::Test
{
protected:
	void SetUp() override { Locator::graphicsDevice::emplace<FakeDevice>(); }
	void TearDown() override { Locator::graphicsDevice::reset(); }
	[[nodiscard]] static const FakeDevice& Device() { return static_cast<const FakeDevice&>(Locator::graphicsDevice::value()); }
};

l3d::L3DVertex Vertex(float x, float y, float z)
{
	l3d::L3DVertex vertex {};
	vertex.position = {x, y, z};
	vertex.normal = {0.0f, 1.0f, 0.0f};
	vertex.texCoord = {x * 0.5f, z * 0.25f};
	return vertex;
}

/// A model of one part with no bones, two primitives of a triangle each and one skin. The indices of the second
/// primitive count from its own first vertex.
l3d::L3DFile Model()
{
	l3d::L3DFile model;
	l3d::L3DSubmeshHeader part {};
	part.flags.lodMask = 1;
	model.AddSubmesh(part);
	l3d::L3DPrimitiveHeader first {};
	first.numVertices = 3;
	first.numTriangles = 1;
	first.material.skinID = 7;
	model.AddPrimitives({first, first});
	model.AddVertices({Vertex(0, 0, 0), Vertex(1, 0, 0), Vertex(0, 0, 1), Vertex(0, 5, 0), Vertex(2, 5, 0), Vertex(0, 5, 3)});
	model.AddIndices({0, 1, 2, 2, 1, 0});
	auto skin = std::make_unique<l3d::L3DTexture>();
	skin->id = 7;
	for (size_t i = 0; i < skin->texels.size(); ++i)
	{
		const auto value = static_cast<uint16_t>(i * 37U);
		std::memcpy(&skin->texels.at(i), &value, sizeof(value));
	}
	model.AddSkin(*skin);
	return model;
}

constexpr L3DMesh::LoadSettings k_Device {.deviceHas1555 = false, .lowResolution = false, .upload = UploadMode::Device};

} // namespace

TEST_F(MeshPrepare, ThePreparationAsksNothingOfTheGraphicsDevice)
{
	const auto model = Model();
	L3DMesh::Prepared prepared;
	// Another thread does the work on the processor
	std::thread worker([&model, &prepared]() { prepared = L3DMesh::Prepare(model, k_Device); });
	worker.join();
	EXPECT_EQ(Device().Calls(), 0U);

	ASSERT_EQ(prepared.subMeshes.size(), 1U);
	ASSERT_EQ(prepared.skins.size(), 1U);
	EXPECT_EQ(prepared.skins.front().id, 7U);
	// Each texel at 32 bits, for a device with no texture format of 1 bit of alpha
	EXPECT_EQ(prepared.skins.front().bytes.size(), 256U * 256U * 4U);
	// Six vertices of a position, texture coordinates, a normal and four 16-bit indices each
	EXPECT_EQ(prepared.subMeshes.front().vertices.size(), 6U * 40U);
	EXPECT_EQ(prepared.boundingBox.maxima, glm::vec3(2.0f, 5.0f, 3.0f));
}

TEST_F(MeshPrepare, AMeshFinishedFromAPreparationHasTheBitsOfALoadedMesh)
{
	const auto model = Model();
	L3DMesh loaded("loaded", false, UploadMode::Device);
	ASSERT_TRUE(loaded.Load(model));
	const auto fromLoad = Device().TakeMade();

	L3DMesh finished("finished", false, UploadMode::Device);
	ASSERT_TRUE(finished.Finish(L3DMesh::Prepare(model, L3DMesh::SettingsNow(UploadMode::Device))));
	const auto fromFinish = Device().TakeMade();

	// The same objects, with the same bytes, in the same order: the skin, the vertices, then the triangles
	ASSERT_EQ(fromLoad.size(), 3U);
	ASSERT_EQ(fromFinish.size(), fromLoad.size());
	for (size_t i = 0; i < fromLoad.size(); ++i)
	{
		EXPECT_EQ(fromFinish[i].kind, fromLoad[i].kind);
		EXPECT_EQ(fromFinish[i].bytes, fromLoad[i].bytes);
	}
	EXPECT_EQ(fromLoad[0].kind, FakeDevice::Made::Kind::Texture);
	EXPECT_EQ(fromLoad[1].kind, FakeDevice::Made::Kind::VertexBuffer);
	EXPECT_EQ(fromLoad[2].kind, FakeDevice::Made::Kind::IndexBuffer);
	const std::vector<uint16_t> indices {0, 1, 2, 5, 4, 3};
	ASSERT_EQ(fromLoad[2].bytes.size(), indices.size() * sizeof(uint16_t));
	EXPECT_EQ(std::memcmp(fromLoad[2].bytes.data(), indices.data(), fromLoad[2].bytes.size()), 0);

	// And the same data for the world
	ASSERT_EQ(finished.GetSubMeshes().size(), 1U);
	const auto& part = *finished.GetSubMeshes().front();
	EXPECT_TRUE(part.HasDeviceMesh());
	EXPECT_EQ(part.GetBodyGeometry().indices, indices);
	EXPECT_EQ(part.GetBodyGeometry().positions, loaded.GetSubMeshes().front()->GetBodyGeometry().positions);
	EXPECT_EQ(part.GetSurface().indices, indices);
	EXPECT_EQ(part.GetPrimitives().size(), 2U);
	EXPECT_EQ(finished.GetSurfaces().size(), 1U);
	EXPECT_EQ(finished.GetSurfaces().front().positions, loaded.GetSurfaces().front().positions);
	EXPECT_EQ(finished.GetBoundingBox().minima, loaded.GetBoundingBox().minima);
	EXPECT_EQ(finished.GetBoundingBox().maxima, loaded.GetBoundingBox().maxima);
	EXPECT_EQ(finished.GetSkinOrder(), loaded.GetSkinOrder());
	ASSERT_NE(finished.GetSkinAlpha(7), nullptr);
	EXPECT_EQ(*finished.GetSkinAlpha(7), *loaded.GetSkinAlpha(7));
}

TEST_F(MeshPrepare, CopiesOfOnePreparationGiveDynamicMeshesOfTheSameBits)
{
	const auto model = Model();
	L3DMesh loaded("loaded", true, UploadMode::Device);
	ASSERT_TRUE(loaded.Load(model));
	const auto fromLoad = Device().TakeMade();

	const auto prepared = L3DMesh::Prepare(model, k_Device);
	for (int copy = 0; copy < 3; ++copy)
	{
		L3DMesh mesh("copy", true, UploadMode::Device);
		ASSERT_TRUE(mesh.Finish(prepared));
		const auto fromCopy = Device().TakeMade();
		// A dynamic mesh makes its skin with no texels and then gives the texels
		ASSERT_EQ(fromCopy.size(), fromLoad.size());
		for (size_t i = 0; i < fromLoad.size(); ++i)
		{
			EXPECT_EQ(fromCopy[i].kind, fromLoad[i].kind);
			EXPECT_EQ(fromCopy[i].bytes, fromLoad[i].bytes);
		}
	}
	// The preparation stays as it was
	EXPECT_EQ(prepared.subMeshes.front().vertices.size(), 6U * 40U);
}

TEST_F(MeshPrepare, APreparationForAMeshOfDataOnlyHasNoTexelsAndNoPackedVertices)
{
	const auto model = Model();
	const auto prepared =
	    L3DMesh::Prepare(model, {.deviceHas1555 = false, .lowResolution = false, .upload = UploadMode::DataOnly});
	ASSERT_EQ(prepared.skins.size(), 1U);
	EXPECT_TRUE(prepared.skins.front().bytes.empty());
	ASSERT_EQ(prepared.subMeshes.size(), 1U);
	EXPECT_TRUE(prepared.subMeshes.front().vertices.empty());

	L3DMesh mesh("data", false, UploadMode::DataOnly);
	ASSERT_TRUE(mesh.Finish(prepared));
	EXPECT_EQ(Device().Calls(), 0U);
	EXPECT_FALSE(mesh.GetSubMeshes().front()->HasDeviceMesh());
	EXPECT_EQ(mesh.GetSubMeshes().front()->GetSurface().indices.size(), 6U);
}
