/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "L3DSubMesh.h"

#include <algorithm>
#include <limits>
#include <vector>

#include <bgfx/bgfx.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/component_wise.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <spdlog/spdlog.h>

#include "Graphics/IndexBuffer.h"
#include "Graphics/ShaderProgram.h"
#include "Graphics/VertexBuffer.h"
#include "L3DMesh.h"

using namespace openblack::graphics;

namespace bgfx
{
// Defined and exported by bgfx but not declared in bgfx.h: frees a Memory that is not handed to bgfx.
void release(const Memory* _mem);
} // namespace bgfx

namespace openblack
{

struct EnhancedL3DVertex
{
	glm::vec3 pos;
	glm::vec2 uv;
	glm::vec3 norm;
	glm::i16vec2 index;
};

/// A vertex of a mesh with lightmaps, with its lightmap coordinates (the third is 0, as the shaders' second texture
/// coordinates have three)
struct LightmappedL3DVertex
{
	EnhancedL3DVertex vertex;
	glm::vec3 lightmapUv;
};
static_assert(sizeof(LightmappedL3DVertex) == sizeof(EnhancedL3DVertex) + sizeof(glm::vec3));

namespace
{
/// The D3D states of the mode of an L3D material type (the type is the index of the original's mode tables:
/// render_modes::k_Modes). 14 and 17 use the functions of modes 5 and 2
void ApplyMode(L3DSubMesh::Primitive& primitive, uint32_t type)
{
	assert(type < render_modes::k_ModeCount);
	const auto& desc = render_modes::Desc(static_cast<render_modes::Mode>(type));
	primitive.materialType = type;
	primitive.depthWrite = desc.zWrite;
	primitive.alphaTest = desc.alphaTest;
	primitive.blend = desc.blend;
	primitive.modulateAlpha = desc.alphaModulate;
	primitive.thresholdAlpha = desc.alphaTest;
}

} // namespace

L3DSubMesh::L3DSubMesh(L3DMesh& mesh) noexcept
    : _l3dMesh(mesh)
{
}

L3DSubMesh::~L3DSubMesh() noexcept = default;

bool L3DSubMesh::Load(const l3d::L3DFile& l3d, uint32_t meshIndex, std::span<const l3d::L3DVertex> vertices) noexcept
{
	const auto& header = l3d.GetSubmeshHeaders()[meshIndex];
	const auto primitiveSpan = l3d.GetPrimitiveSpan(meshIndex);
	const std::span<const l3d::L3DVertex> verticesSpan = vertices.size() == l3d.GetVertexSpan(meshIndex).size()
	                                                         ? vertices
	                                                         : std::span<const l3d::L3DVertex>(l3d.GetVertexSpan(meshIndex));
	const auto& indexSpan = l3d.GetIndexSpan(meshIndex);
	const auto& vertexGroupSpans = l3d.GetVertexGroupSpan(meshIndex);
	const auto& boneSpans = l3d.GetBoneSpan(meshIndex);

	_flags = header.flags;

	// The submesh's record in the name block: its name, its frame and box, and its joint
	if (meshIndex < l3d.GetSubmeshNames().size())
	{
		const auto& name = l3d.GetSubmeshNames()[meshIndex];
		_name.assign(name.name.begin(), std::find(name.name.begin(), name.name.end(), '\0'));
		const auto point = [](const l3d::L3DPoint& p) { return glm::vec3(p.x, p.y, p.z); };
		_frame.toMesh = glm::mat4(glm::vec4(point(name.frameAxes[0]), 0.0f), glm::vec4(point(name.frameAxes[1]), 0.0f),
		                          glm::vec4(point(name.frameAxes[2]), 0.0f), glm::vec4(point(name.frameOrigin), 1.0f));
		_frame.min = point(name.frameMin);
		_frame.max = point(name.frameMax);
		if (name.jointIndex >= 0 && name.jointIndex < 0x100)
		{
			_joint = Joint {
			    .index = static_cast<uint32_t>(name.jointIndex),
			    .pivot = glm::vec3(name.jointPivot.x, name.jointPivot.y, name.jointPivot.z),
			};
		}
	}

	// The UV2 block's lightmap coordinates run over the vertices of every submesh in turn, and it has a lightmap for
	// each submesh, without a skin where there is none
	const auto& lightmapCoordinates = l3d.GetLightmapCoordinates();
	const auto lightmapOffset = static_cast<size_t>(l3d.GetVertexSpan(meshIndex).data() - l3d.GetVertices().data());
	const auto fileVertices = l3d.GetVertexSpan(meshIndex).size();
	_hasLightmapCoordinates = !lightmapCoordinates.empty() && lightmapOffset + fileVertices <= lightmapCoordinates.size();
	if (_hasLightmapCoordinates && meshIndex < l3d.GetLightmaps().size() && l3d.GetLightmaps()[meshIndex].material.skinID != 0)
	{
		_lightmapSkinID = l3d.GetLightmaps()[meshIndex].material.skinID;
	}

	// Count vertices and indices
	uint32_t nVertices = 0;
	uint32_t nIndices = 0;
	for (auto& primitive : primitiveSpan)
	{
		nVertices += primitive.numVertices;
		nIndices += primitive.numTriangles * 3;
	}

	// Construct bounding box
	_boundingBox.maxima = glm::vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	_boundingBox.minima = glm::vec3(FLT_MAX, FLT_MAX, FLT_MAX);
	if (_flags.hasBones)
	{
		for (auto& primitive : primitiveSpan)
		{
			uint32_t vertexOffset = 0;
			for (uint32_t i = 0; i < primitive.numGroups; ++i)
			{
				auto matrix = glm::identity<glm::mat4>();
				for (uint32_t parent = vertexGroupSpans[i].boneIndex; parent != std::numeric_limits<uint32_t>::max();
				     parent = boneSpans[parent].parent)
				{
					const auto& bone = boneSpans[parent];
					const auto orientation = glm::make_mat3(bone.orientation.data());
					const auto translation = glm::make_vec3(&bone.position.x) * orientation;
					const auto local = glm::translate(glm::mat4(orientation), translation);
					matrix = local * matrix;
				}

				for (uint32_t j = 0; j < vertexGroupSpans[i].vertexCount; ++j)
				{
					const auto& vertex = verticesSpan[vertexOffset + j];
					const auto position = glm::xyz(matrix * glm::vec4(glm::make_vec3(&vertex.position.x), 1.0f));
					_boundingBox.maxima = glm::max(_boundingBox.maxima, position);
					_boundingBox.minima = glm::min(_boundingBox.minima, position);
				}
				vertexOffset += vertexGroupSpans[i].vertexCount;
			}
		}
	}
	else
	{
		for (uint32_t i = 0; i < nVertices; i++)
		{
			const auto position = glm::make_vec3(&verticesSpan[i].position.x);
			_boundingBox.maxima = glm::max(_boundingBox.maxima, position);
			_boundingBox.minima = glm::min(_boundingBox.minima, position);
		}
	}

	if (nVertices == 0)
	{
		return false;
	}

	// Get vertices
	// A mesh with lightmaps gathers them here first, and its coordinates are added to them below
	std::vector<EnhancedL3DVertex> lightmappedStaging;
	const bgfx::Memory* verticesMem = nullptr;
	EnhancedL3DVertex* verticesMemAccess = nullptr;
	if (_hasLightmapCoordinates)
	{
		lightmappedStaging.resize(nVertices);
		verticesMemAccess = lightmappedStaging.data();
	}
	else
	{
		verticesMem = bgfx::alloc(sizeof(EnhancedL3DVertex) * nVertices);
		verticesMemAccess = reinterpret_cast<EnhancedL3DVertex*>(verticesMem->data);
	}
	for (uint32_t i = 0; i < nVertices; ++i)
	{
		verticesMemAccess[i].pos = glm::make_vec3(&verticesSpan[i].position.x);
		verticesMemAccess[i].uv = glm::make_vec2(&verticesSpan[i].texCoord.x);
		// TODO(bwrsandman): build normals from mesh
		verticesMemAccess[i].norm = glm::make_vec3(&verticesSpan[i].normal.x);
		verticesMemAccess[i].index.x = -1;
		verticesMemAccess[i].index.y = -1;
	}

	if (nIndices == 0)
	{
		return false;
	}

	// Get Indices
	const bgfx::Memory* indicesMem = bgfx::alloc(sizeof(uint16_t) * nIndices);
	auto* indices = reinterpret_cast<uint16_t*>(indicesMem->data);

	// Fill bone index
	uint32_t vertexIndex = 0;
	for (auto& vertexGroupSpan : vertexGroupSpans)
	{
		for (uint32_t i = 0; i < vertexGroupSpan.vertexCount; ++i)
		{
			verticesMemAccess[vertexIndex].index[0] = vertexGroupSpan.boneIndex;
			verticesMemAccess[vertexIndex].index[1] = -1;
			vertexIndex++;
		}
	}

	_collisionPositions.resize(nVertices);
	_collisionUVs.resize(nVertices);
	_collisionNormals.resize(nVertices);
	_skinBones.assign(nVertices, 0);
	for (uint32_t i = 0; i < nVertices; ++i)
	{
		_collisionPositions[i] = verticesMemAccess[i].pos;
		_collisionUVs[i] = verticesMemAccess[i].uv;
		_collisionNormals[i] = verticesMemAccess[i].norm;
		_skinBones[i] = static_cast<uint16_t>(std::max<int32_t>(0, verticesMemAccess[i].index[0]));
	}
	// before the rest pose below
	_skinLocalPositions = _collisionPositions;

	uint16_t startIndex = 0;
	uint16_t startVertex = 0;
	for (auto& primitive : primitiveSpan)
	{
		// Fix indices for merged vertex buffer
		for (uint32_t j = 0; j < primitive.numTriangles * 3; j++)
		{
			indices[startIndex + j] = indexSpan[startIndex + j] + startVertex;
		}

		assert(static_cast<uint32_t>(primitive.material.type) != 0xe);
		assert(static_cast<uint32_t>(primitive.material.type) != 0x11);
		// the original reads the culling from the material flags bit 0 and the tiling from bit 2
		auto& added = _primitives.emplace_back(Primitive {
		    primitive.material.skinID,
		    startIndex,
		    primitive.numTriangles * 3,
		    false,
		    false,
		    Primitive::BlendMode::Disabled,
		    false,
		    false,
		    primitive.material.alphaCutoutThreshold / 255.0f,
		    glm::vec4(primitive.material.color.bgra.r, primitive.material.color.bgra.g, primitive.material.color.bgra.b,
		              primitive.material.color.bgra.a) /
		        255.0f,
		    (primitive.material.cullMode & 1) != 0,
		    (primitive.material.cullMode & 4) != 0,
		    (primitive.material.cullMode & 0x10) == 0,
		    0,
		});
		ApplyMode(added, static_cast<uint32_t>(primitive.material.type));
		_collisionRanges.emplace_back(startIndex, primitive.numTriangles * 3);
		_collisionVertexRanges.emplace_back(startVertex, primitive.numVertices);

		startVertex += static_cast<uint16_t>(primitive.numVertices);
		startIndex += static_cast<uint16_t>(primitive.numTriangles * 3);
	}

	VertexDecl decl;
	decl.reserve(4);
	decl.emplace_back(VertexAttrib::Attribute::Position, static_cast<uint8_t>(3), VertexAttrib::Type::Float);
	decl.emplace_back(VertexAttrib::Attribute::TexCoord0, static_cast<uint8_t>(2), VertexAttrib::Type::Float);
	decl.emplace_back(VertexAttrib::Attribute::Normal, static_cast<uint8_t>(3), VertexAttrib::Type::Float);
	decl.emplace_back(VertexAttrib::Attribute::Indices, static_cast<uint8_t>(2), VertexAttrib::Type::Int16);

	// build our buffers
	_collisionIndices.assign(indices, indices + nIndices);
	if (_flags.hasBones)
	{
		// Boned meshes (villagers, animals) are drawn in their rest pose: pick them in it, each vertex moved by the chain
		// of its vertex group's bone (the same transform as the bounding box above)
		uint32_t vertex = 0;
		for (const auto& vertexGroupSpan : vertexGroupSpans)
		{
			auto matrix = glm::identity<glm::mat4>();
			for (uint32_t parent = vertexGroupSpan.boneIndex; parent != std::numeric_limits<uint32_t>::max();
			     parent = boneSpans[parent].parent)
			{
				const auto& bone = boneSpans[parent];
				const auto orientation = glm::make_mat3(bone.orientation.data());
				const auto translation = glm::make_vec3(&bone.position.x) * orientation;
				matrix = glm::translate(glm::mat4(orientation), translation) * matrix;
			}
			for (uint32_t j = 0; j < vertexGroupSpan.vertexCount && vertex < nVertices; ++j, ++vertex)
			{
				_collisionPositions[vertex] = glm::xyz(matrix * glm::vec4(_collisionPositions[vertex], 1.0f));
				_collisionNormals[vertex] = glm::mat3(matrix) * _collisionNormals[vertex];
			}
		}
	}
	// The meshes with lightmaps carry each vertex's lightmap coordinates too
	if (_hasLightmapCoordinates)
	{
		verticesMem = bgfx::alloc(sizeof(LightmappedL3DVertex) * nVertices);
		auto* lightmapped = reinterpret_cast<LightmappedL3DVertex*>(verticesMem->data);
		for (uint32_t i = 0; i < nVertices; ++i)
		{
			const auto& uv = lightmapOffset + i < lightmapCoordinates.size() ? lightmapCoordinates[lightmapOffset + i]
			                                                                 : l3d::L3DPoint2D {0.0f, 0.0f};
			lightmapped[i] = {.vertex = lightmappedStaging[i], .lightmapUv = glm::vec3(uv.x, uv.y, 0.0f)};
		}
		decl.emplace_back(VertexAttrib::Attribute::TexCoord1, static_cast<uint8_t>(3), VertexAttrib::Type::Float);
	}
	auto vertexBuffer = std::make_unique<VertexBuffer>(_l3dMesh.GetDebugName(), verticesMem, decl);
	auto indexBuffer = std::make_unique<IndexBuffer>(_l3dMesh.GetDebugName(), indicesMem, IndexBuffer::Type::Uint16);
	_mesh = std::make_unique<graphics::Mesh>(std::move(vertexBuffer), std::move(indexBuffer));

	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "{} submesh {} with {} verts and {} indices", _l3dMesh.GetDebugName(), meshIndex,
	                    nVertices, nIndices);
	return true;
}

