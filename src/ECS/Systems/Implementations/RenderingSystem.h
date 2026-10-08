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
#include <map>
#include <optional>
#include <utility>
#include <vector>

#include <glm/mat4x4.hpp>

#include "3D/AllMeshes.h"
#include "Particles/Creators/Mesh.h"
#include "RenderingSystemCommon.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack
{
class DayNightClock;
} // namespace openblack

namespace openblack::ecs
{
class Registry;
} // namespace openblack::ecs

namespace openblack::ecs::components
{
struct Mesh;
struct Transform;
} // namespace openblack::ecs::components

namespace openblack::ecs::systems
{
/// The components one row of the write walk asks about more than once, looked up once (RenderingSystem.cpp)
struct RowParts;

/// Which of the draw layout's components (ecs::k_ChangesDrawLayout) a row's entity has, as the write walk reads them.
/// They change only with a layout mark (SetLayoutDirty), so a refill takes them from the last walk's row
struct RowClasses
{
	bool alpha;
	bool tree;
	bool field;
	bool abode;
	bool feature;
	bool pot;
	bool villager;
	bool creature;
	bool hand;
	bool deadTree;
	bool bigForest;
	/// A class that casts a static shadow and none of the classes that turn it off (CastsStaticShadow)
	bool staticShadowClass;
	bool morphWithTerrain;
	bool needsSorting;
	bool drawMesh;
};

class RenderingSystem final: public RenderingSystemCommon
{
public:
	~RenderingSystem();
	/// Also marks the rows' classes stale: the next walk finds them again
	void SetLayoutDirty() override;

private:
	void PrepareDrawDescs(bool drawBoundingBox) override;
	void PrepareDrawUploadUniforms(bool drawBoundingBox) override;
	/// The instances written again into the ranges of the last PrepareDrawDescs: the entities' rows by the same walk,
	/// which must fill every range to its count and no more (else false: the draw lists are made again), then this
	/// frame's PSys mesh atoms laid out again in their range after the entities'
	[[nodiscard]] bool RefillKeepingDescs() override;
	/// Takes what every row reads and the PrepareDraw does not change, once: the physics objects' flying state, the
	/// held object, the day and night clock (the tree brightness at the first tree)
	void BeginPrepareDraw() override;
	/// The entities' rows (the drawn ones, their static shadows, the footprint-only ones). `check` (a refill): false
	/// when an entity has no room left in its range or a range is not filled to its count
	bool WriteEntityRows(bool drawBoundingBox, bool check);
	/// The PSys mesh atoms' rows, in psysAtomDrawDescs
	void WriteAtomRows();

	/// The entity ranges of the draw lists, in the order _instanceSlots keeps them
	enum class RowRange : uint8_t
	{
		Instanced,     ///< instancedDrawDescs
		Translucent,   ///< translucentDrawDescs
		SortedOpaque,  ///< sortedOpaqueDrawDescs
		ShadowCaster,  ///< shadowCasterDrawDescs
		FootprintOnly, ///< footprintOnlyDrawDescs
	};
	static constexpr size_t k_RowRanges = 5;
	/// No range: a mesh not in the draw lists, or no hint
	static constexpr uint32_t k_NoSlot = 0xFFFFFFFFu;
	/// _instanceSlots made again from the current draw lists, every range with no row written yet
	void LayOutSlots();
	/// The slot of a mesh's range of the given kind, or k_NoSlot. `hint`, a slot to try first (the last walk's for the
	/// row at the same place), is taken only when it is that kind's range of that mesh
	[[nodiscard]] uint32_t FindSlot(RowRange range, entt::id_type meshId, uint32_t hint) const;
	/// _psysMeshes: this frame's PSys mesh atoms and object ghosts whose meshes are loaded
	void CollectMeshAtoms();
	/// psysAtomDrawDescs from the atoms' counts by mesh, from the end of the entities' rows
	void LayOutAtoms(const std::map<entt::id_type, uint32_t>& psysAtomIds);
	/// The write walk's body for one entity: its row `idx` (instanceUniforms / instanceColours, entityInstances,
	/// meshLandLight, the bounding box row) and its static shadow's row `casterIdx` (RenderContext::k_NoInstanceRow:
	/// none), from the components the walk looked up for it (`parts`). (openblack engine, Motor GPU 1) the fast path
	/// (step 2b) calls it again for the rows that change
	void WriteEntityRow(const ecs::Registry& registry, entt::entity entity, const RowParts& parts, const components::Mesh& mesh,
	                    const components::Transform& transform, uint32_t idx, uint32_t casterIdx, bool drawBoundingBox);
	/// The classes of the walk's row `row`, an `entity`: the last walk's when that row was the same entity and no layout
	/// mark came since, else found again and kept for the next walk
	[[nodiscard]] RowClasses ClassesOfRow(const ecs::Registry& registry, uint32_t row, entt::entity entity);

	/// The mesh atoms of the particle effects this frame (Particles/Creators/Mesh.h), drawn as instances of their mesh
	std::vector<psys::mesh_atoms::Instance> _psysMeshes;
	/// The first row after the entities' ranges: where the PSys mesh atoms' range starts
	uint32_t _entityRowsEnd {0};
	/// Every physics object's entity and whether it flies (not resting), sorted by entity and, for the same entity,
	/// in the physics list's order: PhysicsObjects::IsFlying for every row, taken once per PrepareDraw
	std::vector<std::pair<entt::entity, bool>> _physicsFlying;
	/// The hand's held object (it casts no static shadow), taken once per PrepareDraw
	std::optional<entt::entity> _held;
	/// The trees' brightness this frame (ecs::TreeBrightness), asked at the first tree of a PrepareDraw
	std::optional<uint8_t> _treeBrightness;
	/// The day and night clock (the abodes' window colour), or null without one; taken once per PrepareDraw
	const DayNightClock* _dayNightClock {nullptr};
	/// Where the next row of a mesh goes in one of its ranges: the range and how many rows this walk wrote into it
	struct InstanceSlots
	{
		entt::id_type meshId;
		uint32_t offset;
		uint32_t count;
		uint32_t filled;
	};
	/// Every entity range of the draw lists, made at the start of each walk from them: by RowRange, then by mesh id
	std::vector<InstanceSlots> _instanceSlots;
	/// Where each RowRange starts in _instanceSlots (and, last, its end)
	std::array<uint32_t, k_RowRanges + 1> _rangeBegin {};
	/// The slots the last walk gave each row, by its place in the walk (entityRows): its range and its static shadow's
	/// (k_NoSlot: none), tried first for the row at the same place
	std::vector<std::pair<uint32_t, uint32_t>> _rowSlots;
	/// The entries of RenderContext::entityInstances taken out at the start of a walk, put back with this frame's rows:
	/// the map is filled again without a heap node per row
	std::vector<decltype(RenderContext::entityInstances)::node_type> _instanceNodes;
	/// Each row's entity and its classes as the last walks found them, by its place in the walk (entityRows)
	std::vector<std::pair<entt::entity, RowClasses>> _rowClasses;
	/// A layout mark, or a rebuild, since _rowClasses was checked: the next walk empties it first
	bool _rowClassesStale {true};
};
} // namespace openblack::ecs::systems
