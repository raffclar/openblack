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

#include <array>
#include <span>
#include <vector>

#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/SkeletalAnimation.h"
#include "Help/Spirits.h"

/// The advisors' bodies posed: the anim layers an advisor lists each frame played over its skeleton, and what is read
/// off the pose, the way its head turns to look at something and the tip of its pointing finger
namespace openblack::help::spirits
{

/// An advisor's skeleton: each bone's parent and rest pose
struct SpiritRig
{
	skeletal_animation::Skeleton skeleton;
	/// Each bone's rest pose relative to its parent, where the layers start from
	std::vector<skeletal_animation::Pose> rest;

	[[nodiscard]] bool Empty() const { return rest.empty(); }
};

/// From the mesh's bones, their parents and their rest matrices in the mesh's space
[[nodiscard]] SpiritRig MakeSpiritRig(std::span<const uint32_t> parents, std::span<const glm::mat4> rest);

/// The matrix an advisor is drawn with: its three rows as the axes, then its position
[[nodiscard]] glm::mat4 ModelMatrix(const glm::mat3& rows, const glm::vec3& position);

/// The layers in order over the rest pose, then every bone put in the world under the model matrix
[[nodiscard]] std::vector<glm::mat4> EvaluatePose(const SpiritRig& rig, const DudeData& data, std::span<const AnimLayer> layers,
                                                  const glm::mat4& model);

/// How far the head turns to look at a target, across and up, each at most a sixth of a turn and scaled into [-0.5,
/// 0.5]: from the head bone, centred between the eyes. A target behind the head gives no turn.
[[nodiscard]] glm::vec2 HeadAngles(std::span<const glm::mat4> world,
                                   const std::array<uint32_t, helpdude::k_FaceBones>& faceBones, const glm::vec3& target);

/// The tip of the pointing finger: the root bone's place, moved along its first and third axes by the file's offsets in
/// units of the advisor's height
[[nodiscard]] glm::vec3 Fingertip(std::span<const glm::mat4> world, const DudeData& data, const glm::vec3& fallback);

} // namespace openblack::help::spirits
