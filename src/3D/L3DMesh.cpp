/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "L3DMesh.h"

#include <cstring>

#include <algorithm>
#include <filesystem>
#include <stdexcept>

#include <BulletCollision/CollisionShapes/btConvexHullShape.h>
#include <L3DFile.h>
#include <bgfx/bgfx.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/component_wise.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <glm/matrix.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DSubMesh.h"
#include "FileSystem/FileSystemInterface.h"
#include "Graphics/IndexBuffer.h"
#include "Graphics/Texture2D.h"
#include "Graphics/VertexBuffer.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::graphics;

L3DMesh::L3DMesh(std::string debugName, bool dynamic) noexcept
    : _flags(static_cast<l3d::L3DMeshFlags>(0))
    , _debugName(std::move(debugName))
    , _dynamic(dynamic)
{
}

L3DMesh::~L3DMesh() noexcept = default;

bool L3DMesh::Load(const l3d::L3DFile& l3d) noexcept
{
	bool result = true;

	_flags = static_cast<l3d::L3DMeshFlags>(l3d.GetHeader().flags);
	_nameData = l3d.GetNameData();
	_firstSkin = l3d.GetSkins().empty() ? std::nullopt : std::optional(l3d.GetSkins().front().id);
	for (const auto& skin : l3d.GetSkins())
	{
		_skins[skin.id] = std::make_unique<Texture2D>(_debugName.c_str());
		const auto skinBytes = static_cast<uint32_t>(skin.texels.size() * sizeof(skin.texels[0]));
		// a texture made with its texels can never take others: a dynamic mesh's is made empty and then filled
		_skins[skin.id]->Create(l3d::L3DTexture::k_Width, l3d::L3DTexture::k_Height, 1, TextureFormat::BGRA4, Wrapping::Repeat,
		                        Filter::Linear, _dynamic ? nullptr : bgfx::copy(skin.texels.data(), skinBytes));
		if (_dynamic)
		{
			_skins[skin.id]->Update(skin.texels.data(), skinBytes);
		}
		// the ARGB4444 alpha nibbles at 64 x 64 for the chroma shadows (the texels as 16-bit words)
		std::vector<uint16_t> words(skin.texels.size());
		std::memcpy(words.data(), skin.texels.data(), words.size() * sizeof(uint16_t));
		_shadowAlphaMaps[skin.id] = shadow_math::MakeAlphaMap(words, l3d::L3DTexture::k_Width, l3d::L3DTexture::k_Height);
	}

	if (HasDoorPosition() && !l3d.GetExtraPoints().empty())
	{
		_doorPos = glm::vec3(l3d.GetExtraPoints()[0].x, l3d.GetExtraPoints()[0].y, l3d.GetExtraPoints()[0].z);
	}

	// with flag 0x400 the chimney is always the second extra point, the first being the door; the original reads it
	// without checking the count
	if (HasChimney() && l3d.GetExtraPoints().size() >= 2)
	{
		_chimneyPos = glm::vec3(l3d.GetExtraPoints()[1].x, l3d.GetExtraPoints()[1].y, l3d.GetExtraPoints()[1].z);
	}

	if (ContainsLandscapeFeature() && l3d.GetFootprint().has_value())
	{
		struct FootprintVertex
		{
			glm::vec2 pos;
			glm::vec2 texCoord;
		};
		VertexDecl decl;
		decl.reserve(1);
		decl.emplace_back(VertexAttrib::Attribute::Position, static_cast<uint8_t>(2), VertexAttrib::Type::Float);
		decl.emplace_back(VertexAttrib::Attribute::TexCoord0, static_cast<uint8_t>(2), VertexAttrib::Type::Float);

		const auto& footprint = *l3d.GetFootprint();

		// TODO (#749) use use std::views::enumerate
		for (uint32_t i = 1; const auto& entry : footprint.entries)
		{
			auto texture = std::make_unique<Texture2D>("footprints/texture/" + _debugName + "/" + std::to_string(i));
			++i;
			texture->Create(
			    static_cast<uint16_t>(footprint.header.width), static_cast<uint16_t>(footprint.header.height), 1,
			    graphics::TextureFormat::BGRA4, Wrapping::ClampEdge, Filter::Linear,
			    bgfx::copy(entry.pixels.data(), static_cast<uint32_t>(entry.pixels.size() * sizeof(entry.pixels[0]))));

			const bgfx::Memory* verticesMem =
			    bgfx::alloc(static_cast<uint32_t>(sizeof(FootprintVertex) * entry.triangles.size() * 3));
			auto* vertices = reinterpret_cast<FootprintVertex*>(verticesMem->data);
			// TODO (#749) Maybe use std::views::enumerate
			for (uint8_t j = 0; const auto& t : entry.triangles)
			{
				for (uint8_t k = 0; k < 3; ++k)
				{
					// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index): access is bound to size
					auto& vertex = vertices[j];
					++j;

					// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index): access is bound to size
					const auto& world = t.world[k];
					// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index): access is bound to size
					const auto& uv = t.texture[k];

					vertex.pos.x = world.x;
					vertex.pos.y = world.y;
					vertex.texCoord.x = uv.x / footprint.header.width;
					vertex.texCoord.y = uv.y / footprint.header.height;
				}
			}

			auto vertexBuffer =
			    std::make_unique<VertexBuffer>("footprints/quad/" + _debugName + "/" + std::to_string(i), verticesMem, decl);
			auto mesh = std::make_unique<Mesh>(std::move(vertexBuffer));
			_footprints.emplace_back(Footprint {std::move(texture), std::move(mesh)});
		}
	}

	if (ContainsExtraMetrics() && !l3d.GetExtraMetrics().empty())
	{
		const auto& extraMetrics = l3d.GetExtraMetrics();
		_extraMetrics.reserve(extraMetrics.size());
		for (const auto& e : extraMetrics)
		{
			_extraMetrics.emplace_back(static_cast<glm::mat4>(glm::make_mat4x3(e.data())));
		}
	}

	if (const auto& eBone = l3d.GetEBone(); eBone.has_value() && eBone->bones[0] >= 0)
	{
		const auto& m = eBone->matrices[0];
		_eBonePoint0 = std::make_pair(static_cast<uint32_t>(eBone->bones[0]), glm::vec3(m[9], m[10], m[11]));
	}
	// the pair (0, 1) always, the pair (2, 3) when bone[2] != -1
	if (const auto& eBone = l3d.GetEBone(); eBone.has_value() && eBone->bones[0] >= 0 && eBone->bones[1] >= 0)
	{
		const size_t count = eBone->bones[2] >= 0 && eBone->bones[3] >= 0 ? 4 : 2;
		for (size_t k = 0; k < count; ++k)
		{
			const auto& m = eBone->matrices[k];
			_blobPoints.emplace_back(static_cast<uint32_t>(eBone->bones[k]), glm::vec3(m[9], m[10], m[11]));
		}
	}

	for (const auto& point : l3d.GetNewEP())
	{
		_newEntrancePoints.emplace_back(point.type, glm::vec3(point.position[0], point.position[1], point.position[2]));
	}

	std::map<uint32_t, glm::mat4> matrices;
	const auto& bones = l3d.GetBones();
	_bonesParents.resize(bones.size());
	for (uint32_t i = 0; i < bones.size(); ++i)
	{
		const auto& bone = bones[i];
		// clang-format off
		auto matrix = glm::mat4(bone.orientation[0], bone.orientation[1], bone.orientation[2], 0.0f,
		                        bone.orientation[3], bone.orientation[4], bone.orientation[5], 0.0f,
		                        bone.orientation[6], bone.orientation[7], bone.orientation[8], 0.0f,
		                        bone.position.x, bone.position.y, bone.position.z, 1.0f);
		// clang-format on
		_bonesLocalMatrices.emplace_back(matrix);
		_bonesParents[i] = bone.parent;
		if (bone.parent != std::numeric_limits<uint32_t>::max())
		{
			matrix = matrices[bone.parent] * matrix;
		}
		_bonesDefaultMatrices.emplace_back(matrix);
		matrices.emplace(i, matrix);
	}

	auto submeshCount = l3d.GetSubmeshHeaders().size();
	for (uint32_t i = 0; i < submeshCount; ++i)
	{
		auto subMesh = std::make_unique<L3DSubMesh>(*this);
		if (!subMesh->Load(l3d, i))
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Failed to open L3DSubMesh");
			result = false;
			continue;
		}
		if (subMesh->GetFlags().isPhysics)
		{
			const auto& verticesSpan = l3d.GetVertexSpan(i);
			auto physicsMesh = std::make_unique<btConvexHullShape>(reinterpret_cast<const btScalar*>(verticesSpan.data()),
			                                                       static_cast<int>(verticesSpan.size()),
			                                                       static_cast<int>(sizeof(verticesSpan[0])));
			physicsMesh->optimizeConvexHull();
			_physicsMesh = std::move(physicsMesh);
			// FIXME(bwrsandman): Some meshes have multiple physics meshes
		}
		const auto& bb = subMesh->GetBoundingBox();
		_boundingBox.minima = glm::min(_boundingBox.minima, bb.minima);
		_boundingBox.maxima = glm::max(_boundingBox.maxima, bb.maxima);

		_subMeshes.emplace_back(std::move(subMesh));
	}

	// The windows of the temple shed volumes of light, with the texture of their first primitive. The temple's windows
	// are each a single primitive.
	const auto& names = l3d.GetSubmeshNames();
	for (uint32_t i = 0; i < names.size() && i < submeshCount; ++i)
	{
		const auto& name = names[i];
		const auto& primitives = l3d.GetPrimitiveSpan(i);
		if ((name.flags & l3d::L3DSubmeshName::VolumeLight) == 0 || primitives.empty())
		{
			continue;
		}
		const auto& primitive = primitives.front();
		const auto vertices = l3d.GetVertexSpan(i).first(std::min<size_t>(primitive.numVertices, l3d.GetVertexSpan(i).size()));
		const auto indices =
		    l3d.GetIndexSpan(i).first(std::min<size_t>(primitive.numTriangles * 3, l3d.GetIndexSpan(i).size()));
		const glm::vec3 source {name.volumeLightSource.x, name.volumeLightSource.y, name.volumeLightSource.z};
		_volumeLights.push_back({
		    .skinID = primitive.material.skinID,
		    .mesh = MakeVolumeLight(vertices, indices, source, name.volumeLightLength),
		});
	}
	// TODO(bwrsandman): if no physics mesh was found, make physics mesh the bounding box

	// TODO(bwrsandman): store vertex and index buffers at mesh level
	// No bgfx::frame() here: the textures and buffers own copies of their data, so the file can go at once and the
	// render thread uploads them with the next frame
	return result;
}

