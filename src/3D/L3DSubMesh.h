/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <L3DFile.h>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "AxisAlignedBoundingBox.h"

#include "../Graphics/RenderModes.h"
#include "../Graphics/RenderPass.h"

namespace openblack::graphics
{
class L3DMesh;
class Mesh;
class ShaderProgram;

/// The material properties of an object (render_modes)
using MaterialProperties = render_modes::MaterialProperties;

class L3DSubMesh
{
public:
	struct Primitive
	{
		/// the mode's ALPHABLENDENABLE and SRCBLEND / DESTBLEND (render_modes::k_Modes)
		using BlendMode = render_modes::Blend;

		uint32_t skinID;
		uint32_t indicesOffset;
		uint32_t indicesCount;
		bool depthWrite;
		bool alphaTest;
		BlendMode blend;
		bool modulateAlpha;  ///< Multiply ouput alpha by a uniform
		bool thresholdAlpha; ///< Dismiss fragments below a certain threshold
		float alphaCutoutThreshold;
		glm::vec4 colour; ///< material colour (used by untextured primitives)
		bool twoSided;    ///< material flags bit 0: D3DCULL_NONE, else back faces are culled
		bool wrap;        ///< material flags bit 2: D3DTADDRESS_WRAP, else CLAMP
		/// material flags bit 4 clear: the object's texture offset is added to the UVs.
		/// The original's triangle draw skips the materials with the bit (the rock of waterfall3.l3d).
		bool uvOffset;
		/// the L3D material type (l3d::L3DMaterial::Type) = the mode (render_modes::Mode) the fields above come from
		/// (SetMaterialProperties changes it)
		uint32_t materialType;
	};

public:
	explicit L3DSubMesh(graphics::L3DMesh& mesh) noexcept;
	~L3DSubMesh() noexcept;

	/// `vertices`, when not empty, replaces the file's vertices of this sub-mesh (the same count and order: the hand's
	/// good / evil morph)
	bool Load(const l3d::L3DFile& l3d, uint32_t meshIndex, std::span<const l3d::L3DVertex> vertices = {}) noexcept;
	/// The material properties applied to every primitive: a new material type (blending, Z write) and the
	/// double-sided bit
	void SetMaterialProperties(const MaterialProperties& properties) noexcept;
	/// Every primitive whose material type is `from` becomes `to`. Only the type changes: the material flags
	/// (two-sided, wrap, uv offset) and ALPHAREF stay.
	void ReplaceMaterialType(uint32_t from, uint32_t to) noexcept;
	/// Every primitive draws with the skin `skinId`. Only the skin changes: the material type and flags stay.
	void SetSkin(uint32_t skinId) noexcept;

	/// A primitive built at run time (L3DMeshGenerated.cpp): the material of an existing primitive and its triangles.
	struct GeneratedPrimitive
	{
		Primitive material;
		std::vector<glm::vec3> positions;
		std::vector<glm::vec2> uvs;
		std::vector<glm::vec3> normals;
		std::vector<uint16_t> indices; ///< into this primitive's vertices
		/// Lit and drawn on the CPU in the main view instead (FragMesh::AppendDraw): the sub-mesh is kept
		/// for the other views and for picking, and Renderer::DrawSubMesh leaves it out of RenderPass::Main
		bool cpuDrawn {false};
	};
	bool LoadGenerated(const std::vector<GeneratedPrimitive>& primitives) noexcept;

