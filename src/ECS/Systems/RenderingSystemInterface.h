/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <map>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <entt/fwd.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/LandLight.h"
#include "Graphics/GraphicsHandle.h"
#include "Graphics/Mesh.h"
#include "Graphics/RenderModes.h"

namespace openblack::psys
{
struct Atom;
enum class DrawPath : uint8_t;
} // namespace openblack::psys

namespace openblack::ecs::systems
{
struct RenderContext
{
	RenderContext();
	~RenderContext();
	std::unique_ptr<graphics::Mesh> boundingBox;
	std::unique_ptr<graphics::Mesh> streams;
	std::unique_ptr<graphics::Mesh> footpaths;
	std::unique_ptr<graphics::Mesh> footprints;

	struct InstancedDrawDesc
	{
		InstancedDrawDesc(uint32_t offset, uint32_t count, bool morphWithTerrain)
		    : offset(offset)
		    , count(count)
		    , morphWithTerrain(morphWithTerrain)
		{
		}
		uint32_t offset;
		uint32_t count;
		bool morphWithTerrain;

		// The temple's: the world's draws leave them as they are made here and never read them
		/// The instances aren't seen in the reflection of the temple's main room
		bool hiddenFromReflection {false};
		/// The instances show the reflection through them, as the temple's floor does
		bool showsReflection {false};
		/// Only the submeshes with joints are drawn, as the game draws the main room's doors
		bool onlyJoints {false};
		/// The submeshes with joints are drawn only while their joints turn: the side rooms' copies of their doors
		bool hideShutJoints {false};
		/// The instances are of a temple room the player isn't in, which gives way where it overlaps the room they are in
		bool behindCurrentRoom {false};
		/// The instances are drawn as each primitive's material says: blended and writing depth or not
		bool materialBlending {false};
		/// The instances are all blended by their materials, so they are drawn after the opaque ones
		bool translucent {false};
		/// How far the instances' textures have slid across them
		glm::vec2 uvOffset {0.0f};
		/// Textures some of the submeshes are drawn with in place of their skins, by submesh: the temple's scrolls
		std::vector<std::pair<uint32_t, graphics::TextureHandle>> subMeshTextures;
		/// Colours added to some of the submeshes, by submesh: the temple's controls glowing under the cursor
		std::vector<std::pair<uint32_t, glm::vec3>> subMeshGlows;
		/// Submeshes left undrawn: the temple's buttons draw one of each of their pairs
		std::vector<uint32_t> hiddenSubMeshes;
	};

