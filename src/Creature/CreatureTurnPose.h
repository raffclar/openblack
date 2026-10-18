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

#include <glm/mat4x4.hpp>

/// A creature's body is posed once a game turn, with its bones in the world. Each frame draws the body between the poses
/// of the last two turns, by the part of the turn that has passed. Thus the body, its place and its heading move
/// smoothly between turns, and a move that a turn makes shows over the next turn.
namespace openblack::creature_turn_pose
{
/// The largest weight that a frame gives to the newer pose
inline constexpr float k_MaxWeight = 2.0f;

/// The weight of the newer pose in the drawn body: the part of the turn that has passed, not more than the largest weight
[[nodiscard]] float DrawWeight(float turnFraction);

/// One number of a drawn bone: the newer number times the weight, plus the older number times one less the weight. Each
/// step rounds to a float, the newer product first.
[[nodiscard]] float BlendNumber(float older, float newer, float weight);

/// The bones of the drawn body, between the older and the newer pose. Only the turn and the place of each bone blend,
/// and the last row of each matrix stays (0, 0, 0, 1). The three spans must have the same length.
void Blend(std::span<const glm::mat4> older, std::span<const glm::mat4> newer, float weight, std::span<glm::mat4> drawn);

/// The bones in the world of a body posed in the space of its mesh, at a placement in the world
[[nodiscard]] std::vector<glm::mat4> ToWorld(std::span<const glm::mat4> bones, const glm::mat4& placement);

/// The bones in the space of the mesh of bones in the world, for a body drawn at a placement. The placement must have an
/// inverse.
void ToMesh(std::span<const glm::mat4> world, const glm::mat4& placement, std::span<glm::mat4> bones);
} // namespace openblack::creature_turn_pose