void L3DMesh::RebuildSubMeshes(const l3d::L3DFile& l3d, const std::vector<l3d::L3DVertex>& vertices) noexcept
{
	std::vector<std::unique_ptr<L3DSubMesh>> rebuilt;
	size_t first = 0;
	for (uint32_t i = 0; i < l3d.GetSubmeshHeaders().size(); ++i)
	{
		const auto count = l3d.GetVertexSpan(i).size();
		auto subMesh = std::make_unique<L3DSubMesh>(*this);
		const auto span = first + count <= vertices.size() ? std::span<const l3d::L3DVertex>(vertices.data() + first, count)
		                                                   : std::span<const l3d::L3DVertex>();
		if (!subMesh->Load(l3d, i, span))
		{
			return;
		}
		first += count;
		rebuilt.emplace_back(std::move(subMesh));
	}
	_subMeshes = std::move(rebuilt);
}

void L3DMesh::UpdateSkin(SkinId id, const std::vector<uint16_t>& texels) noexcept
{
	if (const auto skin = _skins.find(id); skin != _skins.end())
	{
		skin->second->Update(texels.data(), static_cast<uint32_t>(texels.size() * sizeof(uint16_t)));
	}
}

void L3DMesh::UpdateBoundingBox() noexcept
{
	_boundingBox = {
	    {std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()},
	    {std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()},
	};
	for (const auto& subMesh : _subMeshes)
	{
		const auto& bb = subMesh->GetBoundingBox();
		_boundingBox.minima = glm::min(_boundingBox.minima, bb.minima);
		_boundingBox.maxima = glm::max(_boundingBox.maxima, bb.maxima);
	}
}

