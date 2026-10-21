/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WorldTextDepth.h"

#include <cmath>

#include <glm/vec4.hpp>

#include "Common/X87.h"

namespace openblack::gui::world_text
{

namespace
{
/// The pitch of a camera that looks straight down: a quarter turn, as a float
constexpr double k_StraightDown = 1.5707963705062866;
/// The pitch is divided by the quarter turn with this product. It is not the nearest double to 2 over pi.
constexpr double k_PerQuarterTurn = 0.6366197546520227;
} // namespace

float DrawnDepth(float pointDepth, float cameraPitch, float nearClip)
{
	// The share of the quarter turn that the camera looks down, from 0 to 1
	double pitch = std::fabs(cameraPitch);
	if (x87::LessEqual(static_cast<float>(pitch), 0.0))
	{
		pitch = 0.0;
	}
	else if (!x87::Less(static_cast<float>(pitch), k_StraightDown))
	{
		pitch = k_StraightDown;
	}
	const float down = x87::Mul(pitch, k_PerQuarterTurn);

	const float nearest = nearClip + nearClip;
	float pulled = pointDepth - k_LevelPull;
	if (pulled < nearest)
	{
		pulled = nearest;
	}
	const float beyond = pulled - nearest;
	const float drawn = (beyond - (std::fabs(beyond) * (down * down))) + nearest;
	return drawn < nearest ? nearest : drawn;
}

float BufferDepth(const glm::mat4& projection, float depth)
{
	const auto clip = projection * glm::vec4(0.0f, 0.0f, depth, 1.0f);
	return clip.z / clip.w;
}

} // namespace openblack::gui::world_text
