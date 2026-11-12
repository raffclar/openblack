/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <entt/entity/fwd.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/ScreenPick.h"
#include "ECS/PlacedMemo.h"

namespace openblack::graphics
{
class L3DMesh;
class L3DSubMesh;
} // namespace openblack::graphics

namespace openblack::ecs
{
class Registry;
}

namespace openblack::creature_morph
{
struct Morph;
}

/// A model's triangles where they are drawn this frame, which the cursor pick and the marks a blow or a burn leaves on a
/// creature's skin both test lines against
namespace openblack::ecs::posed_model
{
/// The bones a model is posed by as it is drawn: an animal by its clip, a villager by its state's clip, a creature by
/// its animations, anything else in the pose its model rests in
[[nodiscard]] std::span<const glm::mat4> BonesOf(const ecs::Registry& registry, entt::entity entity,
                                                 const graphics::L3DMesh& mesh);

/// The corners of a submesh in the world as it is drawn: posed by its bones, a creature's body blended from its
/// meshes, and a model following the land moved up and down with it
void Place(const ecs::Registry& registry, entt::entity entity, const graphics::L3DMesh& mesh, size_t index,
           const glm::mat4& model, std::vector<glm::vec3>& corners);
/// The same, posed by other bones than the model's own, as a copy of a creature drawn elsewhere in a pose of its own.
/// With a shape, a creature's body is placed with that shape, as a copy with a shape of its own is drawn.
void Place(const ecs::Registry& registry, entt::entity entity, const graphics::L3DMesh& mesh, size_t index,
           const glm::mat4& model, std::span<const glm::mat4> bones, std::vector<glm::vec3>& corners,
           const creature_morph::Morph* shape = nullptr);

/// How a placement moves the corners of a model that follows the land
enum class LandFollow : uint8_t
{
	/// Each corner moves up and down with the land under it, as the draw moves it
	AsDrawn,
	/// Each corner moves by the rise that the cursor pick reads for it (see `PickedRiseOffset`)
	AsPicked,
	/// The corners stay where the bones and the matrix put them, as the line test of the hand feel uses them
	Ignore,
};

/// Whether a placement moves the corners of a model with the land under them
[[nodiscard]] constexpr bool MovesWithLand(LandFollow landFollow, bool modelFollowsLand)
{
	return landFollow != LandFollow::Ignore && modelFollowsLand;
}

/// The number of corners of a submesh, and whether the cursor pick tests the submesh
struct SubMeshCorners
{
	size_t corners;
	bool tested;
};

/// A model that follows the land keeps the rise of each corner in one list, one submesh after the other. The cursor
/// pick reads this list from the start, but it counts only the corners of the submeshes that it tests. Thus after a
/// submesh that the pick does not test, each corner gets the rise of a different corner. The return value is the
/// place in the list where the pick starts to read the rises of the submesh `index`.
[[nodiscard]] size_t PickedRiseOffset(std::span<const SubMeshCorners> subMeshes, size_t index);

/// The submesh and the corner in that submesh at a place in the list of the rises of all the corners of a model
[[nodiscard]] std::optional<std::pair<size_t, size_t>> CornerAt(std::span<const SubMeshCorners> subMeshes, size_t place);

/// The same, but the memo gives the corners that it keeps when no input changed since it calculated them. The corners
/// stay valid until the next call with the same memo.
[[nodiscard]] placed_memo::Placed Place(placed_memo::Memo& memo, const ecs::Registry& registry, entt::entity entity,
                                        const graphics::L3DMesh& mesh, size_t index, const glm::mat4& model,
                                        LandFollow landFollow = LandFollow::AsDrawn);

/// Whether the submesh is drawn at the level of detail and stage the model is drawn at: the nearest level of detail and
/// the first stage, as openblack draws every model
[[nodiscard]] bool IsDrawn(const graphics::L3DSubMesh& subMesh);

/// Where a line first meets a model's nearest level of detail as it is posed, the texture corners of the triangle it
/// meets and which of the model's skins that triangle is drawn with
struct SkinHit
{
	screen_pick::MeshHit hit;
	std::array<glm::vec2, 3> uvs;
	/// Which of the model's skins: the last of them that the first submesh's primitive of the same place uses, the
	/// first skin when none does
	uint32_t skin {0};
	/// The submesh the triangle is in
	uint32_t subMesh {0};
};
/// With behind allowed, the whole line counts and the hit furthest back along it is taken.
[[nodiscard]] std::optional<SkinHit> NearestSkinHit(const ecs::Registry& registry, entt::entity entity,
                                                    const graphics::L3DMesh& mesh, const glm::mat4& model, glm::vec3 origin,
                                                    glm::vec3 direction, bool behindAllowed = false);
/// The same, with the model placed by the given bones (in the model's space) and not by the bones it is drawn with
[[nodiscard]] std::optional<SkinHit> NearestSkinHit(const ecs::Registry& registry, entt::entity entity,
                                                    const graphics::L3DMesh& mesh, const glm::mat4& model,
                                                    std::span<const glm::mat4> bones, glm::vec3 origin, glm::vec3 direction,
                                                    bool behindAllowed = false);
/// A point kept on a model's skin, which moves with it as it is posed: a triangle of a submesh, by its first index, and
/// how far along its first side (s) and third side (t) from its first corner
struct SkinPoint
{
	uint32_t subMesh {0};
	uint32_t firstIndex {0};
	float s {0.0f};
	float t {0.0f};
};
/// Where a point on the skin is as the model is drawn this frame: only its triangle's three corners are placed. None when
/// the model no longer has that triangle.
[[nodiscard]] std::optional<glm::vec3> PointOnSkin(const ecs::Registry& registry, entt::entity entity,
                                                   const graphics::L3DMesh& mesh, const glm::mat4& model,
                                                   const SkinPoint& point);
/// The three corners of the triangle of a point on the skin, posed by other bones than the model's own. None when the
/// model does not have that triangle. With a shape, a creature's body is placed with that shape.
[[nodiscard]] std::optional<std::array<glm::vec3, 3>> CornersOnSkin(const ecs::Registry& registry, entt::entity entity,
                                                                    const graphics::L3DMesh& mesh, const glm::mat4& model,
                                                                    std::span<const glm::mat4> bones, const SkinPoint& point,
                                                                    const creature_morph::Morph* shape = nullptr);
/// A triangle's first corner, moved s of the way along its first side and t along its third
[[nodiscard]] glm::vec3 SkinPointPosition(const std::array<glm::vec3, 3>& corners, float s, float t);
} // namespace openblack::ecs::posed_model
