/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>
#include <string_view>
#include <vector>

#include <entt/entity/entity.hpp>

#include "3D/L3DSubMesh.h"

namespace openblack::ecs::physics
{
/// What the callers of the partly built draw set around it
struct PartialBuildOptions
{
	/// The inner wall offset for both cull modes, in model units. Unset: the defaults
	/// (PartialBuild::k_InnerOffsetTwoSided / k_InnerOffsetCulled). The temple's draw sets both to
	/// PartialBuild::k_TempleInnerOffset around its call and restores them afterwards
	std::optional<float> innerOffset;
	/// The melting stream (on for morphable objects and the temple, off for static and complex ones): each vertex's
	/// land delta added to its model y before the cut test and the matrix. Unset: whether the entity has
	/// components::MorphWithTerrain
	std::optional<bool> melting;
	/// No cap. (pending) nothing in the original was found to set it; openblack
	/// has no equivalent, so it is clear unless a caller sets it
	bool noCap {false};
	/// The building draw calls it only for pct != 0 (nothing at all is drawn at 0); the temple
	/// calls it for any build progress < 1, so at 0 the scaffold is still drawn, lowered by the whole height
	bool skipZero {true};
};

/// The partly built draw of a building (used by every static, morphable and complex object; the building and temple
/// draws call it), in world space. The status 0
/// sub-meshes are cut at the plane y = pos.y + pct x H x scale (nothing of a primitive when that is under
/// 0.2 above the origin), each primitive again pushed in along its vertex normals (0.35, 0.2 for two-sided
/// materials, model units) and a cap joining both cuts; the scaffold (the highest status) rises
/// (pct < 0.2), stands, or is cut from the top (pct > 0.8). A damaged building draws it over its FragMesh, at
/// GetPercentForDrawBuilding.
class PartialBuild
{
public:
	static constexpr float k_InnerOffsetTwoSided = 0.2f; ///< two-sided materials
	static constexpr float k_InnerOffsetCulled = 0.35f;
	static constexpr float k_TempleInnerOffset = 1.0f; ///< the temple (both cull modes)

	[[nodiscard]] static std::vector<graphics::L3DSubMesh::GeneratedPrimitive>
	Build(entt::entity building, entt::id_type mesh, float percent, const PartialBuildOptions& options = {});
	/// Build, moved into the building's own space (its Transform) and loaded as a mesh named "<tag>/<n>", with the intact
	/// model as its mark on the landscape: what the building draw shows for pct != 0. 0 when nothing is left. When the
	/// entity keeps components::MorphWithTerrain the melting deltas only decide the cut: the vertex shader adds them.
	[[nodiscard]] static entt::id_type BuildMesh(entt::entity building, entt::id_type intactMesh, float percent,
	                                             std::string_view tag, const PartialBuildOptions& options = {});
	/// Erases a mesh BuildMesh made (0: nothing)
	static void EraseMesh(entt::id_type id);
	PartialBuild() = delete;
};
} // namespace openblack::ecs::physics