bool L3DMesh::LoadFromFilesystem(const std::filesystem::path& path) noexcept
{
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading L3DMesh from file: {}", path.generic_string());
	l3d::L3DFile l3d;

	try
	{
		l3d.ReadFile(*Locator::filesystem::value().GetData(path));
	}
	catch (std::runtime_error& err)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Failed to open l3d mesh from filesystem {}: {}", path.generic_string(),
		                    err.what());
		return false;
	}

	Load(l3d);
	return true;
}

bool L3DMesh::LoadFromFile(const std::filesystem::path& path) noexcept
{
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading L3DMesh from file: {}", path.generic_string());
	l3d::L3DFile l3d;

	const auto result = l3d.Open(Locator::filesystem::value().FindPath(path));
	if (result != l3d::L3DResult::Success)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Failed to open l3d mesh from filesystem {}: {}", path.generic_string(),
		                    l3d::ResultToStr(result));
		return false;
	}

	if (!Load(l3d))
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "Some issues were seen while loading l3d mesh from from file: {}.",
		                   path.generic_string());
	}

	return true;
}

bool L3DMesh::LoadFromBuffer(const std::vector<uint8_t>& data) noexcept
{
	l3d::L3DFile l3d;

	const auto result = l3d.Open(data);
	if (result != l3d::L3DResult::Success)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Failed to open l3d mesh from buffer: {}", l3d::ResultToStr(result));
		return false;
	}

	if (!Load(l3d))
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "Some issues were seen while loading l3d mesh from buffer.");
	}

	return true;
}

