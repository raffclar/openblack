/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureSwap.h"

#include <cmath>

#include <algorithm>

#include "Common/ModelInverseSquareRoot.h"
#include "Common/X87.h"

using namespace openblack;
using namespace openblack::creature_swap;

bool creature_swap::IsBeamTurn(uint32_t turn)
{
	return turn % k_BeamEveryTurns == 0;
}

particles::PlasmaCommand creature_swap::Beam(glm::vec3 from, glm::vec3 to, uint32_t alphaDraw)
{
	// Points nearer each other than a hundredth have the beam leave straight up
	constexpr float k_SamePointSquared = 0.0001f;
	auto direction = to - from;
	if (direction.z * direction.z + direction.y * direction.y + direction.x * direction.x < k_SamePointSquared)
	{
		direction = {0.0f, 1.0f, 0.0f};
	}
	// Normalised with the 3D engine's own inverse square root
	direction *=
	    gutils::ModelInverseSquareRoot(direction.z * direction.z + direction.y * direction.y + direction.x * direction.x);
	return {
	    .start = from,
	    .end = to,
	    .startTangent = direction,
	    .endTangent = direction,
	    .life = k_BeamLife,
	    .speed = k_BeamSpeed,
	    .alpha = static_cast<uint8_t>(k_BeamLowestAlpha + alphaDraw),
	};
}

bool creature_swap::IsSettled(const Shape& now, const Shape& target)
{
	const float sizeGap = target.size - now.size;
	const float alignmentGap = target.alignment - now.alignment;
	return x87::Less(std::abs(sizeGap), k_Settled) && x87::Less(std::abs(alignmentGap), k_Settled);
}

Shape creature_swap::Ease(const Shape& now, const Shape& target)
{
	const float sizeGap = target.size - now.size;
	const float alignmentGap = target.alignment - now.alignment;
	const float size = (sizeGap * k_EaseShare) + now.size;
	const float alignment = now.alignment + (alignmentGap * k_EaseShare);
	return {.size = size, .alignment = std::clamp(alignment, -1.0f, 1.0f)};
}

uint32_t creature_swap::RescaledSightings(uint32_t seen, float neededBefore, float neededAfter)
{
	const float factor = neededBefore > 0.0f ? neededAfter / neededBefore : 1.0f;
	// The count is exact in the multiply, and the product rounds to a float before it is truncated
	const float scaled = x87::Mul(static_cast<double>(seen), factor);
	return scaled <= 0.0f ? 0u : static_cast<uint32_t>(scaled);
}