	[[nodiscard]] openblack::l3d::L3DSubmeshHeader::Flags GetFlags() const { return _flags; }
	[[nodiscard]] bool IsPhysics() const { return _flags.isPhysics; }
	/// Generated from primitives that are drawn on the CPU in the main view (GeneratedPrimitive::cpuDrawn)
	[[nodiscard]] bool IsCpuDrawn() const { return _cpuDrawn; }
	[[nodiscard]] graphics::Mesh& GetMesh() const;
	[[nodiscard]] const AxisAlignedBoundingBox& GetBoundingBox() const { return _boundingBox; }
	[[nodiscard]] const std::vector<Primitive>& GetPrimitives() const { return _primitives; }
	/// Bind-pose positions and merged triangle indices, kept on the CPU for ray picking (the triangle collision test).
	[[nodiscard]] const std::vector<glm::vec3>& GetCollisionPositions() const { return _collisionPositions; }
	[[nodiscard]] const std::vector<uint16_t>& GetCollisionIndices() const { return _collisionIndices; }
	/// Texture coordinates of the collision positions (FragMesh copies them)
	[[nodiscard]] const std::vector<glm::vec2>& GetCollisionUVs() const { return _collisionUVs; }
	/// Per collision vertex: the bone of its vertex group (0 without bones, like vs_object's max(0, -1)) and its position
	/// in that bone's space as the file has it, to pose it on the CPU with the bone matrices: the rigid skin of the
	/// original's skinned draw (one bone matrix per vertex group {count, bone})
	[[nodiscard]] const std::vector<uint16_t>& GetSkinBones() const { return _skinBones; }
	[[nodiscard]] const std::vector<glm::vec3>& GetSkinLocalPositions() const { return _skinLocalPositions; }
	/// Each primitive's triangles in GetCollisionIndices (first index, index count), in the order of GetPrimitives
	[[nodiscard]] const std::vector<std::pair<uint32_t, uint32_t>>& GetCollisionRanges() const { return _collisionRanges; }
	/// The file's vertex normal of each collision position (what the inner walls of the partly built draw are pushed
	/// along)
	[[nodiscard]] const std::vector<glm::vec3>& GetCollisionNormals() const { return _collisionNormals; }
	/// Each primitive's vertices in GetCollisionPositions (first vertex, count), in the order of GetPrimitives: the
	/// original keeps one vertex array per primitive (count, vertices)
	[[nodiscard]] const std::vector<std::pair<uint32_t, uint32_t>>& GetCollisionVertexRanges() const
	{
		return _collisionVertexRanges;
	}
	/// The matrix of the table of joints the submesh turns by, about its pivot, when it has one
	struct Joint
	{
		uint32_t index;
		glm::vec3 pivot;
	};
	[[nodiscard]] const std::optional<Joint>& GetJoint() const { return _joint; }
	/// The submesh's name, which the temple's rooms find their scrolls and signs by
	[[nodiscard]] const std::string& GetName() const { return _name; }
	/// The frame of the submesh's name record, from the frame to the mesh, and the submesh's box in it
	struct Frame
	{
		glm::mat4 toMesh;
		glm::vec3 min;
		glm::vec3 max;
	};
	[[nodiscard]] const Frame& GetFrame() const { return _frame; }
	/// The lightmap skin the game multiplies the submesh by, twice over, with the vertices' second texture
	/// coordinates. Submeshes without one are drawn as they are.
	[[nodiscard]] std::optional<uint32_t> GetLightmapSkinID() const { return _lightmapSkinID; }
	/// Whether the vertices carry the lightmap coordinates of the mesh, which all of its submeshes do when any has a
	/// lightmap
	[[nodiscard]] bool HasLightmapCoordinates() const { return _hasLightmapCoordinates; }

private:
	graphics::L3DMesh& _l3dMesh;

	openblack::l3d::L3DSubmeshHeader::Flags _flags;
	bool _cpuDrawn {false};

	std::unique_ptr<graphics::Mesh> _mesh;
	std::vector<Primitive> _primitives;

	AxisAlignedBoundingBox _boundingBox;
	std::vector<glm::vec3> _collisionPositions;
	std::vector<uint16_t> _collisionIndices;
	std::vector<glm::vec2> _collisionUVs;
	std::vector<uint16_t> _skinBones;
	std::vector<glm::vec3> _skinLocalPositions;
	std::vector<std::pair<uint32_t, uint32_t>> _collisionRanges;
	std::vector<glm::vec3> _collisionNormals;
	std::vector<std::pair<uint32_t, uint32_t>> _collisionVertexRanges;
	std::optional<Joint> _joint;
	std::string _name;
	Frame _frame {glm::mat4(1.0f), glm::vec3(0.0f), glm::vec3(0.0f)};
	std::optional<uint32_t> _lightmapSkinID;
	bool _hasLightmapCoordinates {false};
};
} // namespace openblack::graphics
