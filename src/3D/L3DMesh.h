/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <filesystem>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include <L3DFile.h>
#include <glm/gtc/quaternion.hpp>

#include "AxisAlignedBoundingBox.h"
#include "FootprintPaint.h"
#include "L3DSkinTexels.h"
#include "L3DSubMesh.h"
#include "ScreenPick.h"
#include "Visual/LightBeams.h"
#include "Visual/TreeShadows.h"
#include "Visual/UploadMode.h"

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
    "Unknown11",
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

struct PreparedL3DMesh;

class L3DMesh
{
public:
	/// A footprint that the model paints into the land under it (see footprint_paint)
	struct Footprint
	{
		/// The picture, ARGB at 4 bits a channel, a row for each texel along v
		int width {0};
		int height {0};
		std::vector<uint16_t> texels;
		/// The triangles that lay the picture on the ground
		std::vector<footprint_paint::Triangle> triangles;
	};
	/// The light a window submesh sheds, drawn with one of the mesh's skins
	struct VolumeLight
	{
		SkinId skinID;
		BeamMesh mesh;
	};
	/// A dynamic mesh's vertices and skins can be changed after it is loaded. A mesh that keeps only its data makes no
	/// buffer or texture on the graphics device.
	explicit L3DMesh(std::string debugName = "", bool dynamic = false, UploadMode upload = UploadMode::Device) noexcept;
	virtual ~L3DMesh() noexcept;

	bool Load(const l3d::L3DFile& l3d) noexcept;
	bool LoadFromFilesystem(const std::filesystem::path& path) noexcept;
	bool LoadFromFile(const std::filesystem::path& path) noexcept;
	bool LoadFromBuffer(const std::vector<uint8_t>& data) noexcept;
	/// A model made while the game runs from the triangles of another, whose skins it is drawn with (which must outlive
	/// it): one submesh for each group of primitives
	bool LoadMade(const L3DMesh& skinSource, std::span<const std::vector<L3DSubMesh::MadePrimitive>> subMeshes) noexcept;
	/// A model of a file drawn with the skins of another (which must outlive it), as they change
	bool LoadWithSkinsOf(const l3d::L3DFile& l3d, const L3DMesh& skinSource) noexcept;
	/// A dynamic mesh takes its vertices afresh from a file of the same shape
	void UpdateVertices(const l3d::L3DFile& l3d) noexcept;
	/// Each submesh gets three corners of its own for each face, for the snow of a tree or a fence
	/// (L3DSubMesh::SplitFacesForSnow). Only the first call changes the mesh. Call it from the main thread before the frame
	/// is drawn.
	void SplitFacesForSnow();
	/// A dynamic mesh's skin takes its texels afresh
	void UpdateSkin(SkinId skin, std::span<const uint16_t> given) noexcept;
	[[nodiscard]] bool IsDynamic() const { return _dynamic; }
	/// The texels along a side of the mesh's skins as the game loads them: half the side at the lowest detail levels
	[[nodiscard]] uint16_t GetSkinSide() const { return _skinSource != nullptr ? _skinSource->GetSkinSide() : _skinSide; }
	/// What the mesh makes: its data and its objects on the graphics device, or its data only
	[[nodiscard]] UploadMode GetUploadMode() const { return _upload; }

