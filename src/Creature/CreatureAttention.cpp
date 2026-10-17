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

#include "Common/X87.h"
#include "Common/X87Vector.h"

using namespace openblack;

bool creature_attention::IsLookingAt(const glm::vec3& origin, const glm::vec3& focus, const glm::vec3& creature)
{
	// The lengths and the dot product add z, then y, then x. Each vector is multiplied by the inverse of its length
	const auto offset = creature - origin;
	const float distance = x87::LengthZyx(offset);
	if (distance == 0.0f || distance >= k_LookingRange)
	{
		return false;
	}
	// A camera looking nowhere has no way it looks, and sees nothing ahead of it
	auto ahead = focus - origin;
	if (ahead != glm::vec3(0.0f))
	{
		ahead *= 1.0f / x87::LengthZyx(ahead);
	}
	// The cosine of the angle stays wide in the compare
	return x87::Greater(x87::DotZyx(offset * (1.0f / distance), ahead), x87::Cos(k_LookingAngle));
}

float creature_attention::Share(uint32_t turnsLookedAt, uint32_t turnsLived)
{
	if (turnsLived == 0)
	{
		return 0.0f;
	}
	// The lived turns are counted as a signed number when divided. The two counts are exact, and the quotient rounds
	// to a float one time
	return x87::Div(static_cast<double>(turnsLookedAt), static_cast<double>(static_cast<int32_t>(turnsLived)));
}

int32_t creature_attention::HowMuch(float share)
{
	constexpr int64_t k_Most = 4;
	const auto level = static_cast<int64_t>(share * 5.0f);
	// Counted without a sign, a level below nothing is the most
	return static_cast<int32_t>(level < 0 || level > k_Most ? k_Most : level);
}
