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

#include <algorithm>
#include <filesystem>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#include <L3DFile.h>
#include <entt/core/fwd.hpp>
#include <gtest/gtest.h>

#include "3D/L3DMesh.h"
#include "Graphics/GraphicsDeviceInterface.h"
#include "Graphics/IndexBuffer.h"
#include "Graphics/VertexBuffer.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/Rereads.h"
#include "Resources/ResourcesInterface.h"
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
			SharedIndexBuffer,
			GivenVertices,
			GivenIndices,
			GivenSharedVertices,
			GivenSharedIndices,
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
	void SetTexelsGivenAgain(TextureHandle) const override {}
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
	void SetContentsGivenAgain(VertexBufferHandle) const override { Mark(Made::Kind::GivenVertices); }
	void GiveContentsAgain(VertexBufferHandle, const DeviceMemory* vertices) const override
	{
		Keep(Made::Kind::GivenVertices, vertices);
	}
	void SetContentsGivenAgain(DynamicVertexBufferHandle) const override { Mark(Made::Kind::GivenSharedVertices); }
	void GiveContentsAgain(DynamicVertexBufferHandle, const DeviceMemory* vertices) const override
	{
		Keep(Made::Kind::GivenSharedVertices, vertices);
	}
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
		Keep(Made::Kind::SharedIndexBuffer, indices);
		return {static_cast<uint32_t>(_made.size())};
	}
	void SetName(IndexBufferHandle, std::string_view) const override {}
	void SetContentsGivenAgain(IndexBufferHandle) const override { Mark(Made::Kind::GivenIndices); }
	void GiveContentsAgain(IndexBufferHandle, const DeviceMemory* indices) const override
	{
		Keep(Made::Kind::GivenIndices, indices);
	}
	void SetContentsGivenAgain(DynamicIndexBufferHandle) const override { Mark(Made::Kind::GivenSharedIndices); }
	void GiveContentsAgain(DynamicIndexBufferHandle, const DeviceMemory* indices) const override
	{
		Keep(Made::Kind::GivenSharedIndices, indices);
	}
	void Destroy(IndexBufferHandle) const override {}
	void Destroy(DynamicIndexBufferHandle) const override {}
	[[nodiscard]] DeviceDataGivers& GetDataGivers() const override { return _givers; }

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
	/// The buffers that the game said it gives again, by the kind of their give
	[[nodiscard]] std::vector<Made::Kind> TakeMarked() const
	{
		const std::scoped_lock lock(_mutex);
		return std::exchange(_marked, {});
	}

