/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <filesystem>
#include <limits>
#include <optional>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

#include <L3DFile.h>
#include <glm/gtc/quaternion.hpp>

#include "AxisAlignedBoundingBox.h"
#include "Graphics/LightBeams.h"
#include "Graphics/Mesh.h"
#include "Graphics/ShaderProgram.h"
#include "Graphics/ShadowMath.h"
#include "L3DSubMesh.h"

namespace openblack
{
namespace l3d
{
class L3DFile;
}

constexpr std::array<std::string_view, 32> k_L3DMeshFlagNames {
    "Unknown1",
    "Unknown2",
    "Unknown3",
    "Unknown4",
    "Unknown5",
    "Unknown6",
    "Unknown7",
    "Unknown8",
    "HasBones",
    "Unknown10",
    "HasChimney",
    "HasDoorPosition",
    "Packed",
    "NoDraw",
    "Unknown15",
    "ContainsLandscapeFeature",
    "Unknown17",
    "Unknown18",
    "ContainsUV2",
    "ContainsNameData",
    "ContainsExtraMetrics",
    "ContainsEBone",
    "ContainsTnLData",
    "ContainsNewEP",
    "Unknown25",
    "Unknown26",
    "Unknown27",
    "Unknown28",
    "Unknown29",
    "Unknown30",
    "Unknown31",
    "Unknown32",
};
} // namespace openblack

namespace openblack::graphics
{
class L3DSubMesh;

using SkinId = uint32_t;

// todo: template this
inline l3d::L3DMeshFlags operator&(l3d::L3DMeshFlags a, l3d::L3DMeshFlags b)
{
	return static_cast<l3d::L3DMeshFlags>(static_cast<std::underlying_type<l3d::L3DMeshFlags>::type>(a) &
	                                      static_cast<std::underlying_type<l3d::L3DMeshFlags>::type>(b));
}

class L3DMesh
{
public:
	struct Footprint
	{
		std::unique_ptr<graphics::Texture2D> texture;
		std::unique_ptr<graphics::Mesh> mesh;
	};
	/// The light a window submesh sheds, drawn with one of the mesh's skins
	struct VolumeLight
	{
		SkinId skinID;
		BeamMesh mesh;
	};
	/// A dynamic mesh's skins can take new texels after it is loaded (UpdateSkin); the others' are fixed on the GPU
	explicit L3DMesh(std::string debugName = "", bool dynamic = false) noexcept;
	virtual ~L3DMesh() noexcept;

	bool Load(const l3d::L3DFile& l3d) noexcept;
	bool LoadFromFilesystem(const std::filesystem::path& path) noexcept;
	bool LoadFromFile(const std::filesystem::path& path) noexcept;
	bool LoadFromBuffer(const std::vector<uint8_t>& data) noexcept;
	/// One sub-mesh from run-time triangles (L3DMeshGenerated.cpp)
	bool LoadGenerated(const std::vector<L3DSubMesh::GeneratedPrimitive>& primitives) noexcept;

	/// For a shared mesh whose material properties ask for it: the properties set on every primitive of every sub-mesh
	void SetMaterialProperties(const MaterialProperties& properties) noexcept
	{
		for (auto& subMesh : _subMeshes)
		{
			subMesh->SetMaterialProperties(properties);
		}
	}
	/// Only the physical shield calls it: every sub-mesh (the physics one included), every primitive of material type
	/// `from` becomes type `to`
	void ReplaceMaterialType(uint32_t from, uint32_t to) noexcept
	{
		for (auto& subMesh : _subMeshes)
		{
			subMesh->ReplaceMaterialType(from, to);
		}
	}

	/// Every primitive of every sub-mesh (the physics one included) draws with the skin `id`, which the mesh's skin
	/// source holds when the mesh has none of its own (the worship site wears its temple's skin)
	void SetSkin(SkinId id) noexcept
	{
		for (auto& subMesh : _subMeshes)
		{
			subMesh->SetSkin(id);
		}
	}

