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

#include <unordered_map>
#include <vector>

#include <entt/core/fwd.hpp>
#include <entt/entity/fwd.hpp>
#include <glm/mat4x4.hpp>

#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Systems/RenderingSystemInterface.h"

namespace openblack::graphics
{
class L3DMesh;
}

namespace openblack::ecs
{

/// Skeletal animation of boned meshes (components::SkeletalAnimation): AllAnims.anm clips played on villagers and
/// animals (docs/bw1-notes/animation.md).

/// The animation manager's id of the clip with that index in AllAnims.anm (the ANM_ enum of Data\AllMeshes.h)
entt::id_type ClipId(uint32_t index);

/// Advances every playing clip by that many milliseconds of game time and recomputes the poses.
void UpdateAnimations(float milliseconds);

/// A SuperVillager's fade state (SkeletalAnimation::crossFade) once a frame: the first clip is only kept; another clip
/// starts a fade from the one drawn last (old clip, its last time, fadeMs left, weight 1); the same clip counts the
/// fade down by the frame's ms (below 0: 0, else weight = left / fadeMs). True while there is fade left (the blend is
/// drawn)
bool StepCrossFade(components::SkeletalAnimation::CrossFade& fade, entt::id_type clip, int32_t milliseconds, int32_t fadeMs);

/// The bones the body is drawn with (RenderingSystem's instance poses, the SuperVillagers' eyes): a SuperVillager's
/// cross-faded pose while its fade is drawn (SkeletalAnimation::drawnPose), else the plain pose
[[nodiscard]] const std::vector<glm::mat4>& DrawnPose(const components::SkeletalAnimation& animation);

/// The renderer's view of the poses: instance index -> the bones' model matrices.
using PoseMap = std::unordered_map<uint32_t, const std::vector<glm::mat4>*>;
PoseMap PosesByInstance(const std::unordered_map<entt::entity, systems::RenderContext::EntityInstance>& entityInstances);
/// The same, with the poses of the instances that are not entities (RenderContext::instancePoses, the PSys mesh atoms)
PoseMap PosesByInstance(const systems::RenderContext& context);
/// whether any instance in [offset, offset + count) has its own pose
bool HasPose(const PoseMap& poses, uint32_t offset, uint32_t count);
/// points matrices / count at the instance's pose if it has one that fits the mesh (else leaves them)
void UsePose(const PoseMap& poses, uint32_t instance, const graphics::L3DMesh& mesh, const glm::mat4*& matrices,
             uint8_t& count);

} // namespace openblack::ecs