	/// A list of cpu-side uniforms which is refilled at every \ref PrepareDraw.
	/// This vector will resize to the number of instances it manages
	/// but in practice, it should only grow its reserved memory.
	/// If debug bounding boxes are enabled, it will double in size to fit all
	/// bounding boxes in the second half of the list.
	std::vector<glm::mat4> instanceUniforms;
	/// The fifth column of every instance (i_data4), at the same indices: the object's tint, colour and specular fields
	/// packed by argb_colour::PackInstanceTint / Colour / Specular / Window (src/Graphics/ArgbColour.h), zero for the land
	/// light alone. Zeroed before every refill; instanceUniformBuffer holds the two interleaved (80 bytes each).
	std::vector<glm::vec4> instanceColours;
	/// Stores information for rendering which is prepared at \ref PrepareDraw.
	std::map<entt::id_type, const InstancedDrawDesc> instancedDrawDescs;
	/// The entities with components::NotDrawn (a building at 0 %): not drawn, only their landscape footprint
	/// (Renderer::DrawFootprintPass; the footprint stays on while it is unbuilt)
	std::map<entt::id_type, const InstancedDrawDesc> footprintOnlyDrawDescs;
	/// Same for entities with a components::Alpha (drawn blended after the opaque ones). Their opacity travels in the
	/// unused w of the first column of the model matrix, as 1 - alpha so that opaque instances keep 0 there.
	std::map<entt::id_type, const InstancedDrawDesc> translucentDrawDescs;
	/// (openblack) the opaque entities with components::NeedsSorting (the held object): their own range, queued in the
	/// Z-sorter like a mesh with the sort flag (Renderer::DrawPass)
	std::map<entt::id_type, const InstancedDrawDesc> sortedOpaqueDrawDescs;
	/// Every instance of an entity with components::NeedsSorting, fading ones too: queued whatever its mesh
	std::unordered_set<uint32_t> sortedInstances;
	/// The instances (indices of instanceUniforms) of those that are PSys mesh atoms with UseAdditiveAlpha: material mode
	/// 13 (SRCALPHA / ONE, no Z write), Particles/Creators/Mesh.h
	std::unordered_set<uint32_t> additiveInstances;
	/// The opaque PSys mesh atoms drawn cut by a plane (a particle flag): out of instancedDrawDescs, drawn in
	/// Renderer::DrawPass (the cut-atom loop after DrawCutAboveWater) with graphics::sea_pass::CutAtoms
	std::map<entt::id_type, const InstancedDrawDesc> cutAtomDrawDescs;
	/// The translucent ones (in translucentDrawDescs): their instance indices, drawn cut from the sorted list
	std::unordered_set<uint32_t> cutAtomInstances;
	/// Every PSys mesh atom of this frame (psys::mesh_atoms::Instance) with its effect's draw path (psys::DrawPath), in
	/// mesh_atoms::Collect's order, refilled at every PrepareDraw. Its instance is in the old ranges (instancedDrawDescs,
	/// translucentDrawDescs, cutAtomDrawDescs) while psys::manager::k_DrawByPath is false, and in psysAtomDrawDescs only
	/// once it is true. Sorted: its own Z object at `key` (opaque, translucent or cut alike); Queued / Immediate: drawn at
	/// its place in its effect's items (psys::manager::OrderedEffect, by psysAtomIndex[atom]).
	struct ParticleInstance
	{
		uint32_t index; ///< Its instance in instanceUniforms / instanceColours / instancePoses
		entt::id_type meshId;
		psys::DrawPath path;
		uint32_t effect;        ///< The effect's id (psys::manager)
		const psys::Atom* atom; ///< The atom (Effect::DrawAtom::atom)
		glm::vec3 key;          ///< The object's translation
		bool translucent;       ///< Additive or faded with the global alpha (mesh_atoms::Instance::translucent)
		bool additive;          ///< Also in additiveInstances
		bool cut;               ///< Drawn cut by a plane (sea_pass::CutAtoms); also in cutAtomInstances
		/// The render mode every material takes instead of its own (mesh_atoms::Instance::mode)
		std::optional<graphics::render_modes::Mode> mode {};
	};
	std::vector<ParticleInstance> psysAtoms;
	/// atom -> its index in psysAtoms
	std::unordered_map<const psys::Atom*, uint32_t> psysAtomIndex;
	/// With psys::manager::k_DrawByPath: the ranges of every PSys mesh atom, drawn by none of the other loops
	std::map<entt::id_type, const InstancedDrawDesc> psysAtomDrawDescs;
	/// Blended instances sorted at another point than their model matrix's translation (the one-shot orb, whose sort key
	/// is pushed toward the camera by its radius): instance index -> the point
	std::unordered_map<uint32_t, glm::vec3> sortPoints;
	/// How the models of a mesh take the land light (land_light::ObjectMode): the plain models, trees, worship sites,
	/// spell icons, the Dove class; one per mesh (RenderingSystem LandLightOf), refilled at every PrepareDraw
	std::unordered_map<entt::id_type, land_light::ObjectLight> meshLandLight;
	/// Where each entity's model matrix is this frame
	struct EntityInstance
	{
		entt::id_type meshId;
		uint32_t index;
		bool morphWithTerrain;
		/// Takes dynamic shadows (off for trees, forests, some pots...): the hand's shadow
		bool receivesDynamicShadow;
	};
	std::unordered_map<entt::entity, EntityInstance> entityInstances;
	/// (openblack engine) no row: an entity's casterIndex when it casts no static shadow
	static constexpr uint32_t k_NoInstanceRow = 0xFFFFFFFFu;
	/// Every drawn entity's rows of this layout, in the write walk's order (RenderingSystem::WriteEntityRow): what
	/// the fast path rewrites without a new layout
	struct EntityRow
	{
		entt::entity entity;
		uint32_t index;       ///< instanceUniforms / instanceColours
		uint32_t casterIndex; ///< its static shadow's (shadowCasterDrawDescs), or k_NoInstanceRow
	};
	std::vector<EntityRow> entityRows;
	/// The bones of the instances that are not entities: the PSys mesh atoms of a ParticleAnimCreator
	/// (Particles/Creators/Mesh.h), instance index -> the bones' model matrices, refilled at every PrepareDraw
	std::unordered_map<uint32_t, std::vector<glm::mat4>> instancePoses;
	/// The objects that cast a static shadow (see RenderingSystem.cpp, CastsStaticShadow), again, in their own range
	std::map<entt::id_type, const InstancedDrawDesc> shadowCasterDrawDescs;
	/// Not an actual vertex buffer, but a dynamic general purpose buffer which
	/// stores uniform data as a GPU-side copy of \ref _instanceUniforms and
	/// which is populated in \ref PrepareDraw and consumed in \ref DrawModels.
	/// This buffer will resize if the size of \ref _instanceUniforms exceeds
	/// its allocated size. It will never shrink.
	/// The values stored are a list of uniforms (model matrix) needed for both
	/// the instances of entities and their bounding boxes.
	graphics::DynamicVertexBufferHandle instanceUniformBuffer;

	/// The instances are written again at the next PrepareDraw (Registry::SetDirty)
	bool dirty {true};
	/// The draw lists are made again at the next PrepareDraw (a component of the draw layout gained or lost,
	/// ecs::k_ChangesDrawLayout); without it a dirty frame writes the instances into the ranges they have
	bool layoutDirty {true};
	bool hasBoundingBoxes {false};
};

class RenderingSystemInterface
{
public:
	/// What the drawn entities look like or where they are changed: the instances are written again
	virtual void SetDirty() = 0;
	/// Which entities are drawn, or with which mesh or in which range, changed: the draw lists are made again
	virtual void SetLayoutDirty() = 0;
	virtual void PrepareDraw(bool drawBoundingBox, bool drawFootpaths, bool drawStreams) = 0;
	[[nodiscard]] virtual const RenderContext& GetContext() = 0;
	virtual ~RenderingSystemInterface() = default;
};
} // namespace openblack::ecs::systems
