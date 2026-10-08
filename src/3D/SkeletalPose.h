/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <vector>

#include <glm/mat4x4.hpp>

namespace openblack
{
class L3DAnim;
}

namespace openblack::graphics
{
class L3DMesh;

/// The model matrix of every bone of the mesh at `milliseconds` into the clip (the frames interpolated like the
/// original, then each bone put under its parent). A clip for another skeleton leaves the rest pose. `locals`, when
/// given: the frame's local matrices (L3DAnim::SampleLocal, what the skinning uses), empty for a clip
/// of another skeleton
void ComputePose(const L3DMesh& mesh, const L3DAnim& clip, float milliseconds, std::vector<glm::mat4>& pose,
                 std::vector<glm::mat4>* locals = nullptr);
/// ComputePose with the locals kept: they are sampled into `locals` in place, so a buffer kept from frame to frame is
/// reused instead of a new one each time
void ComputePoseInto(const L3DMesh& mesh, const L3DAnim& clip, float milliseconds, std::vector<glm::mat4>& pose,
                     std::vector<glm::mat4>& locals);

/// The SuperVillager's cross-fade: the blend sampler of `clip` at
/// `milliseconds` stored with weight 1 - oldWeight (each float ((b - a) f + a) x weight), then that of `oldClip` at
/// `oldMilliseconds` with oldWeight added, in the bones' local matrices, no normalisation; then each bone under its
/// parent as ComputePose. A clip for another skeleton leaves the rest pose.
void ComputeBlendedPose(const L3DMesh& mesh, const L3DAnim& clip, float milliseconds, const L3DAnim& oldClip,
                        float oldMilliseconds, float oldWeight, std::vector<glm::mat4>& pose);

} // namespace openblack::graphics
