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

#include <memory>
#include <optional>
#include <vector>

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "FragMesh.h"

namespace openblack::graphics::world_triangles
{
struct Frame;
}

namespace openblack::ecs::physics
{
struct PhysicsObject;

/// One FragMesh draw of a frame (openblack, for the draw snapshot): taken with the game data (SnapshotFragMeshes),
/// drawn without it (AppendFragMeshes). The FragMesh is shared, not copied: once in a component it is never changed
/// (an impact or a merge makes a new one), so the draw may hold it while the game goes on.
struct FragMeshDraw
{
	std::shared_ptr<const FragMesh> mesh;
	/// a fragment's world matrix (its draw pose while it flies); none for a broken building's (its triangles are in the
	/// world)
	std::optional<glm::mat4> world;
	FragMesh::DrawLight light; ///< FragMesh::ObjectLight(position, tint, tintSpecular, frame) of the snapshot's frame
	/// what that light was made from, for a light measured again with another FrameLight
	glm::vec3 position {0.0f};
	uint32_t tint {0xFFFFFFFFu};
	uint32_t tintSpecular {0};
};

/// Buildings broken by thrown rocks and the fragments knocked off them.
class Buildings
{
public:
	/// Whether it breaks buildings: rocks (physics row 3) and the toy with row 20.
	[[nodiscard]] static bool PhysicallyDestroysAbodes(entt::entity entity);
	/// A hit on the building's proxy. Returns false when the building is gone.
	static bool ReactToPhysicsImpact(entt::entity building, PhysicsObject& po);
	/// When a fragment stops: a big piece (area > 9) whose building still stands becomes its rubble. Returns the entity
	/// that stays (entt::null when it merged).
	static entt::entity FragmentEndPhysics(entt::entity fragment, const PhysicsObject& po);
	/// Every game turn: a fragment vanishes when its time is up.
	static void ProcessTurn();
	static void DestroyFragment(entt::entity fragment);
	/// entt's on_destroy<BuildingDamage> (Registry::Destroy, Remove, Reset): the FragMesh's generated model out of the
	/// mesh cache at the real destruction of a deleted building (dead list: the zombie's body leaves at the start of the
	/// next turn). Connected once per registry, from abodes::ConnectDrawMeshListener (idempotent).
	static void ConnectDamageListener();
	/// The draw of a damaged building: its FragMesh plus the intact model partly built over it as its DrawMesh; for the
	/// abodes' redraws (repair, life, built) while it has a FragMesh.
	/// Nothing for a building with none.
	static void Redraw(entt::entity building);
	/// Once repaired: the FragMesh goes, the whole model is drawn again (the construction draw if it has a site).
	/// Nothing for a building with none.
	static void RemoveDamage(entt::entity building);
	/// Buildings forget a hitter (the pass-through pair ends); with a building given, that building forgets its last
	/// hitter.
	static void ForgetHitter(entt::entity hitter, entt::entity building = entt::null);
	/// This frame's draws of every FragMesh (first pass): a broken building's (no matrix, its position, the fire's
	/// charring grey and glow or 0xFFFFFFFF / 0) and a fragment's (its world matrix and translation, the FragMesh's
	/// default 0xFFFFFFFF / 0)
	/// = SnapshotFragMeshes + AppendFragMeshes(out, draws), in one thread
	/// `turnTime`: the turn and its fraction the fire glow is computed at (SnapshotFragMeshes)
	static void AppendFragMeshes(graphics::world_triangles::Frame& out, const FragMesh::FrameLight& frame, float turnTime);
	/// The game side of AppendFragMeshes: `out` is cleared and gets this frame's draws, in AppendFragMeshes' order (the
	/// broken buildings, then the fragments), with their matrix and light (the fire's tint, the land light, the haze)
	static void SnapshotFragMeshes(std::vector<FragMeshDraw>& out, const FragMesh::FrameLight& frame, float turnTime);
	/// The draw side: FragMesh::AppendDraw of every draw, in order. Reads nothing but `draws` and their FragMeshes
	static void AppendFragMeshes(graphics::world_triangles::Frame& out, const std::vector<FragMeshDraw>& draws);
	Buildings() = delete;
};
} // namespace openblack::ecs::physics
