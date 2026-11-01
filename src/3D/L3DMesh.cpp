/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "L3DMesh.h"

#include <cassert>

#include <algorithm>
#include <filesystem>
#include <map>
#include <stdexcept>

#include <L3DFile.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <glm/matrix.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DSubMesh.h"
#include "EngineConfig.h"
#include "FileSystem/FileSystemInterface.h"
#include "Graphics/GraphicsDeviceInterface.h"
#include "Graphics/Texture2D.h"
#include "Graphics/VertexBuffer.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{
/// The texture format that keeps texels of this layout
TextureFormat TextureFormatOf(l3d_skin::Layout layout)
{
	switch (layout)
	{
	case l3d_skin::Layout::Argb1555:
		return TextureFormat::BGR5A1;
	case l3d_skin::Layout::Rgba8:
		return TextureFormat::RGBA8;
	case l3d_skin::Layout::Argb4444:
		break;
	}
	return TextureFormat::BGRA4;
}
} // namespace

L3DMesh::L3DMesh(std::string debugName, bool dynamic, UploadMode upload) noexcept
    : _flags(static_cast<l3d::L3DMeshFlags>(0))
    , _debugName(std::move(debugName))
    , _dynamic(dynamic)
    , _upload(upload)
{
}

L3DMesh::~L3DMesh() noexcept = default;

bool L3DMesh::LoadMade(const L3DMesh& skinSource, std::span<const std::vector<L3DSubMesh::MadePrimitive>> subMeshes) noexcept
{
	_skinSource = &skinSource;
	for (const auto& primitives : subMeshes)
	{
		auto subMesh = std::make_unique<L3DSubMesh>(*this);
		if (!subMesh->LoadMade(primitives))
		{
			continue;
		}
		const auto& bb = subMesh->GetBoundingBox();
		_boundingBox.minima = glm::min(_boundingBox.minima, bb.minima);
		_boundingBox.maxima = glm::max(_boundingBox.maxima, bb.maxima);
		_subMeshes.emplace_back(std::move(subMesh));
	}
	return !_subMeshes.empty();
}

L3DMesh::LoadSettings L3DMesh::SettingsNow(UploadMode upload)
{
	return {
	    .deviceHas1555 =
	        Locator::graphicsDevice::has_value() &&
	        Locator::graphicsDevice::value().HasTextureFormat(graphics::TextureFormat::BGR5A1, graphics::TextureUse::Texture2D),
	    .lowResolution = Locator::config::has_value() && Locator::config::value().detail.lowResTextures,
	    .upload = upload,
	};
}

bool L3DMesh::LoadWithSkinsOf(const l3d::L3DFile& l3d, const L3DMesh& skinSource) noexcept
{
	_skinSource = &skinSource;
	return Load(l3d);
}

bool L3DMesh::Load(const l3d::L3DFile& l3d) noexcept
{
	return Finish(Prepare(l3d, SettingsNow(_upload)));
}

