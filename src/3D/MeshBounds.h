/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <span>
#include <vector>

#include <L3DFile.h>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

/// The box that the 3D engine puts round a mesh. A part with bones has its vertices in the space of their bones. The
/// engine puts the bones in the pose that the model rests in. Then it moves each group of vertices by its bone. The box
/// holds the smallest and largest point on each axis. The draw distances of a model come from the size of this box.
namespace openblack::mesh_bounds
{

/// The matrix of each bone in the rest pose: the turn and place of the bone, then the matrix of its parent. A bone
/// without a parent stays as it is. The columns of each matrix are the rows of the engine, and the last is the place.
[[nodiscard]] std::vector<glm::mat4> RestPose(std::span<const l3d::L3DBone> bones);

/// A vertex moved by the rest matrix of its bone, in the order of the engine. On each axis, the engine adds the x and
/// z products first, then the y product, then the place.
[[nodiscard]] glm::vec3 OnBone(const glm::mat4& bone, const glm::vec3& vertex) noexcept;

} // namespace openblack::mesh_bounds
