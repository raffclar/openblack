/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureAttention.h"

#include <cmath>

#include <glm/geometric.hpp>

using namespace openblack;

bool creature_attention::IsLookingAt(const glm::vec3& origin, const glm::vec3& focus, const glm::vec3& creature)
{
	const auto offset = creature - origin;
	const auto distance = glm::length(offset);
	if (distance == 0.0f || distance >= k_LookingRange)
	{
		return false;
	}
	// A camera looking nowhere has no way it looks, and sees nothing ahead of it
	auto ahead = focus - origin;
	if (ahead != glm::vec3(0.0f))
	{
		ahead = glm::normalize(ahead);
	}
	return glm::dot(offset / distance, ahead) > std::cos(k_LookingAngle);
}

double creature_attention::Share(uint32_t turnsLookedAt, uint32_t turnsLived)
{
	if (turnsLived == 0)
	{
		return 0.0;
	}
	// The lived turns are counted as a signed number when divided
	return static_cast<double>(turnsLookedAt) / static_cast<double>(static_cast<int32_t>(turnsLived));
}

int32_t creature_attention::HowMuch(double share)
{
	constexpr int64_t k_Most = 4;
	const auto level = static_cast<int64_t>(share * 5.0);
	// Counted without a sign, a level below nothing is the most
	return static_cast<int32_t>(level < 0 || level > k_Most ? k_Most : level);
}
