/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Meshes built at run time from triangles (building fragments, FragMesh): one sub-mesh, drawn like a pack mesh.

#include <cfloat>

#include <stdexcept>

#include <bgfx/bgfx.h>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "Graphics/IndexBuffer.h"
#include "Graphics/VertexBuffer.h"
#include "Resources/Loaders.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{
struct GeneratedVertex
{
	glm::vec3 pos;
	glm::vec2 uv;
	glm::vec3 norm;
	glm::i16vec2 index;
};
} // namespace

bool L3DSubMesh::LoadGenerated(const std::vector<GeneratedPrimitive>& primitives) noexcept
{
	_flags = {};
	_flags.lodMask = 1;
	// L3DMesh::LoadGenerated never mixes the two kinds in one sub-mesh
	_cpuDrawn = !primitives.empty() && primitives.front().cpuDrawn;
	uint32_t nVertices = 0;
	uint32_t nIndices = 0;
	for (const auto& p : primitives)
	{
		nVertices += static_cast<uint32_t>(p.positions.size());
		nIndices += static_cast<uint32_t>(p.indices.size());
	}
	if (nVertices == 0 || nIndices == 0 || nVertices > 0xFFFF)
	{
		return false;
	}
	_boundingBox.maxima = glm::vec3(-FLT_MAX);
	_boundingBox.minima = glm::vec3(FLT_MAX);
	const bgfx::Memory* verticesMem = bgfx::alloc(sizeof(GeneratedVertex) * nVertices);
	auto* vertices = reinterpret_cast<GeneratedVertex*>(verticesMem->data);
	const bgfx::Memory* indicesMem = bgfx::alloc(sizeof(uint16_t) * nIndices);
	auto* indices = reinterpret_cast<uint16_t*>(indicesMem->data);
	_collisionPositions.clear();
	_collisionIndices.clear();
	_collisionUVs.clear();
	_collisionRanges.clear();
	_collisionNormals.clear();
	_collisionVertexRanges.clear();
	_primitives.clear();
	uint32_t vertex = 0;
	uint32_t index = 0;
	for (const auto& p : primitives)
	{
		const auto base = vertex;
		for (size_t i = 0; i < p.positions.size(); ++i, ++vertex)
		{
			vertices[vertex] = {p.positions[i], i < p.uvs.size() ? p.uvs[i] : glm::vec2(0.0f),
			                    i < p.normals.size() ? p.normals[i] : glm::vec3(0.0f, 1.0f, 0.0f), glm::i16vec2(-1, -1)};
			_collisionPositions.push_back(p.positions[i]);
			_collisionUVs.push_back(vertices[vertex].uv);
			_collisionNormals.push_back(vertices[vertex].norm);
			_boundingBox.maxima = glm::max(_boundingBox.maxima, p.positions[i]);
			_boundingBox.minima = glm::min(_boundingBox.minima, p.positions[i]);
		}
		_collisionRanges.emplace_back(index, static_cast<uint32_t>(p.indices.size()));
		_collisionVertexRanges.emplace_back(base, static_cast<uint32_t>(p.positions.size()));
		auto material = p.material;
		material.indicesOffset = index;
		material.indicesCount = static_cast<uint32_t>(p.indices.size());
		for (const auto i : p.indices)
		{
			const auto merged = static_cast<uint16_t>(base + i);
			indices[index++] = merged;
			_collisionIndices.push_back(merged);
		}
		_primitives.push_back(material);
	}
	// not boned: every vertex in the object's space
	_skinBones.assign(_collisionPositions.size(), 0);
	_skinLocalPositions = _collisionPositions;
	VertexDecl decl;
	decl.reserve(4);
	decl.emplace_back(VertexAttrib::Attribute::Position, static_cast<uint8_t>(3), VertexAttrib::Type::Float);
	decl.emplace_back(VertexAttrib::Attribute::TexCoord0, static_cast<uint8_t>(2), VertexAttrib::Type::Float);
	decl.emplace_back(VertexAttrib::Attribute::Normal, static_cast<uint8_t>(3), VertexAttrib::Type::Float);
	decl.emplace_back(VertexAttrib::Attribute::Indices, static_cast<uint8_t>(2), VertexAttrib::Type::Int16);
	auto vertexBuffer = std::make_unique<VertexBuffer>(_l3dMesh.GetDebugName(), verticesMem, decl);
	auto indexBuffer = std::make_unique<IndexBuffer>(_l3dMesh.GetDebugName(), indicesMem, IndexBuffer::Type::Uint16);
	_mesh = std::make_unique<graphics::Mesh>(std::move(vertexBuffer), std::move(indexBuffer));
	return true;
}

bool L3DMesh::LoadGenerated(const std::vector<L3DSubMesh::GeneratedPrimitive>& primitives) noexcept
{
	// as many sub-meshes as the 16-bit indices need
	_subMeshes.clear();
	_boundingBox.minima = glm::vec3(FLT_MAX);
	_boundingBox.maxima = glm::vec3(-FLT_MAX);
	std::vector<L3DSubMesh::GeneratedPrimitive> group;
	size_t vertices = 0;
	const auto flush = [&]() {
		if (group.empty())
		{
			return true;
		}
		auto subMesh = std::make_unique<L3DSubMesh>(*this);
		if (!subMesh->LoadGenerated(group))
		{
			return false;
		}
		_boundingBox.minima = glm::min(_boundingBox.minima, subMesh->GetBoundingBox().minima);
		_boundingBox.maxima = glm::max(_boundingBox.maxima, subMesh->GetBoundingBox().maxima);
		_subMeshes.emplace_back(std::move(subMesh));
		group.clear();
		vertices = 0;
		return true;
	};
	for (const auto& p : primitives)
	{
		// a new sub-mesh too where the primitives drawn on the CPU start or stop (L3DSubMesh::IsCpuDrawn)
		if ((vertices + p.positions.size() > 0xFFFF || (!group.empty() && group.back().cpuDrawn != p.cpuDrawn)) && !flush())
		{
			return false;
		}
		vertices += p.positions.size();
		group.push_back(p);
	}
	return flush() && !_subMeshes.empty();
}

void L3DMesh::SetFootprintSource(std::shared_ptr<const L3DMesh> source) noexcept
{
	if (source && source->ContainsLandscapeFeature())
	{
		_flags = static_cast<l3d::L3DMeshFlags>(static_cast<uint32_t>(_flags) |
		                                        static_cast<uint32_t>(l3d::L3DMeshFlags::ContainsLandscapeFeature));
	}
	_footprintSource = std::move(source);
}

resources::L3DLoader::result_type
resources::L3DLoader::operator()(FromGeneratedTag, const std::string& debugName,
                                 const std::vector<L3DSubMesh::GeneratedPrimitive>& primitives) const
{
	auto mesh = std::make_shared<graphics::L3DMesh>(debugName);
	if (!mesh->LoadGenerated(primitives))
	{
		throw std::runtime_error("Unable to generate mesh " + debugName);
	}
	return mesh;
}