	[[nodiscard]] uint8_t GetNumSubMeshes() const { return static_cast<uint8_t>(_subMeshes.size()); }
	[[nodiscard]] const std::vector<std::unique_ptr<L3DSubMesh>>& GetSubMeshes() const { return _subMeshes; }
	[[nodiscard]] const std::unordered_map<SkinId, std::unique_ptr<graphics::Texture2D>>& GetSkins() const
	{
		return _skinSource != nullptr ? _skinSource->GetSkins() : _skins;
	}
	/// The skins' ids in the order the file lists them
	[[nodiscard]] const std::vector<SkinId>& GetSkinOrder() const
	{
		return _skinSource != nullptr ? _skinSource->GetSkinOrder() : _skinOrder;
	}
	/// Where each skin is solid, at 64 by 64 places over it, which the cursor is tested against on a model picked through
	/// its texture's holes
	[[nodiscard]] const screen_pick::AlphaMask* GetSkinMask(SkinId skin) const
	{
		if (_skinSource != nullptr)
		{
			return _skinSource->GetSkinMask(skin);
		}
		const auto found = _skinMasks.find(skin);
		return found != _skinMasks.end() ? &found->second : nullptr;
	}
	/// Each skin's alpha at 64 by 64 places over it, which a tree's shadow on the land is cut out by
	[[nodiscard]] const graphics::tree_shadows::AlphaMask* GetSkinAlpha(SkinId skin) const
	{
		if (_skinSource != nullptr)
		{
			return _skinSource->GetSkinAlpha(skin);
		}
		const auto found = _skinAlpha.find(skin);
		return found != _skinAlpha.end() ? &found->second : nullptr;
	}
	[[nodiscard]] const std::vector<Footprint>& GetFootprints() const { return _footprints; }
	[[nodiscard]] const std::vector<VolumeLight>& GetVolumeLights() const { return _volumeLights; }
	[[nodiscard]] const std::vector<uint32_t>& GetBoneParents() const { return _bonesParents; }
	[[nodiscard]] const std::vector<glm::mat4>& GetBoneMatrices() const { return _bonesDefaultMatrices; }
	[[nodiscard]] const std::optional<glm::vec3>& GetDoorPos() const { return _doorPos; }
	/// The top of the chimney the smoke rises from, in the mesh
	[[nodiscard]] const std::optional<glm::vec3>& GetChimneyPos() const { return _chimneyPos; }
	[[nodiscard]] const std::vector<glm::mat4>& GetExtraMetrics() const { return _extraMetrics; }
	/// A place on the model that the game puts a thing at, by the kind of thing
	struct EffectPoint
	{
		glm::vec3 position;
		uint32_t kind;
	};
	/// The model's effect points, in the model's space, in the order the model keeps them
	[[nodiscard]] const std::vector<EffectPoint>& GetEffectPoints() const { return _effectPoints; }
	/// A point fixed to one of the model's bones, in that bone's space
	struct BonePoint
	{
		uint32_t bone;
		glm::vec3 point;
	};
	/// The first of the points fixed to its bones, when it has any: where a shark's wake comes from
	[[nodiscard]] const std::optional<BonePoint>& GetFirstBonePoint() const { return _firstBonePoint; }
	/// A frame fixed to one of the model's bones, in that bone's space: its axes and its origin
	struct BoneFrame
	{
		uint32_t bone;
		glm::mat4 frame;
	};
	/// The frames fixed to its bones, in the order the model gives them, while each names a bone: the detailed villagers'
	/// eyes, right then left
	[[nodiscard]] const std::vector<BoneFrame>& GetBoneFrames() const { return _boneFrames; }
	/// The triangles of its physics submeshes in the model's space, as the game's physics collides with them
	[[nodiscard]] const std::vector<std::array<glm::vec3, 3>>& GetPhysicsTriangles() const { return _physicsTriangles; }
	/// Every submesh's vertex positions in the model's space and its primitives' triangles, each primitive's indices
	/// counting from its own first vertex, as the game picks a point on a model's surface
	struct SurfacePrimitive
	{
		uint32_t vertexBase;
		uint32_t indexBase;
		uint32_t numTriangles;
	};
	struct Surface
	{
		std::vector<glm::vec3> positions;
		std::vector<uint16_t> indices;
		std::vector<SurfacePrimitive> primitives;
	};
	[[nodiscard]] const std::vector<Surface>& GetSurfaces() const { return _surfaces; }
	[[nodiscard]] AxisAlignedBoundingBox GetBoundingBox() const { return _boundingBox; }
	/// How far along a ray, in the mesh's space, it first meets a submesh drawn of the mesh (as the game picks the
	/// triangle under the mouse while it draws), other than the submeshes left undrawn. Only the temple's rooms keep
	/// their triangles to be picked.
	struct PickHit
	{
		float distance;
		uint32_t subMesh;
	};
	[[nodiscard]] std::optional<PickHit> Pick(glm::vec3 origin, glm::vec3 direction, bool onlyJoints = false,
	                                          bool withoutJoints = false, std::span<const uint32_t> hidden = {}) const;

