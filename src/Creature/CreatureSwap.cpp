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
	const auto sizeGap = static_cast<double>(target.size) - static_cast<double>(now.size);
	const auto alignmentGap = static_cast<float>(static_cast<double>(target.alignment) - static_cast<double>(now.alignment));
	return std::abs(sizeGap) < k_Settled && std::abs(static_cast<double>(alignmentGap)) < k_Settled;
}

Shape creature_swap::Ease(const Shape& now, const Shape& target)
{
	// The size's gap is worked out at full precision; the alignment's is kept as a float first
	const auto sizeGap = static_cast<double>(target.size) - static_cast<double>(now.size);
	const auto alignmentGap = static_cast<float>(static_cast<double>(target.alignment) - static_cast<double>(now.alignment));
	const auto size = static_cast<float>(sizeGap * static_cast<double>(k_EaseShare) + static_cast<double>(now.size));
	const auto alignment = static_cast<float>(static_cast<double>(alignmentGap) * static_cast<double>(k_EaseShare) +
	                                          static_cast<double>(now.alignment));
	return {.size = size, .alignment = std::clamp(alignment, -1.0f, 1.0f)};
}

uint32_t creature_swap::RescaledSightings(uint32_t seen, float neededBefore, float neededAfter)
{
	const auto factor = neededBefore > 0.0f ? static_cast<double>(neededAfter) / static_cast<double>(neededBefore) : 1.0;
	const auto scaled = static_cast<double>(seen) * factor;
	return scaled <= 0.0 ? 0u : static_cast<uint32_t>(scaled);
}