private:
	void Count() const
	{
		const std::scoped_lock lock(_mutex);
		++_calls;
	}
	void Mark(Made::Kind kind) const
	{
		const std::scoped_lock lock(_mutex);
		_marked.push_back(kind);
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
	mutable std::vector<Made::Kind> _marked;
	mutable DeviceDataGivers _givers;
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
	// Each texel at 16 bits, as the model with no flags keeps it
	EXPECT_EQ(prepared.skins.front().bytes.size(), 256U * 256U * 2U);
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
	// The vertices and the triangles go in the shared buffers, thus a new mesh gets no new memory from the driver
	EXPECT_EQ(fromLoad[1].kind, FakeDevice::Made::Kind::DynamicVertexBuffer);
	EXPECT_EQ(fromLoad[2].kind, FakeDevice::Made::Kind::SharedIndexBuffer);
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

TEST_F(MeshPrepare, PreparedSkinTexelsGiveTheBitsOfTexelsGivenDirectly)
{
	const auto model = Model();
	std::vector<uint16_t> texels(256U * 256U);
	for (size_t i = 0; i < texels.size(); ++i)
	{
		texels[i] = static_cast<uint16_t>(0xF000U | (i * 13U));
	}

	L3DMesh direct("direct", true, UploadMode::Device);
	ASSERT_TRUE(direct.Load(model));
	static_cast<void>(Device().TakeMade());
	direct.UpdateSkin(7, texels);
	const auto fromDirect = Device().TakeMade();

	L3DMesh other("other", true, UploadMode::Device);
	ASSERT_TRUE(other.Load(model));
	static_cast<void>(Device().TakeMade());
	const auto prepared = direct.PrepareSkin(texels);
	ASSERT_TRUE(other.Fits(prepared));
	other.UpdateSkin(7, prepared);
	const auto fromPrepared = Device().TakeMade();

	ASSERT_EQ(fromDirect.size(), 1U);
	ASSERT_EQ(fromPrepared.size(), 1U);
	EXPECT_EQ(fromPrepared.front().kind, FakeDevice::Made::Kind::TextureUpdate);
	EXPECT_EQ(fromPrepared.front().bytes, fromDirect.front().bytes);
	EXPECT_EQ(*other.GetSkinAlpha(7), *direct.GetSkinAlpha(7));

	// A mesh of data only needs other texels
	L3DMesh dataOnly("data", true, UploadMode::DataOnly);
	ASSERT_TRUE(dataOnly.Load(model));
	EXPECT_FALSE(dataOnly.Fits(prepared));
}

TEST_F(MeshPrepare, TempleMeshesOfOneBlendShareOneSetOfDeviceObjects)
{
	const auto model = Model();
	resources::MeshManager meshes;
	constexpr entt::id_type k_First = 1;
	constexpr entt::id_type k_Second = 2;
	meshes.Load(k_First, resources::L3DLoader::FromPreparedDynamicTag {}, "first", L3DMesh::Prepare(model, k_Device));
	const auto fromFirst = Device().TakeMade();
	// The skin with no texels, the texels, the vertices and the triangles
	ASSERT_EQ(fromFirst.size(), 4U);

	// The second temple takes the mesh of the first and makes no object on the graphics device
	meshes.Load(k_Second, resources::L3DLoader::FromSharedTag {}, meshes.Handle(k_First).handle());
	EXPECT_TRUE(Device().TakeMade().empty());
	ASSERT_NE(meshes.Find(k_Second), nullptr);
	EXPECT_EQ(meshes.Find(k_Second), meshes.Find(k_First));

	// One change of the skin is a change of the skin of the two temples
	std::vector<uint16_t> texels(256U * 256U, 0xF0F0U);
	meshes.Handle(k_First)->UpdateSkin(7, texels);
	EXPECT_EQ(Device().TakeMade().size(), 1U);

	// The mesh stays while one of the temples has it
	meshes.Erase(k_First);
	ASSERT_NE(meshes.Find(k_Second), nullptr);
	EXPECT_EQ(meshes.Find(k_Second)->GetSkinOrder().size(), 1U);
}

TEST_F(MeshPrepare, ASharedVertexBufferUsesTheSharedBufferAndStaysFixed)
{
	const std::vector<uint8_t> bytes(3U * 12U, 0x5AU);
	const VertexDecl decl {VertexAttrib(VertexAttrib::Attribute::Position, 3, VertexAttrib::Type::Float)};

	const VertexBuffer own("own", Device().Copy(bytes.data(), static_cast<uint32_t>(bytes.size())), decl);
	const VertexBuffer shared("shared", Device().Copy(bytes.data(), static_cast<uint32_t>(bytes.size())), decl, false,
	                          VertexBuffer::Storage::Shared);
	const VertexBuffer dynamic("dynamic", Device().Copy(bytes.data(), static_cast<uint32_t>(bytes.size())), decl, true);
	const auto made = Device().TakeMade();

	// A buffer of its own, then two parts of the shared buffer, each with the same bytes
	ASSERT_EQ(made.size(), 3U);
	EXPECT_EQ(made[0].kind, FakeDevice::Made::Kind::VertexBuffer);
	EXPECT_EQ(made[1].kind, FakeDevice::Made::Kind::DynamicVertexBuffer);
	EXPECT_EQ(made[2].kind, FakeDevice::Made::Kind::DynamicVertexBuffer);
	for (const auto& one : made)
	{
		EXPECT_EQ(one.bytes, bytes);
	}
	EXPECT_FALSE(own.IsShared());
	EXPECT_TRUE(shared.IsShared());
	EXPECT_FALSE(shared.IsDynamic());
	EXPECT_TRUE(dynamic.IsShared());
	EXPECT_TRUE(dynamic.IsDynamic());
	EXPECT_TRUE(shared.IsValid());
	EXPECT_EQ(shared.GetCount(), 3U);
}

TEST_F(MeshPrepare, EachKindOfBufferGivesItsContentsAgainToItsOwnBufferOnTheDevice)
{
	const std::vector<uint8_t> bytes(3U * 12U, 0x5AU);
	const std::vector<uint8_t> again(3U * 12U, 0xA5U);
	const VertexDecl decl {VertexAttrib(VertexAttrib::Attribute::Position, 3, VertexAttrib::Type::Float)};
	const auto memory = [&bytes] { return Device().Copy(bytes.data(), static_cast<uint32_t>(bytes.size())); };

	const VertexBuffer own("own", memory(), decl);
	const VertexBuffer shared("shared", memory(), decl, false, VertexBuffer::Storage::Shared);
	const VertexBuffer dynamic("dynamic", memory(), decl, true);
	const IndexBuffer ownIndices("own indices", memory(), IndexBuffer::Type::Uint16);
	const IndexBuffer sharedIndices("shared indices", memory(), IndexBuffer::Type::Uint16, IndexBuffer::Storage::Shared);
	std::ignore = Device().TakeMade();

	// A dynamic buffer is also given again when its owner tells the device so
	for (const auto* buffer : {&own, &shared, &dynamic})
	{
		buffer->SetContentsGivenAgain();
		buffer->GiveContentsAgain(again);
	}
	for (const auto* buffer : {&ownIndices, &sharedIndices})
	{
		buffer->SetContentsGivenAgain();
		buffer->GiveContentsAgain(again);
	}
	using Kind = FakeDevice::Made::Kind;
	const std::vector<Kind> kinds {Kind::GivenVertices, Kind::GivenSharedVertices, Kind::GivenSharedVertices,
	                               Kind::GivenIndices, Kind::GivenSharedIndices};
	EXPECT_EQ(Device().TakeMarked(), kinds);
	const auto given = Device().TakeMade();
	ASSERT_EQ(given.size(), kinds.size());
	for (size_t i = 0; i < given.size(); ++i)
	{
		EXPECT_EQ(given[i].kind, kinds[i]);
		EXPECT_EQ(given[i].bytes, again);
	}
}

TEST_F(MeshPrepare, AMeshTellsTheDeviceWhichBuffersItGivesAgain)
{
	L3DMesh still("still", false, UploadMode::Device);
	ASSERT_TRUE(still.Load(Model()));
	L3DMesh dynamic("dynamic", true, UploadMode::Device);
	ASSERT_TRUE(dynamic.Load(Model()));
	std::ignore = Device().TakeMarked();

	// The vertices of a dynamic mesh change, thus only its triangles are given again
	using Kind = FakeDevice::Made::Kind;
	still.SetFileSource([](resources::Rereads& rereads) { return rereads.File("mesh.l3d"); });
	EXPECT_EQ(Device().TakeMarked(), (std::vector<Kind> {Kind::GivenSharedVertices, Kind::GivenSharedIndices}));
	dynamic.SetFileSource([](resources::Rereads& rereads) { return rereads.File("mesh.l3d"); });
	EXPECT_EQ(Device().TakeMarked(), (std::vector<Kind> {Kind::GivenSharedIndices}));
}

TEST_F(MeshPrepare, AMeshGivesTheBitsOfItsLoadAgainFromItsFile)
{
	const auto model = Model();
	L3DMesh mesh("given again", false, UploadMode::Device);
	ASSERT_TRUE(mesh.Load(model));
	const auto fromLoad = Device().TakeMade();
	ASSERT_EQ(fromLoad.size(), 3U);

	// The skin, the vertices and the triangles again, with the bytes of the load
	mesh.GiveDeviceDataAgain(model);
	const auto given = Device().TakeMade();
	ASSERT_EQ(given.size(), 3U);
	EXPECT_EQ(given[0].kind, FakeDevice::Made::Kind::TextureUpdate);
	EXPECT_EQ(given[1].kind, FakeDevice::Made::Kind::GivenSharedVertices);
	EXPECT_EQ(given[2].kind, FakeDevice::Made::Kind::GivenSharedIndices);
	for (size_t i = 0; i < given.size(); ++i)
	{
		EXPECT_EQ(given[i].bytes, fromLoad[i].bytes);
	}
}

TEST_F(MeshPrepare, AMeshWithAFileSourceIsAGiverOfTheDeviceUntilItIsDeleted)
{
	auto mesh = std::make_unique<L3DMesh>("given again", false, UploadMode::Device);
	ASSERT_TRUE(mesh->Load(Model()));
	std::ignore = Device().TakeMade();
	mesh->SetFileSource([](resources::Rereads& rereads) { return rereads.File("mesh.l3d"); });
	auto& givers = Locator::graphicsDevice::value().GetDataGivers();
	ASSERT_EQ(givers.Count(), 1U);

	// The mesh reads its file through the rereads of the restore. A file that cannot be read gives nothing again.
	resources::Rereads rereads([](const std::filesystem::path& path) {
		EXPECT_EQ(path, std::filesystem::path("mesh.l3d"));
		return std::vector<uint8_t> {};
	});
	givers.GiveAll(rereads);
	EXPECT_EQ(rereads.ReadCount(), 1U);
	EXPECT_TRUE(Device().TakeMade().empty());

	mesh.reset();
	EXPECT_EQ(givers.Count(), 0U);
}

TEST_F(MeshPrepare, ADynamicMeshGivesOnlyItsTrianglesAgain)
{
	const auto model = Model();
	L3DMesh mesh("dynamic", true, UploadMode::Device);
	ASSERT_TRUE(mesh.Load(model));
	const auto fromLoad = Device().TakeMade();

	// The skin and the vertices of a dynamic mesh change, thus the device keeps them
	mesh.GiveDeviceDataAgain(model);
	const auto given = Device().TakeMade();
	ASSERT_EQ(given.size(), 1U);
	EXPECT_EQ(given[0].kind, FakeDevice::Made::Kind::GivenSharedIndices);
	EXPECT_EQ(given[0].bytes, fromLoad.back().bytes);
}

TEST_F(MeshPrepare, ADynamicMeshWithAModelSourceGivesItsShapeAndSkinsAgain)
{
	const auto model = Model();
	L3DMesh mesh("dynamic", true, UploadMode::Device);
	ASSERT_TRUE(mesh.Load(model));
	const auto fromLoad = Device().TakeMade();
	std::ignore = Device().TakeMarked();
	EXPECT_FALSE(mesh.GivesVerticesAgain());

	// With a source, the device keeps no copy of the vertices and the triangles of the dynamic mesh
	uint32_t firstCalls = 0;
	uint32_t secondCalls = 0;
	mesh.SetModelSource([&model, &firstCalls](L3DMesh& given) {
		++firstCalls;
		given.GiveDeviceDataAgain(model);
	});
	EXPECT_TRUE(mesh.GivesVerticesAgain());
	using Kind = FakeDevice::Made::Kind;
	EXPECT_EQ(Device().TakeMarked(), (std::vector<Kind> {Kind::GivenSharedVertices, Kind::GivenSharedIndices}));
	auto& givers = Locator::graphicsDevice::value().GetDataGivers();
	ASSERT_EQ(givers.Count(), 1U);

	// A new source replaces the source and adds no giver
	mesh.SetModelSource([&model, &secondCalls](L3DMesh& given) {
		++secondCalls;
		given.GiveDeviceDataAgain(model);
	});
	EXPECT_TRUE(Device().TakeMarked().empty());
	ASSERT_EQ(givers.Count(), 1U);

	resources::Rereads rereads([](const std::filesystem::path&) { return std::vector<uint8_t> {}; });
	givers.GiveAll(rereads);
	EXPECT_EQ(firstCalls, 0U);
	EXPECT_EQ(secondCalls, 1U);
	const auto bytesOf = [](const std::vector<FakeDevice::Made>& made, Kind kind) {
		const auto found = std::ranges::find(made, kind, &FakeDevice::Made::kind);
		return found != made.end() ? found->bytes : std::vector<uint8_t> {};
	};
	const auto given = Device().TakeMade();
	ASSERT_EQ(given.size(), 3U);
	EXPECT_EQ(given[0].kind, Kind::TextureUpdate);
	EXPECT_EQ(given[1].kind, Kind::GivenSharedVertices);
	EXPECT_EQ(given[2].kind, Kind::GivenSharedIndices);
	EXPECT_EQ(given[0].bytes, bytesOf(fromLoad, Kind::TextureUpdate));
	EXPECT_EQ(given[1].bytes, bytesOf(fromLoad, Kind::DynamicVertexBuffer));
	EXPECT_EQ(given[2].bytes, fromLoad.back().bytes);
}

TEST_F(MeshPrepare, TheFacesForTheSnowOfAMeshThatGivesItsDataAgainAreGivenAgain)
{
	const auto model = Model();
	L3DMesh mesh("snow", false, UploadMode::Device);
	ASSERT_TRUE(mesh.Load(model));
	mesh.SetFileSource([](resources::Rereads& rereads) { return rereads.File("mesh.l3d"); });
	std::ignore = Device().TakeMarked();
	std::ignore = Device().TakeMade();

	// The new faces go in new buffers, which the device also does not keep
	mesh.SplitFacesForSnow();
	const auto split = Device().TakeMade();
	using Kind = FakeDevice::Made::Kind;
	ASSERT_EQ(split.size(), 2U);
	EXPECT_EQ(Device().TakeMarked(), (std::vector<Kind> {Kind::GivenSharedVertices, Kind::GivenSharedIndices}));

	// The faces come again from the surface of the submesh, with the bytes of the split
	mesh.GiveDeviceDataAgain(model);
	const auto given = Device().TakeMade();
	ASSERT_EQ(given.size(), 3U);
	EXPECT_EQ(given[0].kind, Kind::TextureUpdate);
	EXPECT_EQ(given[1].kind, Kind::GivenSharedVertices);
	EXPECT_EQ(given[1].bytes, split[0].bytes);
	EXPECT_EQ(given[2].kind, Kind::GivenSharedIndices);
	EXPECT_EQ(given[2].bytes, split[1].bytes);
}

TEST_F(MeshPrepare, AMeshOfDataOnlyIgnoresAFileSource)
{
	L3DMesh mesh("data only", false, UploadMode::DataOnly);
	ASSERT_TRUE(mesh.Load(Model()));
	mesh.SetFileSource([](resources::Rereads& rereads) { return rereads.File("mesh.l3d"); });
	EXPECT_EQ(Locator::graphicsDevice::value().GetDataGivers().Count(), 0U);
}