	/// The hand's good / evil morph: the sub-meshes built again from `l3d` with `vertices` (all the file's vertices, the
	/// same count and order) instead of the file's; the skins stay. No bgfx::frame() (called from the draw thread)
	void RebuildSubMeshes(const l3d::L3DFile& l3d, const std::vector<l3d::L3DVertex>& vertices) noexcept;
	/// The hand's good / evil morph: the skin `id` gets these ARGB4444 texels (the material is marked dirty)
	void UpdateSkin(SkinId id, const std::vector<uint16_t>& texels) noexcept;
	/// The mesh's box made again from its sub-meshes', after RebuildSubMeshes moved its vertices
	void UpdateBoundingBox() noexcept;
	[[nodiscard]] uint8_t GetNumSubMeshes() const { return static_cast<uint8_t>(_subMeshes.size()); }
	[[nodiscard]] const std::vector<std::unique_ptr<L3DSubMesh>>& GetSubMeshes() const { return _subMeshes; }
	/// The mesh's own skins, or (a generated mesh without any) those of its skin source
	[[nodiscard]] const std::unordered_map<SkinId, std::unique_ptr<graphics::Texture2D>>& GetSkins() const
	{
		return _skins.empty() && _skinSource ? _skinSource->GetSkins() : _skins;
	}
	/// The first skin in the file's order (the skin source's when the mesh has none of its own), or nullopt
	[[nodiscard]] std::optional<SkinId> GetFirstSkin() const
	{
		return _skins.empty() && _skinSource ? _skinSource->GetFirstSkin() : _firstSkin;
	}
	/// The temple's windows' volumes of light, made as the mesh loads
	[[nodiscard]] const std::vector<VolumeLight>& GetVolumeLights() const { return _volumeLights; }
	[[nodiscard]] const std::vector<Footprint>& GetFootprints() const
	{
		return _footprintSource ? _footprintSource->GetFootprints() : _footprints;
	}
	/// A generated mesh (a broken building) keeps the landscape footprint of the mesh it was made from.
	void SetFootprintSource(std::shared_ptr<const L3DMesh> source) noexcept;
	/// A generated mesh made of another one's primitives (the PSys exploded pieces) draws with that mesh's embedded skins
	void SetSkinSource(std::shared_ptr<const L3DMesh> source) noexcept { _skinSource = std::move(source); }
	/// The 64 x 64 shadow map of one of the mesh's own skins (cached with the texture), for the chroma casters' shadow
	/// (graphics::shadow_list); nullptr for a skin that is not embedded
	[[nodiscard]] const graphics::shadow_math::AlphaMap* GetShadowAlphaMap(SkinId skin) const
	{
		if (_shadowAlphaMaps.empty() && _skinSource)
		{
			return _skinSource->GetShadowAlphaMap(skin);
		}
		const auto found = _shadowAlphaMaps.find(skin);
		return found != _shadowAlphaMaps.end() ? &found->second : nullptr;
	}
	[[nodiscard]] const std::vector<uint32_t>& GetBoneParents() const { return _bonesParents; }
	[[nodiscard]] const std::vector<glm::mat4>& GetBoneMatrices() const { return _bonesDefaultMatrices; }
	/// Each bone's rest matrix relative to its parent, as the file has it (L3DBone orientation + position, what the
	/// animation skins without a clip). GetBoneMatrices are these put under their parents with glm
	[[nodiscard]] const std::vector<glm::mat4>& GetBoneLocals() const { return _bonesLocalMatrices; }
	[[nodiscard]] const std::optional<glm::vec3>& GetDoorPos() const { return _doorPos; }
	/// The chimney (flag HasChimney 0x400): extra point [1] in the mesh space
	[[nodiscard]] const std::optional<glm::vec3>& GetChimneyPos() const { return _chimneyPos; }
	[[nodiscard]] const std::vector<glm::mat4>& GetExtraMetrics() const { return _extraMetrics; }
	/// Ground blob points of the EBone block (animals): bone index and position in that bone's space, 2 or 4 of them
	[[nodiscard]] const std::vector<std::pair<uint32_t, glm::vec3>>& GetBlobPoints() const { return _blobPoints; }
	/// The first point of the EBone block (bones[0], matrices[0] position), whatever the others: the shark's wake. Empty
	/// without an EBone block.
	[[nodiscard]] const std::optional<std::pair<uint32_t, glm::vec3>>& GetEBonePoint0() const { return _eBonePoint0; }
	/// The NewEP block (flag ContainsNewEP): each entrance point's ABODE_EPP type and position in the mesh space, in
	/// the block's order (an abode takes the first of a type)
	[[nodiscard]] const std::vector<std::pair<int32_t, glm::vec3>>& GetNewEntrancePoints() const { return _newEntrancePoints; }
	[[nodiscard]] float GetMass() const { return _physicsMass; }
	[[nodiscard]] AxisAlignedBoundingBox GetBoundingBox() const { return _boundingBox; }
	/// Nearest hit of the ray origin + t * direction (mesh space, direction need not be unit) with the triangles of the
	/// drawn (non-physics) sub-meshes, both faces. Returns t; `faceNormal`, when given, the hit triangle's unit normal
	/// (mesh space): norm((V1 - V0) x (V2 - V0)), left as it is when zero. With `skipStatuses` the status sub-meshes
	/// (scaffolds, the temple's shell), which are not drawn, are left out too
	[[nodiscard]] std::optional<float> RayIntersect(const glm::vec3& origin, const glm::vec3& direction,
	                                                glm::vec3* faceNormal = nullptr, bool skipStatuses = false) const noexcept;
	/// How far along a ray, in the mesh's space, it first meets a submesh drawn of the mesh (as the game picks the
	/// triangle under the mouse while it draws), other than the submeshes left undrawn: only the submeshes with joints,
	/// or only those without, and never the hidden ones. Only the temple's rooms are picked so.
	struct PickHit
	{
		float distance;
		uint32_t subMesh;
	};
	[[nodiscard]] std::optional<PickHit> Pick(glm::vec3 origin, glm::vec3 direction, bool onlyJoints = false,
	                                          bool withoutJoints = false, std::span<const uint32_t> hidden = {}) const;

private:
	l3d::L3DMeshFlags _flags;
	std::string _debugName;
	bool _dynamic;