std::optional<float> L3DMesh::RayIntersect(const glm::vec3& origin, const glm::vec3& direction, glm::vec3* faceNormal,
                                           bool skipStatuses) const noexcept
{
	std::optional<float> best;
	for (const auto& subMesh : _subMeshes)
	{
		if (subMesh->IsPhysics() || (skipStatuses && subMesh->GetFlags().status != 0))
		{
			continue;
		}
		const auto& positions = subMesh->GetCollisionPositions();
		const auto& indices = subMesh->GetCollisionIndices();
		for (size_t i = 0; i + 2 < indices.size(); i += 3)
		{
			if (indices[i] >= positions.size() || indices[i + 1] >= positions.size() || indices[i + 2] >= positions.size())
			{
				continue;
			}
			// Moller-Trumbore
			const auto& a = positions[indices[i]];
			const auto e1 = positions[indices[i + 1]] - a;
			const auto e2 = positions[indices[i + 2]] - a;
			const auto p = glm::cross(direction, e2);
			const float det = glm::dot(e1, p);
			if (std::abs(det) < 1e-12f)
			{
				continue;
			}
			const float inv = 1.0f / det;
			const auto s0 = origin - a;
			const float u = glm::dot(s0, p) * inv;
			if (u < 0.0f || u > 1.0f)
			{
				continue;
			}
			const auto q = glm::cross(s0, e1);
			const float v = glm::dot(direction, q) * inv;
			if (v < 0.0f || u + v > 1.0f)
			{
				continue;
			}
			const float t = glm::dot(e2, q) * inv;
			if (t > 0.0f && (!best || t < *best))
			{
				best = t;
				if (faceNormal != nullptr)
				{
					const auto n = glm::cross(e1, e2);
					*faceNormal = n.x != 0.0f || n.y != 0.0f || n.z != 0.0f ? glm::normalize(n) : n;
				}
			}
		}
	}
	return best;
}

std::optional<L3DMesh::PickHit> L3DMesh::Pick(glm::vec3 origin, glm::vec3 direction, bool onlyJoints, bool withoutJoints,
                                              std::span<const uint32_t> hidden) const
{
	std::optional<PickHit> nearest;
	for (uint32_t i = 0; i < _subMeshes.size(); ++i)
	{
		const auto& subMesh = _subMeshes[i];
		// The submeshes drawn: no physics, statuses or low levels of detail. Boned submeshes are never picked so.
		const auto flags = subMesh->GetFlags();
		if (subMesh->IsPhysics() || flags.status != 0 || (flags.lodMask & 1) != 1 || flags.hasBones)
		{
			continue;
		}
		const bool jointed = subMesh->GetJoint().has_value();
		if ((onlyJoints && !jointed) || (withoutJoints && jointed) || std::ranges::find(hidden, i) != hidden.end())
		{
			continue;
		}
		// Past the submesh's box, the ray can't meet it
		const auto& box = subMesh->GetBoundingBox();
		const auto inverse = 1.0f / direction;
		const auto toMinima = (box.minima - origin) * inverse;
		const auto toMaxima = (box.maxima - origin) * inverse;
		if (const float leave = glm::compMin(glm::max(toMinima, toMaxima));
		    leave < 0.0f || glm::compMax(glm::min(toMinima, toMaxima)) > leave)
		{
			continue;
		}
		const auto& positions = subMesh->GetCollisionPositions();
		const auto& indices = subMesh->GetCollisionIndices();
		for (size_t t = 0; t + 2 < indices.size(); t += 3)
		{
			if (indices[t] >= positions.size() || indices[t + 1] >= positions.size() || indices[t + 2] >= positions.size())
			{
				continue;
			}
			// Moller and Trumbore's test, from either side, as the game picks
			const auto& a = positions[indices[t]];
			const auto edge1 = positions[indices[t + 1]] - a;
			const auto edge2 = positions[indices[t + 2]] - a;
			const auto across = glm::cross(direction, edge2);
			const float determinant = glm::dot(edge1, across);
			if (std::abs(determinant) < 1e-12f)
			{
				continue;
			}
			const float inverseDeterminant = 1.0f / determinant;
			const auto fromCorner = origin - a;
			const float u = glm::dot(fromCorner, across) * inverseDeterminant;
			if (u < 0.0f || u > 1.0f)
			{
				continue;
			}
			const auto up = glm::cross(fromCorner, edge1);
			const float v = glm::dot(direction, up) * inverseDeterminant;
			if (v < 0.0f || u + v > 1.0f)
			{
				continue;
			}
			const float distance = glm::dot(edge2, up) * inverseDeterminant;
			if (distance > 0.0f && (!nearest.has_value() || distance < nearest->distance))
			{
				nearest = PickHit {.distance = distance, .subMesh = i};
			}
		}
	}
	return nearest;
}
