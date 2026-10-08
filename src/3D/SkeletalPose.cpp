/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SkeletalPose.h"

#include <algorithm>
#include <limits>

#include "3D/L3DAnim.h"
#include "3D/L3DMesh.h"

namespace openblack::graphics
{

namespace
{
/// W = L * W(parent) with row vectors, so Wparent * L here (bones come after their
/// parents)
void PutUnderParents(const L3DMesh& mesh, const std::vector<glm::mat4>& local, std::vector<glm::mat4>& pose)
{
	const auto& parents = mesh.GetBoneParents();
	for (size_t i = 0; i < local.size(); ++i)
	{
		pose[i] = parents[i] != std::numeric_limits<uint32_t>::max() ? pose[parents[i]] * local[i] : local[i];
	}
}
} // namespace

void ComputePose(const L3DMesh& mesh, const L3DAnim& clip, float milliseconds, std::vector<glm::mat4>& pose,
                 std::vector<glm::mat4>* locals)
{
	if (locals != nullptr)
	{
		ComputePoseInto(mesh, clip, milliseconds, pose, *locals);
		return;
	}
	std::vector<glm::mat4> local;
	ComputePoseInto(mesh, clip, milliseconds, pose, local);
}

void ComputePoseInto(const L3DMesh& mesh, const L3DAnim& clip, float milliseconds, std::vector<glm::mat4>& pose,
                     std::vector<glm::mat4>& locals)
{
	const auto& rest = mesh.GetBoneMatrices();
	// sampled straight into the caller's buffer: SampleLocal sizes it and writes every bone
	clip.SampleLocal(static_cast<int32_t>(std::max(0.0f, milliseconds)), locals);
	pose = rest;
	if (locals.size() != rest.size())
	{
		locals.clear();
		return;
	}
	PutUnderParents(mesh, locals, pose);
}

void ComputeBlendedPose(const L3DMesh& mesh, const L3DAnim& clip, float milliseconds, const L3DAnim& oldClip,
                        float oldMilliseconds, float oldWeight, std::vector<glm::mat4>& pose)
{
	const auto& rest = mesh.GetBoneMatrices();
	std::vector<glm::mat4> local;
	std::vector<glm::mat4> old;
	clip.SampleLocal(static_cast<int32_t>(std::max(0.0f, milliseconds)), local, true);
	oldClip.SampleLocal(static_cast<int32_t>(std::max(0.0f, oldMilliseconds)), old, true);
	pose = rest;
	if (local.size() != rest.size() || old.size() != rest.size())
	{
		return;
	}
	// the new clip's weight
	const float weight = 1.0f - oldWeight;
	for (size_t i = 0; i < local.size(); ++i)
	{
		// the 12 floats of the original 4x3 matrix (glm's 3 rows of each of the 4 columns); the 4th row stays (0, 0, 0, 1)
		for (int column = 0; column < 4; ++column)
		{
			for (int row = 0; row < 3; ++row)
			{
				// one operation per statement (no contraction), as the FPU at 24 bits rounds each
				const float stored = local[i][column][row] * weight;
				const float added = old[i][column][row] * oldWeight;
				local[i][column][row] = added + stored;
			}
		}
	}
	PutUnderParents(mesh, local, pose);
}

} // namespace openblack::graphics