void L3DSubMesh::SetMaterialProperties(const MaterialProperties& properties) noexcept
{
	for (auto& primitive : _primitives)
	{
		// the new mode from the old one and the properties
		const auto mode = render_modes::ModeFromProperties(static_cast<render_modes::Mode>(primitive.materialType), properties);
		ApplyMode(primitive, static_cast<uint32_t>(mode));
		primitive.twoSided = properties.doubleSided; // material flags bit 0
	}
}

void L3DSubMesh::ReplaceMaterialType(uint32_t from, uint32_t to) noexcept
{
	// The original writes any value in the type; openblack only knows the 19 modes of the table. Never hit: the only
	// caller (the physical shield) uses (5, 13) and (4, 13)
	if (to >= render_modes::k_ModeCount)
	{
		return;
	}
	for (auto& primitive : _primitives)
	{
		if (primitive.materialType != from)
		{
			continue;
		}
		// the D3D states of the new type's mode (the type picks the mode in the tables); twoSided, wrap, uvOffset and
		// alphaCutoutThreshold come from the other material bytes, which are not touched
		ApplyMode(primitive, to);
	}
}

void L3DSubMesh::SetSkin(uint32_t skinId) noexcept
{
	for (auto& primitive : _primitives)
	{
		primitive.skinID = skinId;
	}
}

Mesh& L3DSubMesh::GetMesh() const
{
	return *_mesh;
}

} // namespace openblack
