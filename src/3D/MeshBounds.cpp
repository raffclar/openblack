/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MeshBounds.h"

#include <cstdint>

#include <limits>

#include "Common/X87Matrix.h"

namespace openblack::mesh_bounds
{

namespace
{

/// The order in which the engine adds the products when it moves a vertex to bound a mesh: x, z, y on each axis
constexpr x87::AxisOrders k_BoundOrder {{{0, 2, 1}, {0, 2, 1}, {0, 2, 1}}};

glm::mat4 BoneMatrix(const l3d::L3DBone& bone)
{
	const auto& turn = bone.orientation;
	return {
	    glm::vec4(turn[0], turn[1], turn[2], 0.0f),
	    glm::vec4(turn[3], turn[4], turn[5], 0.0f),
	    glm::vec4(turn[6], turn[7], turn[8], 0.0f),
	    glm::vec4(bone.position.x, bone.position.y, bone.position.z, 1.0f),
	};
}

} // namespace

std::vector<glm::mat4> RestPose(std::span<const l3d::L3DBone> bones)
{
	std::vector<glm::mat4> pose;
	pose.reserve(bones.size());
	for (const auto& bone : bones)
	{
		// The engine moves a bone without a parent through a matrix that does not move it. A parent must come
		// before its child: the engine reads no other parent.
		const bool hasParent = bone.parent != std::numeric_limits<uint32_t>::max() && bone.parent < pose.size();
		const glm::mat4 parent = hasParent ? pose[bone.parent] : glm::mat4(1.0f);
		pose.push_back(x87::Product(BoneMatrix(bone), parent));
	}
	return pose;
}

glm::vec3 OnBone(const glm::mat4& bone, const glm::vec3& vertex) noexcept
{
	return x87::TransformPoint(bone, vertex, k_BoundOrder);
}

} // namespace openblack::mesh_bounds
