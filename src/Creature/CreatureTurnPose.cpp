/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureTurnPose.h"

#include <cassert>

#include <algorithm>

#include <glm/matrix.hpp>

#include "Common/X87.h"

namespace openblack::creature_turn_pose
{

float DrawWeight(float turnFraction)
{
	// The game compares the part of the turn with a double constant
	return x87::Less(turnFraction, static_cast<double>(k_MaxWeight)) ? turnFraction : k_MaxWeight;
}

// The weights and the numbers are floats. Thus each product and the sum round to a float one time, as the game rounds
// them, and plain float operations give the same bits.
float BlendNumber(float older, float newer, float weight)
{
	const float newerPart = weight * newer;
	const float olderWeight = 1.0f - weight;
	return (olderWeight * older) + newerPart;
}

namespace
{
/// One drawn bone between the older and the newer bone, with the weight of the older bone given
inline glm::mat4 BlendBone(const glm::mat4& older, const glm::mat4& newer, float weight, float olderWeight)
{
	glm::mat4 out;
	for (glm::length_t column = 0; column < 4; ++column)
	{
		for (glm::length_t row = 0; row < 3; ++row)
		{
			const float newerPart = weight * newer[column][row];
			out[column][row] = (olderWeight * older[column][row]) + newerPart;
		}
		out[column][3] = column == 3 ? 1.0f : 0.0f;
	}
	return out;
}
} // namespace

void Blend(std::span<const glm::mat4> older, std::span<const glm::mat4> newer, float weight, std::span<glm::mat4> drawn)
{
	assert(older.size() == drawn.size() && newer.size() == drawn.size());
	const float olderWeight = 1.0f - weight;
	for (size_t bone = 0; bone < drawn.size(); ++bone)
	{
		drawn[bone] = BlendBone(older[bone], newer[bone], weight, olderWeight);
	}
}

void BlendToMesh(std::span<const glm::mat4> older, std::span<const glm::mat4> newer, float weight, const glm::mat4& placement,
                 std::span<glm::mat4> bones)
{
	assert(older.size() == bones.size() && newer.size() == bones.size());
	const auto inverse = glm::inverse(placement);
	const float olderWeight = 1.0f - weight;
	for (size_t bone = 0; bone < bones.size(); ++bone)
	{
		bones[bone] = inverse * BlendBone(older[bone], newer[bone], weight, olderWeight);
	}
}

std::vector<glm::mat4> ToWorld(std::span<const glm::mat4> bones, const glm::mat4& placement)
{
	std::vector<glm::mat4> world(bones.size());
	std::ranges::transform(bones, world.begin(), [&placement](const glm::mat4& bone) { return placement * bone; });
	return world;
}

void ToMesh(std::span<const glm::mat4> world, const glm::mat4& placement, std::span<glm::mat4> bones)
{
	assert(world.size() == bones.size());
	const auto inverse = glm::inverse(placement);
	std::ranges::transform(world, bones.begin(), [&inverse](const glm::mat4& bone) { return inverse * bone; });
}

} // namespace openblack::creature_turn_pose