	/// The settings of this computer that a load reads. The main thread reads them before other threads prepare a load.
	struct LoadSettings
	{
		/// Whether the graphics device keeps textures with 1 bit of alpha and 5 bits for each colour
		bool deviceHas1555 {false};
		/// At the lowest detail levels the skins load at half their size
		bool lowResolution {false};
		UploadMode upload {UploadMode::Device};
	};
	/// The settings of this computer now, for a mesh that makes what the upload mode says. Call it from the main thread.
	[[nodiscard]] static LoadSettings SettingsNow(UploadMode upload);
	/// The work of a load on the processor (see PreparedL3DMesh)
	using Prepared = PreparedL3DMesh;
	/// Does the work of a load on the processor. This function reads only the file and the settings, so any thread can
	/// call it.
	[[nodiscard]] static Prepared Prepare(const l3d::L3DFile& l3d, const LoadSettings& settings);
	/// Keeps a prepared load and makes the skins and buffers on the graphics device, when the mesh makes them. The
	/// prepared load must have the upload mode of the mesh. The result tells whether all the submeshes opened.
	bool Finish(Prepared prepared) noexcept;

private:
	l3d::L3DMeshFlags _flags;
	std::string _debugName;
	bool _dynamic;
	bool _facesSplit {false};
	UploadMode _upload;

	std::unordered_map<SkinId, std::unique_ptr<graphics::Texture2D>> _skins;
	std::vector<SkinId> _skinOrder;
	/// A model made at run time draws with the skins of the model it was made from, which the resource cache keeps for the
	/// level's life, as long as the made model's
	const L3DMesh* _skinSource {nullptr};
	std::unordered_map<SkinId, screen_pick::AlphaMask> _skinMasks;
	std::unordered_map<SkinId, graphics::tree_shadows::AlphaMask> _skinAlpha;
	/// How the skins' texels go into their textures, by the model's flags
	l3d_skin::Layout _skinLayout {l3d_skin::Layout::Argb4444};
	/// Texels along each side of each skin: 256, or 128 at the lowest detail levels
	uint16_t _skinSide {l3d::L3DTexture::k_Width};
	std::vector<Footprint> _footprints; ///< If ContainsLandscapeFeature() is true
	std::optional<BonePoint> _firstBonePoint;
	std::vector<BoneFrame> _boneFrames;
	std::vector<VolumeLight> _volumeLights;
	std::vector<std::unique_ptr<L3DSubMesh>> _subMeshes;
	std::vector<uint32_t> _bonesParents;
	std::vector<glm::mat4> _bonesDefaultMatrices;
	std::optional<glm::vec3> _doorPos;
	std::optional<glm::vec3> _chimneyPos;
	std::vector<glm::mat4> _extraMetrics;
	std::vector<EffectPoint> _effectPoints;
	/// Bounding box if no physics mesh was found
	std::vector<std::array<glm::vec3, 3>> _physicsTriangles;
	std::vector<Surface> _surfaces;
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

/// The work of a load on the processor: all that the mesh keeps of its file, the texels of its skins and the packed
/// vertices of its submeshes. A copy of it gives a mesh of the same bits.
struct PreparedL3DMesh
{
	/// A skin's id and its texels in the layout of the texture, or no texels when the mesh makes no texture
	struct Skin
	{
		SkinId id;
		std::vector<uint8_t> bytes;
	};
	UploadMode upload {UploadMode::Device};
	l3d::L3DMeshFlags flags {};
	std::string nameData;
	l3d_skin::Layout skinLayout {l3d_skin::Layout::Argb4444};
	uint16_t skinSide {l3d::L3DTexture::k_Width};
	/// The skins in the order that the file gives them
	std::vector<Skin> skins;
	std::unordered_map<SkinId, screen_pick::AlphaMask> skinMasks;
	std::unordered_map<SkinId, graphics::tree_shadows::AlphaMask> skinAlpha;
	std::optional<glm::vec3> doorPos;
	std::optional<glm::vec3> chimneyPos;
	std::vector<L3DMesh::Footprint> footprints;
	std::vector<glm::mat4> extraMetrics;
	std::vector<L3DMesh::EffectPoint> effectPoints;
	std::optional<L3DMesh::BonePoint> firstBonePoint;
	std::vector<L3DMesh::BoneFrame> boneFrames;
	std::vector<uint32_t> bonesParents;
	std::vector<glm::mat4> bonesDefaultMatrices;
	/// The submeshes that the mesh keeps, and the submeshes of the file that it cannot open
	std::vector<L3DSubMesh::Prepared> subMeshes;
	std::vector<uint32_t> failedSubMeshes;
	std::vector<L3DMesh::Surface> surfaces;
	std::vector<std::array<glm::vec3, 3>> physicsTriangles;
	AxisAlignedBoundingBox boundingBox {
	    {std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()},
	    {std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()},
	};
	std::vector<L3DMesh::VolumeLight> volumeLights;
};

} // namespace openblack::graphics