L3DMesh::Prepared L3DMesh::Prepare(const l3d::L3DFile& l3d, const LoadSettings& settings)
{
	Prepared prepared;
	prepared.upload = settings.upload;
	prepared.flags = static_cast<l3d::L3DMeshFlags>(l3d.GetHeader().flags);
	const auto flags = static_cast<uint32_t>(prepared.flags);
	const auto has = [&prepared](l3d::L3DMeshFlags flag) { return static_cast<bool>(prepared.flags & flag); };
	prepared.nameData = l3d.GetNameData();
	prepared.skinLayout = l3d_skin::LayoutOf(flags, settings.deviceHas1555);
	prepared.skinSide = l3d_skin::LoadedSide(settings.lowResolution);
	for (const auto& skin : l3d.GetSkins())
	{
		const auto texels =
		    l3d_skin::LoadedTexels(std::span(reinterpret_cast<const uint16_t*>(skin.texels.data()), skin.texels.size()), flags,
		                           settings.lowResolution);
		// An opaque skin is solid at each texel. The masks of the solid texels serve the picks and the tree shadows.
		const auto solid =
		    l3d_skin::HasOpaqueSkins(flags) ? l3d_skin::SolidAlphaTexels(texels.size()) : std::vector<uint16_t> {};
		const auto alpha = solid.empty() ? std::span<const uint16_t>(texels) : std::span<const uint16_t>(solid);
		const auto side = l3d_skin::SideOf(texels.size());
		prepared.skinMasks[skin.id] = screen_pick::MaskOf(alpha, side, side);
		prepared.skinAlpha[skin.id] = tree_shadows::MaskOf(alpha, side, side);
		auto& kept = prepared.skins.emplace_back(PreparedL3DMesh::Skin {.id = skin.id, .bytes = {}});
		if (settings.upload == UploadMode::Device)
		{
			kept.bytes = l3d_skin::TexelsIn(prepared.skinLayout, texels);
		}
	}

	if (has(l3d::L3DMeshFlags::HasDoorPosition) && !l3d.GetExtraPoints().empty())
	{
		prepared.doorPos = glm::vec3(l3d.GetExtraPoints()[0].x, l3d.GetExtraPoints()[0].y, l3d.GetExtraPoints()[0].z);
	}

	// A chimney is always the second extra point, the first being the door
	if (has(l3d::L3DMeshFlags::HasChimney) && l3d.GetExtraPoints().size() >= 2)
	{
		prepared.chimneyPos = glm::vec3(l3d.GetExtraPoints()[1].x, l3d.GetExtraPoints()[1].y, l3d.GetExtraPoints()[1].z);
	}

	if (has(l3d::L3DMeshFlags::ContainsLandscapeFeature) && l3d.GetFootprint().has_value())
	{
		// The land is painted with the footprints on the processor, so their pictures and triangles stay here
		const auto& footprint = *l3d.GetFootprint();
		for (const auto& entry : footprint.entries)
		{
			Footprint painted {
			    .width = static_cast<int>(footprint.header.width),
			    .height = static_cast<int>(footprint.header.height),
			    .texels = entry.pixels,
			    .triangles = {},
			};
			painted.triangles.reserve(entry.triangles.size());
			for (const auto& triangle : entry.triangles)
			{
				footprint_paint::Triangle& made = painted.triangles.emplace_back();
				for (size_t k = 0; k < made.corners.size(); ++k)
				{
					made.corners.at(k) = glm::vec2(triangle.world.at(k).x, triangle.world.at(k).y);
					made.texels.at(k) = glm::vec2(triangle.texture.at(k).x, triangle.texture.at(k).y);
				}
			}
			prepared.footprints.emplace_back(std::move(painted));
		}
	}

	if (has(l3d::L3DMeshFlags::ContainsExtraMetrics) && !l3d.GetExtraMetrics().empty())
	{
		const auto& extraMetrics = l3d.GetExtraMetrics();
		prepared.extraMetrics.reserve(extraMetrics.size());
		for (const auto& e : extraMetrics)
		{
			prepared.extraMetrics.emplace_back(static_cast<glm::mat4>(glm::make_mat4x3(e.data())));
		}
	}

	// Each effect point's matrix is its rotation then its position
	for (const auto& point : l3d.GetNewEffectPoints())
	{
		prepared.effectPoints.push_back(
		    {.position = {point.matrix[9], point.matrix[10], point.matrix[11]}, .kind = point.kind});
	}

	// Each point's matrix is its rotation then its position
	if (const auto& eBone = l3d.GetEBone(); eBone.has_value() && eBone->bones[0] >= 0)
	{
		const auto& matrix = eBone->matrices[0];
		prepared.firstBonePoint =
		    BonePoint {.bone = static_cast<uint32_t>(eBone->bones[0]), .point = glm::vec3(matrix[9], matrix[10], matrix[11])};
	}
	if (const auto& eBone = l3d.GetEBone(); eBone.has_value())
	{
		for (size_t i = 0; i < eBone->bones.size() && eBone->bones.at(i) >= 0; ++i)
		{
			prepared.boneFrames.push_back({.bone = static_cast<uint32_t>(eBone->bones.at(i)),
			                               .frame = static_cast<glm::mat4>(glm::make_mat4x3(eBone->matrices.at(i).data()))});
		}
	}

	std::map<uint32_t, glm::mat4> matrices;
	const auto& bones = l3d.GetBones();
	prepared.bonesParents.resize(bones.size());
	for (uint32_t i = 0; i < bones.size(); ++i)
	{
		const auto& bone = bones[i];
		// clang-format off
		auto matrix = glm::mat4(bone.orientation[0], bone.orientation[1], bone.orientation[2], 0.0f,
		                        bone.orientation[3], bone.orientation[4], bone.orientation[5], 0.0f,
		                        bone.orientation[6], bone.orientation[7], bone.orientation[8], 0.0f,
		                        bone.position.x, bone.position.y, bone.position.z, 1.0f);
		// clang-format on
		prepared.bonesParents[i] = bone.parent;
		if (bone.parent != std::numeric_limits<uint32_t>::max())
		{
			matrix = matrices[bone.parent] * matrix;
		}
		prepared.bonesDefaultMatrices.emplace_back(matrix);
		matrices.emplace(i, matrix);
	}

	const auto submeshCount = l3d.GetSubmeshHeaders().size();
	for (uint32_t i = 0; i < submeshCount; ++i)
	{
		// A submesh can hold nothing at all, like one of the temple library's: there is nothing of it to draw or touch
		if (l3d.GetPrimitiveSpan(i).empty())
		{
			continue;
		}
		auto subMesh = L3DSubMesh::Prepare(l3d, i, settings.upload == UploadMode::Device);
		if (!subMesh.has_value())
		{
			prepared.failedSubMeshes.push_back(i);
			continue;
		}
		{
			// Its surface, as the game picks points on it
			auto& surface = prepared.surfaces.emplace_back();
			const auto vertices = l3d.GetVertexSpan(i);
			surface.positions.reserve(vertices.size());
			for (const auto& vertex : vertices)
			{
				surface.positions.emplace_back(vertex.position.x, vertex.position.y, vertex.position.z);
			}
			const auto indices = l3d.GetIndexSpan(i);
			surface.indices.assign(indices.begin(), indices.end());
			uint32_t vertexBase = 0;
			uint32_t indexBase = 0;
			for (const auto& primitive : l3d.GetPrimitiveSpan(i))
			{
				surface.primitives.push_back(
				    {.vertexBase = vertexBase, .indexBase = indexBase, .numTriangles = primitive.numTriangles});
				vertexBase += primitive.numVertices;
				indexBase += primitive.numTriangles * 3;
			}
		}
		if (subMesh->data.flags.isPhysics)
		{
			const auto& verticesSpan = l3d.GetVertexSpan(i);
			// Its triangles, each primitive's indices counting from its own first vertex
			uint32_t vertexBase = 0;
			uint32_t indexBase = 0;
			const auto indices = l3d.GetIndexSpan(i);
			for (const auto& primitive : l3d.GetPrimitiveSpan(i))
			{
				for (uint32_t t = 0; t < primitive.numTriangles && indexBase + (t * 3) + 2 < indices.size(); ++t)
				{
					std::array<glm::vec3, 3> triangle {};
					for (uint32_t c = 0; c < 3; ++c)
					{
						const auto index = vertexBase + indices[indexBase + (t * 3) + c];
						if (index < verticesSpan.size())
						{
							const auto& p = verticesSpan[index].position;
							triangle.at(c) = glm::vec3(p.x, p.y, p.z);
						}
					}
					prepared.physicsTriangles.push_back(triangle);
				}
				vertexBase += primitive.numVertices;
				indexBase += primitive.numTriangles * 3;
			}
			// FIXME(bwrsandman): Some meshes have multiple physics meshes
		}
		const auto& bb = subMesh->data.boundingBox;
		prepared.boundingBox.minima = glm::min(prepared.boundingBox.minima, bb.minima);
		prepared.boundingBox.maxima = glm::max(prepared.boundingBox.maxima, bb.maxima);

		prepared.subMeshes.emplace_back(std::move(*subMesh));
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
		prepared.volumeLights.push_back({
		    .skinID = primitive.material.skinID,
		    .mesh = MakeVolumeLight(vertices, indices, source, name.volumeLightLength),
		});
	}
	// TODO(bwrsandman): if no physics mesh was found, make physics mesh the bounding box

	// TODO(bwrsandman): store vertex and index buffers at mesh level

	return prepared;
}

