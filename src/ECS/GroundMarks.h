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

#include <entt/entity/fwd.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

/// The temporary marks on the ground (a list, the newest first, of an object and its life in ms): a morphable object
/// of the mesh MeshId::TreeRootsPile that melts into the land once (land_morph), lasts 15000 ms and fades over its
/// last second. Two makers: an uprooted tree's crater (HandTrees.cpp) and an explosion on dry land.
/// Wiki: docs/bw1-notes/rendering-objects.md, "Meshes stuck to the ground (land_morph)".
namespace openblack::ecs::ground_marks
{

constexpr int32_t k_LifeMs = 15000;        ///< a mark's life
constexpr int32_t k_FadeMs = 1000;         ///< the last second fades
constexpr float k_FadeAlphaPerMs = 0.255f; ///< the alpha byte per ms of life left (255 at 1000 ms)
constexpr float k_ExplosionScale = 8.0f;   ///< an explosion's mark is 8 times the mesh

/// a mark at the position, turned about Y by the caller's rotation and uniformly scaled, with the white dust of
/// disappear_smoke mode 1. entt::null without the mesh.
entt::entity Create(const glm::vec3& position, const glm::mat3& rotation, float scale);

/// an explosion's mark: angle = PSysFloatRand(2 pi), scale 8; the turn is the rows (c, 0, s), (0, 1, 0),
/// (-s, 0, c)
entt::entity CreateExplosionMark(const glm::vec3& position, float angle);

/// once a frame: a mark with life <= 1000 ms takes alpha (life x 0.255, truncated);
/// life -= the frame's game ms; at 0 or less it goes. The marks whose entity went with the map are dropped.
void Update(float gameMilliseconds);

/// the list is emptied with the map
void Clear();

} // namespace openblack::ecs::ground_marks