	std::unordered_map<SkinId, std::unique_ptr<graphics::Texture2D>> _skins;
	std::optional<SkinId> _firstSkin;
	std::vector<Footprint> _footprints; ///< If ContainsLandscapeFeature() is true
	std::vector<VolumeLight> _volumeLights;
	std::shared_ptr<const L3DMesh> _footprintSource;
	std::shared_ptr<const L3DMesh> _skinSource;
	std::unordered_map<SkinId, graphics::shadow_math::AlphaMap> _shadowAlphaMaps;
	std::vector<std::unique_ptr<L3DSubMesh>> _subMeshes;
	std::vector<uint32_t> _bonesParents;
	std::vector<glm::mat4> _bonesDefaultMatrices;
	std::vector<glm::mat4> _bonesLocalMatrices;
	std::optional<glm::vec3> _doorPos;
	std::optional<glm::vec3> _chimneyPos;
	std::vector<glm::mat4> _extraMetrics;
	std::vector<std::pair<uint32_t, glm::vec3>> _blobPoints;
	std::optional<std::pair<uint32_t, glm::vec3>> _eBonePoint0;
	std::vector<std::pair<int32_t, glm::vec3>> _newEntrancePoints;
	/// Bounding box if no physics mesh was found
	float _physicsMass {1.0f}; // TODO(bwrsandman): Find somewhere in file a value
	AxisAlignedBoundingBox _boundingBox {
	    {std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()},
	    {std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()},
	};
	std::string _nameData;

public:
	[[nodiscard]] const std::string& GetDebugName() const { return _debugName; }
	[[nodiscard]] const std::string& GetNameData() const { return _nameData; }

	[[nodiscard]] uint32_t GetFlags() const { return static_cast<uint32_t>(_flags); }

	[[nodiscard]] bool IsBoned() const { return static_cast<bool>(_flags & l3d::L3DMeshFlags::HasBones); }
	[[nodiscard]] bool HasDoorPosition() const { return static_cast<bool>(_flags & l3d::L3DMeshFlags::HasDoorPosition); }
	[[nodiscard]] bool HasChimney() const { return static_cast<bool>(_flags & l3d::L3DMeshFlags::HasChimney); }
	[[nodiscard]] bool IsPacked() const { return static_cast<bool>(_flags & l3d::L3DMeshFlags::Packed); }
	[[nodiscard]] bool IsNoDraw() const { return static_cast<bool>(_flags & l3d::L3DMeshFlags::NoDraw); }
	/// The header's 0x200 (L3DMeshFlags::Unknown10): the object copies it when it gets the mesh, and it is the only bit
	/// by which the whole object is queued for z-sorting instead of drawn at once
	[[nodiscard]] bool IsZSorted() const { return static_cast<bool>(_flags & l3d::L3DMeshFlags::Unknown10); }
	[[nodiscard]] bool ContainsLandscapeFeature() const
	{
		return static_cast<bool>(_flags & l3d::L3DMeshFlags::ContainsLandscapeFeature);
	}
	[[nodiscard]] bool IsContainsUV2() const { return static_cast<bool>(_flags & l3d::L3DMeshFlags::ContainsUV2); }
	[[nodiscard]] bool IsContainsNameData() const { return static_cast<bool>(_flags & l3d::L3DMeshFlags::ContainsNameData); }
	[[nodiscard]] bool ContainsExtraMetrics() const
	{
		return static_cast<bool>(_flags & l3d::L3DMeshFlags::ContainsExtraMetrics);
	}
	[[nodiscard]] bool IsContainsEBone() const { return static_cast<bool>(_flags & l3d::L3DMeshFlags::ContainsEBone); }
	[[nodiscard]] bool IsContainsTnLData() const { return static_cast<bool>(_flags & l3d::L3DMeshFlags::ContainsTnLData); }
	[[nodiscard]] bool IsContainsNewEP() const { return static_cast<bool>(_flags & l3d::L3DMeshFlags::ContainsNewEP); }
	// const bool IsContainsNewData() const { return _flags & 0xFC8000; } // ???
};
} // namespace openblack::graphics