bool L3DMesh::Finish(Prepared prepared) noexcept
{
	assert(prepared.upload == _upload);
	_flags = prepared.flags;
	_nameData = std::move(prepared.nameData);
	_skinLayout = prepared.skinLayout;
	_skinSide = prepared.skinSide;
	for (const auto& skin : prepared.skins)
	{
		_skinOrder.push_back(skin.id);
		_skins[skin.id] = std::make_unique<Texture2D>(_debugName.c_str());
		if (_upload == UploadMode::DataOnly)
		{
			_skins[skin.id]->Describe(_skinSide, _skinSide, 1, TextureFormatOf(_skinLayout));
			continue;
		}
		const auto size = static_cast<uint32_t>(skin.bytes.size());
		// bgfx only lets a texture created without texels have them changed
		_skins[skin.id]->Create(_skinSide, _skinSide, 1, TextureFormatOf(_skinLayout), Wrapping::Repeat, Filter::Linear,
		                        _dynamic ? nullptr : Locator::graphicsDevice::value().Copy(skin.bytes.data(), size));
		if (_dynamic)
		{
			_skins[skin.id]->Update(skin.bytes.data(), size);
		}
	}
	_skinMasks = std::move(prepared.skinMasks);
	_skinAlpha = std::move(prepared.skinAlpha);
	_doorPos = prepared.doorPos;
	_chimneyPos = prepared.chimneyPos;
	_footprints = std::move(prepared.footprints);
	_extraMetrics = std::move(prepared.extraMetrics);
	_effectPoints = std::move(prepared.effectPoints);
	_firstBonePoint = prepared.firstBonePoint;
	_boneFrames = std::move(prepared.boneFrames);
	_bonesParents = std::move(prepared.bonesParents);
	_bonesDefaultMatrices = std::move(prepared.bonesDefaultMatrices);
	for (const auto failed : prepared.failedSubMeshes)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Failed to open submesh {} of mesh {}", failed, _debugName);
	}
	_subMeshes.reserve(prepared.subMeshes.size());
	for (auto& subMeshData : prepared.subMeshes)
	{
		auto subMesh = std::make_unique<L3DSubMesh>(*this);
		subMesh->Finish(std::move(subMeshData));
		_subMeshes.emplace_back(std::move(subMesh));
	}
	_surfaces = std::move(prepared.surfaces);
	_physicsTriangles = std::move(prepared.physicsTriangles);
	_boundingBox = prepared.boundingBox;
	_volumeLights = std::move(prepared.volumeLights);
	return prepared.failedSubMeshes.empty();
}

std::optional<L3DMesh::PickHit> L3DMesh::Pick(glm::vec3 origin, glm::vec3 direction, bool onlyJoints, bool withoutJoints,
                                              std::span<const uint32_t> hidden) const
{
	std::optional<PickHit> nearest;
	for (uint32_t i = 0; i < _subMeshes.size(); ++i)
	{
		const auto& subMesh = _subMeshes[i];
		// The submeshes drawn: no physics, statuses or low levels of detail
		if (subMesh->IsPhysics() || subMesh->GetFlags().status != 0 || (subMesh->GetFlags().lodMask & 1) != 1)
		{
			continue;
		}
		if (onlyJoints && !subMesh->GetJoint().has_value())
		{
			continue;
		}
		if ((withoutJoints && subMesh->GetJoint().has_value()) || std::ranges::find(hidden, i) != hidden.end())
		{
			continue;
		}
		if (const auto distance = subMesh->Pick(origin, direction);
		    distance.has_value() && (!nearest.has_value() || *distance < nearest->distance))
		{
			nearest = PickHit {.distance = *distance, .subMesh = i};
		}
	}
	return nearest;
}

void L3DMesh::UpdateVertices(const l3d::L3DFile& l3d) noexcept
{
	assert(_dynamic);
	_boundingBox = {
	    glm::vec3(std::numeric_limits<float>::max()),
	    glm::vec3(std::numeric_limits<float>::lowest()),
	};
	for (const auto& subMesh : _subMeshes)
	{
		subMesh->UpdateVertices(l3d);
		const auto& bb = subMesh->GetBoundingBox();
		_boundingBox.minima = glm::min(_boundingBox.minima, bb.minima);
		_boundingBox.maxima = glm::max(_boundingBox.maxima, bb.maxima);
	}
}

void L3DMesh::SplitFacesForSnow()
{
	if (_facesSplit)
	{
		return;
	}
	_facesSplit = true;
	for (const auto& subMesh : _subMeshes)
	{
		static_cast<void>(subMesh->SplitFacesForSnow());
	}
}

void L3DMesh::UpdateSkin(SkinId skin, std::span<const uint16_t> given) noexcept
{
	UpdateSkin(skin, PrepareSkin(given));
}

L3DMesh::PreparedSkin L3DMesh::PrepareSkin(std::span<const uint16_t> given) const
{
	PreparedSkin prepared {.flags = GetFlags(), .side = _skinSide, .layout = _skinLayout, .upload = _upload};
	// Full-size texels for a skin kept at half size go into it as the mean of each two by two square
	std::vector<uint16_t> halved;
	if (l3d_skin::SideOf(given.size()) != _skinSide)
	{
		halved = l3d_skin::LoadedTexels(given, GetFlags(), true);
	}
	const auto texels = halved.empty() ? given : std::span<const uint16_t>(halved);
	if (_upload == UploadMode::Device)
	{
		prepared.bytes = l3d_skin::TexelsIn(_skinLayout, texels);
	}
	if (l3d_skin::HasOpaqueSkins(GetFlags()))
	{
		prepared.alpha = tree_shadows::MaskOf(l3d_skin::SolidAlphaTexels(texels.size()), _skinSide, _skinSide);
	}
	else
	{
		prepared.alpha = tree_shadows::MaskOf(texels, _skinSide, _skinSide);
	}
	return prepared;
}

bool L3DMesh::Fits(const PreparedSkin& prepared) const
{
	return prepared.flags == GetFlags() && prepared.side == _skinSide && prepared.layout == _skinLayout &&
	       prepared.upload == _upload;
}

void L3DMesh::UpdateSkin(SkinId skin, const PreparedSkin& prepared) noexcept
{
	assert(_dynamic);
	assert(Fits(prepared));
	if (const auto found = _skins.find(skin); found != _skins.end() && _upload == UploadMode::Device)
	{
		found->second->Update(prepared.bytes.data(), static_cast<uint32_t>(prepared.bytes.size()));
	}
	if (const auto found = _skinAlpha.find(skin); found != _skinAlpha.end())
	{
		found->second = prepared.alpha;
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
